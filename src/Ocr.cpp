#include "Ocr.h"
#include "OcrModelManager.h"
#include "Settings.h"

#include <QFile>
#include <QTextStream>
#include <QImage>
#include <QRect>
#include <QStringList>
#include <QDebug>
#include <QStringConverter>
#include <QCoreApplication>
#include <QDateTime>

namespace {
void ocrLog(const QString& msg) {
    QFile f(QCoreApplication::applicationDirPath() + "/pbShot_ocr.log");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts.setEncoding(QStringConverter::Utf8);
        ts << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")
           << "  " << msg << "\n";
    }
}
}

#include <vector>
#include <algorithm>
#include <cmath>
#include <utility>

#ifdef ORT_AVAILABLE
// ---- MinGW + ONNX Runtime: SAL アノテーション shim ----
// ONNX Runtime の onnxruntime_c_api.h は SAL (Microsoft 注釈) を多用する。
// MinGW-w64 の sal.h は不完全なため、未定義のマクロを空にして無害化する。
#if defined(__MINGW32__) || defined(__MINGW64__)
#  define _Frees_ptr_opt_
#  define _In_
#  define _In_opt_
#  define _In_z_
#  define _In_opt_z_
#  define _Out_
#  define _Out_opt_
#  define _Outptr_
#  define _Outptr_opt_
#  define _Inout_
#  define _Inout_opt_
#  define _Ret_maybenull_
#  define _Ret_notnull_
#  define _Outptr_result_maybenull_
#  define _Outptr_opt_result_maybenull_
#  define _Outptr_result_z_
#  define _In_reads_(x)
#  define _In_reads_bytes_(x)
#  define _Out_writes_(x)
#  define _Out_writes_all_(x)
#  define _Out_writes_to_(x, y)
#  define _Out_writes_bytes_all_(x)
#  define _Inout_updates_all_(x)
#  define _Outptr_result_buffer_(x)
#  define _Outptr_result_buffer_all_maybenull_(x)
#  define _Outptr_result_buffer_maybenull_(x)
#  define _Outptr_result_buffer_to_maybenull_(x, y)
#  define _Return_type_success_(x)
#  define _Success_(x)
#  define _Check_return_
#  define _Must_inspect_result_
#  ifndef _COM_Outptr_
#    define _COM_Outptr_
#  endif
#endif
#include <onnxruntime_cxx_api.h>
#endif

// =============================================================================
// PP-OCRv4 推論パイプライン
//
// det (DBNet)   : 入力 [1,3,H,W] (ImageNet 正規化) → 確率マップ [1,1,H,W]
//                  二値化 → 連結成分 → 矩形抽出 → unclip 近似拡張
// rec (CRNN)    : 入力 [1,3,48,W'] (x/255-0.5)/0.5 → logits [1,T,C]
//                  CTC デコードで文字列化（dict は japan_dict.txt + space）
// =============================================================================

namespace {

#ifdef ORT_AVAILABLE

// シンプルな 4 近傍連結成分 → axis-aligned bbox。
// in は Format_Grayscale8 (0/255 の 2 値画像) を期待。
std::vector<QRect> findRects(const QImage& bin, int minSide = 4) {
    const int W = bin.width();
    const int H = bin.height();
    std::vector<uint8_t> visited(size_t(W) * size_t(H), 0);
    std::vector<QRect> rects;
    rects.reserve(64);

    auto pixOn = [&](int x, int y) -> bool {
        return *(bin.constScanLine(y) + x) > 127;
    };

    std::vector<std::pair<int, int>> stack;
    stack.reserve(256);

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const size_t idx = size_t(y) * size_t(W) + size_t(x);
            if (visited[idx]) continue;
            if (!pixOn(x, y)) { visited[idx] = 1; continue; }

            // BFS
            stack.clear();
            stack.push_back({x, y});
            visited[idx] = 1;
            int xmin = x, xmax = x, ymin = y, ymax = y;
            while (!stack.empty()) {
                auto [cx, cy] = stack.back();
                stack.pop_back();
                if (cx < xmin) xmin = cx;
                if (cx > xmax) xmax = cx;
                if (cy < ymin) ymin = cy;
                if (cy > ymax) ymax = cy;
                static const int DX[4] = {-1, 1,  0, 0};
                static const int DY[4] = { 0, 0, -1, 1};
                for (int k = 0; k < 4; ++k) {
                    const int nx = cx + DX[k];
                    const int ny = cy + DY[k];
                    if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                    const size_t nidx = size_t(ny) * size_t(W) + size_t(nx);
                    if (visited[nidx]) continue;
                    visited[nidx] = 1;
                    if (!pixOn(nx, ny)) continue;
                    stack.push_back({nx, ny});
                }
            }
            const int rw = xmax - xmin + 1;
            const int rh = ymax - ymin + 1;
            if (rw >= minSide && rh >= minSide) {
                rects.emplace_back(xmin, ymin, rw, rh);
            }
        }
    }
    return rects;
}

