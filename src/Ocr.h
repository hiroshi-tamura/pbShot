#pragma once
#include <QObject>
#include <QString>
#include <QImage>
#include <memory>

class OcrModelManager;

// OCR 推論ラッパ（pImpl で ONNX Runtime 依存をヘッダから隠蔽）。
class Ocr : public QObject {
    Q_OBJECT
public:
    static Ocr& instance();

    OcrModelManager* models() { return m_models; }
    bool isReady() const;

    // 同期 OCR。models() の DL 完了後に呼ぶ。
    QString recognize(const QImage& img, QString* errorOut = nullptr);

signals:
    // current/total が 0/0 のときは indeterminate 扱い、それ以外は確定進捗。
    // stage はラベルに表示する短い文字列。別スレッドから emit されるので
    // 受信側は QueuedConnection で自動的に UI スレッドに渡る。
    void progress(int current, int total, const QString& stage);

private:
    Ocr();
    ~Ocr() override;
    OcrModelManager* m_models = nullptr;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
