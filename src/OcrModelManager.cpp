#include "OcrModelManager.h"
#include "Settings.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QProgressDialog>
#include <QApplication>
#include <QThread>

namespace {
// PP-OCRv5 (server) を採用。日本語/中文/英語/混在を 1 モデルでカバー、最新世代。
// det/rec は marsena/paddleocr-onnx-models（HF tree API で 200 OK 確認済）、
// cls は SWHL/RapidOCR の v1（v5/v4/v3 配下に存在しない）、
// dict は PaddleOCR 公式 main ブランチの ppocrv5_dict.txt。
constexpr const char* kHF_MARSENA =
    "https://huggingface.co/marsena/paddleocr-onnx-models/resolve/main/";
constexpr const char* kHF_PPv1 =
    "https://huggingface.co/SWHL/RapidOCR/resolve/main/PP-OCRv1/";
constexpr const char* kGH_PDLOCR_MAIN =
    "https://raw.githubusercontent.com/PaddlePaddle/PaddleOCR/main/ppocr/utils/dict/";
}

OcrModelManager::OcrModelManager(QObject* parent) : QObject(parent) {
    // PP-OCRv4 mobile (det/cls) + 日本語 rec + japan_dict.txt。
    // 各ファイルは fallback URL を持ち、先頭から順に試す。
    m_files = {
        // 検出 (det) - PP-OCRv5 server 約 84MB
        { "det.onnx",
          { QString::fromLatin1(kHF_MARSENA) + "PP-OCRv5_server_det_infer.onnx" },
          50 * 1024 * 1024 },
        // 認識 (rec) - PP-OCRv5 server 約 80MB、日中英+混在 1 モデルで対応
        { "rec.onnx",
          { QString::fromLatin1(kHF_MARSENA) + "PP-OCRv5_server_rec_infer.onnx" },
          50 * 1024 * 1024 },
        // 角度分類 (cls) - 約 0.57MB
        { "cls.onnx",
          { QString::fromLatin1(kHF_PPv1) + "ch_ppocr_mobile_v2.0_cls_infer.onnx" },
          400 * 1024 },
        // PP-OCRv5 用統合 dict (約 74KB)
        { "ppocrv5_dict.txt",
          { QString::fromLatin1(kGH_PDLOCR_MAIN) + "ppocrv5_dict.txt" },
          50 * 1024 },
    };
}

OcrModelManager::~OcrModelManager() {
    if (m_reply) { m_reply->abort(); m_reply->deleteLater(); }
    if (m_currentFile) { m_currentFile->close(); delete m_currentFile; }
}

QString OcrModelManager::fullPath(const ModelFile& m) const {
    return Settings::modelsDir() + "/" + m.relativePath;
}

bool OcrModelManager::fileLooksValid(const ModelFile& m) const {
    QFileInfo fi(fullPath(m));
    return fi.exists() && fi.size() >= m.expectedMin;
}

bool OcrModelManager::isReady() const {
    for (const auto& m : m_files) {
        if (!fileLooksValid(m)) return false;
    }
    return true;
}

QString OcrModelManager::detModelPath() const { return fullPath(m_files[0]); }
QString OcrModelManager::recModelPath() const { return fullPath(m_files[1]); }
QString OcrModelManager::clsModelPath() const { return fullPath(m_files[2]); }
QString OcrModelManager::jpDictPath()   const { return fullPath(m_files[3]); }

void OcrModelManager::removeAll() {
    for (const auto& m : m_files) {
        QFile::remove(fullPath(m));
    }
}

void OcrModelManager::ensureModelsAsync(QWidget* parent) {
    if (isReady()) {
        emit ready();
        return;
    }
    if (!m_nam) m_nam = new QNetworkAccessManager(this);

    if (!m_dlg) {
        m_dlg = new QProgressDialog(parent);
        m_dlg->setWindowTitle("OCR モデルをダウンロード中");
        m_dlg->setLabelText("OCR モデルを取得しています…");
        m_dlg->setRange(0, 100);
        m_dlg->setMinimumDuration(0);
        m_dlg->setAutoClose(false);
        m_dlg->setAutoReset(false);
        m_dlg->setWindowModality(Qt::ApplicationModal);
        connect(m_dlg, &QProgressDialog::canceled, this, [this]{
            cancelAll("ダウンロードがキャンセルされました");
        });
    }
    m_dlg->show();

    m_index = -1;
    startNext();
}

