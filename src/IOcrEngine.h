#pragma once
#include <QObject>
#include <QString>
#include <QImage>

// OCR エンジンの共通インターフェース。
// Ocr (PP-OCR/ONNX) と TesseractOcr が実装する。
//
// progress シグナルは current=0,total=0 のとき indeterminate 表示。
// recognize() は呼び出しスレッドで同期実行され、UI スレッドから呼ぶ場合は
// QtConcurrent などで別スレッドへ逃がすこと。
class IOcrEngine : public QObject {
    Q_OBJECT
public:
    explicit IOcrEngine(QObject* parent = nullptr) : QObject(parent) {}
    ~IOcrEngine() override = default;

    virtual bool isReady() const = 0;
    virtual QString recognize(const QImage& img, QString* errorOut = nullptr) = 0;
    virtual QString displayName() const = 0;

signals:
    void progress(int current, int total, const QString& stage);
};

// Settings::ocrEngine() の値から対応するエンジンを返す。所有権は singleton 側。
IOcrEngine* currentOcrEngine();
