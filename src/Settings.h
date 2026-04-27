#pragma once
#include <QString>
#include <QKeySequence>

class Settings {
public:
    static QString saveDir();
    static void setSaveDir(const QString& dir);
    static QString format();
    static void setFormat(const QString& fmt);
    static bool copyToClipboardOnSave();
    static void setCopyToClipboardOnSave(bool b);
    static QString nextFilename();

    static QKeySequence hotkeyRegion();
    static void setHotkeyRegion(const QKeySequence& s);
    static QKeySequence hotkeyFullscreen();
    static void setHotkeyFullscreen(const QKeySequence& s);

    static QString cacheDir();
    static void setCacheDir(const QString& dir);
    static QString nextCacheFilename();
    static int  cacheMaxSizeMB();
    static void setCacheMaxSizeMB(int mb);
    static void pruneCache();

    // OCR モデル: exe 隣の models/ フォルダ（ポータブル）
    static QString modelsDir();

    // OCR エンジン選択。"tesseract" (既定) または "ppocr"
    static QString ocrEngine();
    static void    setOcrEngine(const QString& engine);

    // Tesseract 用ポータブルディレクトリ（exe 隣）
    static QString tesseractDir();
    static QString tessdataDir();

    static int  imageQuality();
    static void setImageQuality(int q);

    static bool autostart();
    static void setAutostart(bool enable);

    static void ensureInitialized();
};