void OcrModelManager::startNext() {
    while (true) {
        ++m_index;
        if (m_index >= m_files.size()) {
            if (m_dlg) { m_dlg->close(); m_dlg->deleteLater(); m_dlg = nullptr; }
            // 大きなファイルの rename 直後は QFileInfo のサイズが
            // 0 を返すことがあるので、最大 5 秒間ポーリングしてから判定する。
            int waited = 0;
            while (!isReady() && waited < 5000) {
                QThread::msleep(100);
                waited += 100;
            }
            if (isReady()) emit ready();
            else {
                QString detail;
                for (const auto& m : m_files) {
                    QFileInfo fi(fullPath(m));
                    detail += QString("\n  %1: exists=%2 size=%3 (need>=%4)")
                              .arg(m.relativePath)
                              .arg(fi.exists() ? "yes" : "no")
                              .arg(fi.size())
                              .arg(m.expectedMin);
                }
                emit failed("ダウンロード後の整合性チェックに失敗:" + detail);
            }
            return;
        }
        if (!fileLooksValid(m_files[m_index])) break;
    }
    m_urlIndex = 0;
    startUrl();
}

void OcrModelManager::startUrl() {
    const auto& m = m_files[m_index];
    if (m_urlIndex >= m.urls.size()) {
        cancelAll(QString("全ての URL でダウンロード失敗: %1").arg(m.relativePath));
        return;
    }
    const QString url = m.urls[m_urlIndex];

    if (m_dlg) {
        m_dlg->setLabelText(QString("ダウンロード中 (%1/%2): %3 [候補 %4/%5]")
                            .arg(m_index + 1)
                            .arg(m_files.size())
                            .arg(m.relativePath)
                            .arg(m_urlIndex + 1)
                            .arg(m.urls.size()));
        m_dlg->setValue(0);
    }

    QString outPath = fullPath(m) + ".part";
    if (m_currentFile) { m_currentFile->close(); delete m_currentFile; }
    m_currentFile = new QFile(outPath);
    if (!m_currentFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        cancelAll(QString("ファイルを開けません: %1").arg(outPath));
        return;
    }

    QNetworkRequest req((QUrl(url)));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader, "pbShot-OCR/0.1");

    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::downloadProgress,
            this, &OcrModelManager::onProgress);
    connect(m_reply, &QNetworkReply::readyRead, this, [this]{
        if (m_currentFile && m_reply) m_currentFile->write(m_reply->readAll());
    });
    connect(m_reply, &QNetworkReply::finished,
            this, &OcrModelManager::onFinished);
}

void OcrModelManager::onProgress(qint64 received, qint64 total) {
    if (!m_dlg) return;
    int pct = (total > 0) ? int((received * 100) / total) : 0;
    m_dlg->setValue(pct);
    QApplication::processEvents();
}

void OcrModelManager::onFinished() {
    if (!m_reply) return;
    auto err = m_reply->error();
    int httpStatus = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray remaining = m_reply->readAll();
    if (m_currentFile && !remaining.isEmpty()) m_currentFile->write(remaining);
    if (m_currentFile) { m_currentFile->flush(); m_currentFile->close(); }

    QString partPath = m_currentFile ? m_currentFile->fileName() : QString();
    delete m_currentFile;
    m_currentFile = nullptr;

    QNetworkReply* r = m_reply;
    m_reply = nullptr;
    QString errMsg = r->errorString();
    r->deleteLater();

    auto tryNextUrl = [this, &partPath]() {
        QFile::remove(partPath);
        ++m_urlIndex;
        startUrl();
    };

    // HTTP 200 以外（HuggingFace の "Entry not found" 等）は失敗扱い。
    // QNetworkReply は 404 でも error() が NoError のことがあるので明示的に弾く。
    bool httpOk = (httpStatus >= 200 && httpStatus < 300) || httpStatus == 0;
    if (err != QNetworkReply::NoError || !httpOk) {
        if (m_urlIndex + 1 < m_files[m_index].urls.size()) {
            tryNextUrl();
            return;
        }
        cancelAll(QString("ダウンロード失敗 (%1) HTTP=%2: %3")
                  .arg(m_files[m_index].relativePath)
                  .arg(httpStatus)
                  .arg(errMsg));
        return;
    }

    QString finalPath = fullPath(m_files[m_index]);
    QFile::remove(finalPath);
    if (!QFile::rename(partPath, finalPath)) {
        cancelAll(QString("リネーム失敗: %1 -> %2").arg(partPath, finalPath));
        return;
    }

    if (!fileLooksValid(m_files[m_index])) {
        // サイズが期待値未満 → 次の候補 URL を試す
        QFile::remove(finalPath);
        if (m_urlIndex + 1 < m_files[m_index].urls.size()) {
            ++m_urlIndex;
            startUrl();
            return;
        }
        cancelAll(QString("ファイル検証失敗: %1").arg(m_files[m_index].relativePath));
        return;
    }

    startNext();
}

void OcrModelManager::cancelAll(const QString& msg) {
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_currentFile) {
        m_currentFile->close();
        QFile::remove(m_currentFile->fileName());
        delete m_currentFile;
        m_currentFile = nullptr;
    }
    if (m_dlg) {
        m_dlg->close();
        m_dlg->deleteLater();
        m_dlg = nullptr;
    }
    emit failed(msg);
}