// CTC グリーディーデコード (PP-OCR 規約: blank = 0, dict[idx-1] が文字)。
QString ctcDecode(const float* logits, int T, int C, const QStringList& dict) {
    QString out;
    out.reserve(T);
    int prev = -1;
    for (int t = 0; t < T; ++t) {
        const float* row = logits + size_t(t) * size_t(C);
        int best = 0;
        float bestVal = row[0];
        for (int c = 1; c < C; ++c) {
            if (row[c] > bestVal) { bestVal = row[c]; best = c; }
        }
        if (best != prev && best != 0) {
            const int idx = best - 1;
            if (idx >= 0 && idx < dict.size()) {
                out += dict[idx];
            }
        }
        prev = best;
    }
    return out;
}

#endif // ORT_AVAILABLE

} // namespace

// =============================================================================
// Impl
// =============================================================================

#ifdef ORT_AVAILABLE
struct Ocr::Impl {
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "pbShot"};
    Ort::SessionOptions opts;
    std::unique_ptr<Ort::Session> det;
    std::unique_ptr<Ort::Session> rec;
    QStringList dict;
    bool initialized = false;
    QString lastError;
    // rec モデルの入力高さ。PP-OCRv1=32, PP-OCRv3/v4=48。モデルから動的取得。
    int recInputH = 32;

    Impl() {
        opts.SetIntraOpNumThreads(2);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    }

    bool ensureLoaded(OcrModelManager* mm) {
        if (initialized) return true;
        ocrLog(QString("ensureLoaded start det=%1 rec=%2 dict=%3")
               .arg(mm->detModelPath(), mm->recModelPath(), mm->jpDictPath()));
        try {
#ifdef _WIN32
            auto detW = mm->detModelPath().toStdWString();
            auto recW = mm->recModelPath().toStdWString();
            ocrLog("creating det Session...");
            det = std::make_unique<Ort::Session>(env, detW.c_str(), opts);
            ocrLog("det Session OK, creating rec Session...");
            rec = std::make_unique<Ort::Session>(env, recW.c_str(), opts);
            ocrLog("rec Session OK");
            // rec モデルの入力 shape [N,C,H,W] から H を取得（固定値）
            try {
                auto info = rec->GetInputTypeInfo(0);
                auto shape = info.GetTensorTypeAndShapeInfo().GetShape();
                if (shape.size() >= 4 && shape[2] > 0) {
                    recInputH = int(shape[2]);
                }
                ocrLog(QString("rec input H = %1").arg(recInputH));
            } catch (...) {
                ocrLog("rec input shape probe failed, using default H=32");
            }
#else
            auto detS = mm->detModelPath().toStdString();
            auto recS = mm->recModelPath().toStdString();
            det = std::make_unique<Ort::Session>(env, detS.c_str(), opts);
            rec = std::make_unique<Ort::Session>(env, recS.c_str(), opts);
#endif

            QFile f(mm->jpDictPath());
            if (!f.open(QIODevice::ReadOnly)) {
                lastError = QString("dict open failed: %1").arg(mm->jpDictPath());
                ocrLog("ERR " + lastError);
                return false;
            }
            QTextStream ts(&f);
            ts.setEncoding(QStringConverter::Utf8);
            dict.clear();
            while (!ts.atEnd()) {
                QString line = ts.readLine();
                if (!line.isEmpty()) dict.append(line);
            }
            // PP-OCR 規約: 末尾に半角スペース
            dict.append(QStringLiteral(" "));
            ocrLog(QString("dict loaded: %1 entries").arg(dict.size()));

            initialized = true;
            return true;
        } catch (const Ort::Exception& e) {
            lastError = QString("ORT init: %1").arg(e.what());
            ocrLog("ORT EXC " + lastError);
            return false;
        } catch (const std::exception& e) {
            lastError = QString("std::exception: %1").arg(e.what());
            ocrLog("STD EXC " + lastError);
            return false;
        }
    }

    using ProgressCb = std::function<void(int, int, const QString&)>;
    QString recognizeImpl(const QImage& srcIn, const ProgressCb& cb);

    std::vector<QRect> runDet(const QImage& detInput);
    QString runRec(const QImage& region);
};
#else
struct Ocr::Impl {
    using ProgressCb = std::function<void(int, int, const QString&)>;
    bool ensureLoaded(OcrModelManager*) { return false; }
    QString recognizeImpl(const QImage&, const ProgressCb&) { return {}; }
};
#endif

