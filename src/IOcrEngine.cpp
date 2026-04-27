#include "IOcrEngine.h"
#include "Settings.h"
#include "Ocr.h"
#include "TesseractOcr.h"

namespace {

class PPOcrAdapter : public IOcrEngine {
public:
    PPOcrAdapter() : IOcrEngine(nullptr) {
        connect(&Ocr::instance(), &Ocr::progress,
                this, &IOcrEngine::progress);
    }
    bool isReady() const override { return Ocr::instance().isReady(); }
    QString displayName() const override { return QStringLiteral("PP-OCR"); }
    QString recognize(const QImage& img, QString* errorOut) override {
        return Ocr::instance().recognize(img, errorOut);
    }
};

class TesseractAdapter : public IOcrEngine {
public:
    TesseractAdapter() : IOcrEngine(nullptr) {
        connect(&TesseractOcr::instance(), &TesseractOcr::progress,
                this, &IOcrEngine::progress);
    }
    bool isReady() const override { return TesseractOcr::instance().isReady(); }
    QString displayName() const override { return QStringLiteral("Tesseract"); }
    QString recognize(const QImage& img, QString* errorOut) override {
        return TesseractOcr::instance().recognize(img, QStringLiteral("jpn+eng"), errorOut);
    }
};

} // namespace

IOcrEngine* currentOcrEngine() {
    static PPOcrAdapter ppocr;
    static TesseractAdapter tess;
    return Settings::ocrEngine() == QStringLiteral("tesseract")
           ? static_cast<IOcrEngine*>(&tess)
           : static_cast<IOcrEngine*>(&ppocr);
}
