#pragma once
#include <QObject>
#include <QString>
#include <QImage>

// Tesseract OCR (CLI ラッパ)
//
// pbShot は tesseract.exe + tessdata を exe 同梱で配布する想定。
// 検索順:
//   1) Settings::tesseractDir() / tesseract.exe
//   2) Settings::applicationDirPath() / tesseract.exe
//   3) PATH 上の tesseract
class TesseractOcr : public QObject {
    Q_OBJECT
public:
    static TesseractOcr& instance();

    // 同梱 or PATH に tesseract.exe があり、tessdata に jpn か eng の少なくとも
    // どちらかがあれば true。
    bool isReady() const;

    // 解決済みの tesseract.exe パス（無ければ空文字）。
    QString tesseractExePath() const;

    // 解決済みの tessdata ディレクトリ（無ければ空文字）。
    QString tessdataDir() const;

    // 同期 OCR。langs は "jpn+eng" のように Tesseract の -l 形式。
    QString recognize(const QImage& img, const QString& langs,
                      QString* errorOut = nullptr);

signals:
    // Ocr と同じ progress 形状（current=0,total=0 で indeterminate）。
    void progress(int current, int total, const QString& stage);

private:
    TesseractOcr();
};