// =============================================================================
// Ocr (公開 API)
// =============================================================================

Ocr& Ocr::instance() {
    static Ocr inst;
    return inst;
}

Ocr::Ocr() : QObject(nullptr) {
    m_models = new OcrModelManager(this);
    m_impl = std::make_unique<Impl>();
}

Ocr::~Ocr() = default;

bool Ocr::isReady() const {
    return m_models && m_models->isReady();
}

QString Ocr::recognize(const QImage& img, QString* errorOut) {
    ocrLog(QString("recognize() called img=%1x%2 isReady=%3")
           .arg(img.width()).arg(img.height()).arg(isReady()));
    if (!isReady()) {
        if (errorOut) *errorOut = QStringLiteral("OCR モデルが未初期化です");
        ocrLog("not ready -> abort");
        return {};
    }
    if (img.isNull()) {
        if (errorOut) *errorOut = QStringLiteral("入力画像が空です");
        return {};
    }

#ifdef ORT_AVAILABLE
    emit progress(0, 0, QStringLiteral("OCR エンジン準備中"));
    if (!m_impl->ensureLoaded(m_models)) {
        if (errorOut) *errorOut = m_impl->lastError.isEmpty()
            ? QStringLiteral("OCR エンジンの初期化に失敗しました")
            : m_impl->lastError;
        ocrLog("ensureLoaded failed -> " + (errorOut ? *errorOut : QString()));
        return {};
    }
    try {
        auto cb = [this](int c, int t, const QString& s) {
            emit progress(c, t, s);
        };
        QString result = m_impl->recognizeImpl(img, cb);
        ocrLog(QString("recognizeImpl done, result.length=%1").arg(result.length()));
        emit progress(0, 0, QString());
        if (result.isEmpty() && errorOut) {
            *errorOut = QStringLiteral("テキストが検出できませんでした");
        }
        return result;
    } catch (const Ort::Exception& e) {
        if (errorOut) *errorOut = QString::fromUtf8(e.what());
        ocrLog(QString("ORT exc in recognizeImpl: %1").arg(e.what()));
        return {};
    } catch (const std::exception& e) {
        if (errorOut) *errorOut = QString::fromUtf8(e.what());
        ocrLog(QString("std exc in recognizeImpl: %1").arg(e.what()));
        return {};
    }
#else
    if (errorOut) {
        *errorOut = QStringLiteral(
            "OCR 推論エンジン (ONNX Runtime) がビルドに含まれていません");
    }
    return {};
#endif
}

// =============================================================================
// 推論実装 (ORT_AVAILABLE のみ)
// =============================================================================
#ifdef ORT_AVAILABLE

std::vector<QRect> Ocr::Impl::runDet(const QImage& detInput) {
    const int H = detInput.height();
    const int W = detInput.width();

    static const float kMean[3] = {0.485f, 0.456f, 0.406f};
    static const float kStd [3] = {0.229f, 0.224f, 0.225f};

    // PP-OCR は cv2.imread (BGR) で訓練。QImage RGB888 を BGR に並べ替える。
    std::vector<float> data(size_t(3) * size_t(H) * size_t(W));
    for (int y = 0; y < H; ++y) {
        const uchar* row = detInput.constScanLine(y);
        for (int x = 0; x < W; ++x) {
            const uchar* px = row + size_t(x) * 3u;  // [R,G,B]
            for (int c = 0; c < 3; ++c) {
                const float v = (px[2 - c] / 255.0f - kMean[c]) / kStd[c];
                data[size_t(c) * size_t(H) * size_t(W)
                     + size_t(y) * size_t(W) + size_t(x)] = v;
            }
        }
    }
    std::vector<int64_t> shape = {1, 3, H, W};

    auto mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    auto tensor = Ort::Value::CreateTensor<float>(
        mem, data.data(), data.size(), shape.data(), shape.size());

    Ort::AllocatorWithDefaultOptions alloc;
    auto inName  = det->GetInputNameAllocated(0, alloc);
    auto outName = det->GetOutputNameAllocated(0, alloc);
    const char* inNames[]  = { inName.get() };
    const char* outNames[] = { outName.get() };

    auto outputs = det->Run(
        Ort::RunOptions{nullptr}, inNames, &tensor, 1, outNames, 1);

    const auto info = outputs[0].GetTensorTypeAndShapeInfo();
    const auto outShape = info.GetShape();
    if (outShape.size() < 4) return {};
    const int oH = int(outShape[outShape.size() - 2]);
    const int oW = int(outShape[outShape.size() - 1]);
    const float* outData = outputs[0].GetTensorData<float>();

    QImage bin(oW, oH, QImage::Format_Grayscale8);
    bin.fill(0);
    for (int y = 0; y < oH; ++y) {
        uchar* row = bin.scanLine(y);
        for (int x = 0; x < oW; ++x) {
            row[x] = (outData[size_t(y) * size_t(oW) + size_t(x)] > 0.3f) ? 255 : 0;
        }
    }

    auto rects = findRects(bin);
    const qreal sx = qreal(W) / qreal(oW);
    const qreal sy = qreal(H) / qreal(oH);
    for (auto& r : rects) {
        r = QRect(int(r.x() * sx), int(r.y() * sy),
                  int(r.width() * sx), int(r.height() * sy));
    }
    return rects;
}

QString Ocr::Impl::runRec(const QImage& region) {
    if (region.width() < 2 || region.height() < 2) return {};
    const int targetH = recInputH;
    int targetW = std::max(16, int(qreal(region.width()) * targetH / region.height()));
    targetW = ((targetW + 7) / 8) * 8;
    if (targetW < 16) targetW = 16;
    if (targetW > 800) targetW = 800;

    QImage scaled = region.convertToFormat(QImage::Format_RGB888)
                          .scaled(targetW, targetH,
                                  Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    // PP-OCR は BGR 入力。QImage RGB888 を BGR に並べ替える。
    std::vector<float> data(size_t(3) * size_t(targetH) * size_t(targetW));
    for (int y = 0; y < targetH; ++y) {
        const uchar* row = scaled.constScanLine(y);
        for (int x = 0; x < targetW; ++x) {
            const uchar* px = row + size_t(x) * 3u;  // [R,G,B]
            for (int c = 0; c < 3; ++c) {
                const float v = (px[2 - c] / 255.0f - 0.5f) / 0.5f;
                data[size_t(c) * size_t(targetH) * size_t(targetW)
                     + size_t(y) * size_t(targetW) + size_t(x)] = v;
            }
        }
    }
    std::vector<int64_t> shape = {1, 3, targetH, targetW};
    auto mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    auto tensor = Ort::Value::CreateTensor<float>(
        mem, data.data(), data.size(), shape.data(), shape.size());

    Ort::AllocatorWithDefaultOptions alloc;
    auto inName  = rec->GetInputNameAllocated(0, alloc);
    auto outName = rec->GetOutputNameAllocated(0, alloc);
    const char* inNames[]  = { inName.get() };
    const char* outNames[] = { outName.get() };

    auto outputs = rec->Run(
        Ort::RunOptions{nullptr}, inNames, &tensor, 1, outNames, 1);

    const auto info = outputs[0].GetTensorTypeAndShapeInfo();
    const auto outShape = info.GetShape();
    if (outShape.size() < 3) return {};
    const int T = int(outShape[outShape.size() - 2]);
    const int C = int(outShape[outShape.size() - 1]);
    const float* outData = outputs[0].GetTensorData<float>();

    return ctcDecode(outData, T, C, dict);
}

QString Ocr::Impl::recognizeImpl(const QImage& srcIn, const ProgressCb& cb) {
    QImage src = srcIn.convertToFormat(QImage::Format_RGB888);
    const int oW = src.width();
    const int oH = src.height();
    if (oW < 8 || oH < 8) return {};

    // 入力リサイズ: 32 の倍数、min 320 / max 1280。短辺基準でアップスケール許容。
    const int shortSide = 736;
    const qreal ratio = qreal(shortSide) / qreal(std::min(oW, oH));
    auto roundTo32 = [](int v) {
        v = std::max(32, ((v + 16) / 32) * 32);
        return v;
    };
    int dW = std::min(1280, roundTo32(int(std::round(oW * ratio))));
    int dH = std::min(1280, roundTo32(int(std::round(oH * ratio))));

    QImage detInput = src.scaled(dW, dH, Qt::IgnoreAspectRatio,
                                 Qt::SmoothTransformation);

    if (cb) cb(0, 0, QStringLiteral("テキスト領域を検出中"));
    std::vector<QRect> rects = runDet(detInput);
    if (rects.empty()) return {};

    // 元サイズへスケールバック + unclip 近似拡張 (PP-OCR の axis-aligned 簡易版)
    const qreal sx = qreal(oW) / qreal(dW);
    const qreal sy = qreal(oH) / qreal(dH);
    std::vector<QRect> mapped;
    mapped.reserve(rects.size());
    for (const auto& r : rects) {
        int x = int(std::floor(r.x()      * sx));
        int y = int(std::floor(r.y()      * sy));
        int w = int(std::ceil (r.width()  * sx));
        int h = int(std::ceil (r.height() * sy));
        const int padX = std::max(2, int(w * 0.10));
        const int padY = std::max(2, int(h * 0.30));
        x = std::max(0, x - padX);
        y = std::max(0, y - padY);
        w = std::min(oW - x, w + padX * 2);
        h = std::min(oH - y, h + padY * 2);
        if (w >= 6 && h >= 6) mapped.emplace_back(x, y, w, h);
    }
    if (mapped.empty()) return {};

    // 読み順ソート: y 軸でグルーピング → 同行内は x 昇順
    std::sort(mapped.begin(), mapped.end(),
              [](const QRect& a, const QRect& b) {
                  const int dy = a.y() - b.y();
                  const int tol = std::min(a.height(), b.height()) / 2;
                  if (std::abs(dy) > tol) return a.y() < b.y();
                  return a.x() < b.x();
              });

    QStringList lines;
    QStringList currentLine;
    int lineY = -100000;
    int lineH = 0;

    const int N = int(mapped.size());
    if (cb) cb(0, N, QStringLiteral("文字を認識中"));
    for (int i = 0; i < N; ++i) {
        const QRect& r = mapped[i];
        QImage region = src.copy(r);
        QString text = runRec(region);
        if (cb) cb(i + 1, N, QStringLiteral("文字を認識中"));
        if (text.isEmpty()) continue;

        if (lineY < -10000 ||
            std::abs(r.y() - lineY) > std::max(8, lineH / 2)) {
            if (!currentLine.isEmpty()) {
                lines << currentLine.join(QStringLiteral(" "));
                currentLine.clear();
            }
            lineY = r.y();
            lineH = r.height();
        }
        currentLine << text;
    }
    if (!currentLine.isEmpty()) {
        lines << currentLine.join(QStringLiteral(" "));
    }
    return lines.join(QStringLiteral("\n"));
}

#endif // ORT_AVAILABLE
