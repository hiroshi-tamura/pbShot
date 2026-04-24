#include "Settings.h"
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QCoreApplication>

static QSettings& s() {
    // exe と同じフォルダに pbShot.ini を置く（レジストリや他フォルダを汚さない）
    static QSettings st(
        QCoreApplication::applicationDirPath() + "/pbShot.ini",
        QSettings::IniFormat);
    return st;
}

QString Settings::saveDir() {
    QString def = QCoreApplication::applicationDirPath() + "/Screenshots";
    QString d = s().value("saveDir", def).toString();
    QDir().mkpath(d);
    return d;
}

void Settings::setSaveDir(const QString& dir) { s().setValue("saveDir", dir); }

QString Settings::format() { return s().value("format", "png").toString(); }
void Settings::setFormat(const QString& f) { s().setValue("format", f); }

bool Settings::copyToClipboardOnSave() {
    return s().value("copyToClipboardOnSave", true).toBool();
}
void Settings::setCopyToClipboardOnSave(bool b) {
    s().setValue("copyToClipboardOnSave", b);
}

QString Settings::nextFilename() {
    QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    return QString("%1/pbShot_%2.%3").arg(saveDir(), ts, format());
}

QString Settings::cacheDir() {
    QString def = QCoreApplication::applicationDirPath() + "/Cache";
    QString d = s().value("cacheDir", def).toString();
    QDir().mkpath(d);
    return d;
}

// 自動起動キーだけは Windows 仕様上 HKCU\...\Run に書く必要があるため例外。
// それ以外の設定は exe 隣の pbShot.ini に集約している。

void Settings::ensureInitialized() {
    saveDir();   // mkpath
    cacheDir();  // mkpath
    if (!s().contains("configVersion")) {
        s().setValue("configVersion", 1);
        s().sync();
    }
}
void Settings::setCacheDir(const QString& dir) { s().setValue("cacheDir", dir); }
QString Settings::nextCacheFilename() {
    QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    return QString("%1/pbShot_%2.png").arg(cacheDir(), ts);
}

int Settings::cacheMaxSizeMB() { return s().value("cacheMaxSizeMB", 500).toInt(); }
void Settings::setCacheMaxSizeMB(int mb) { s().setValue("cacheMaxSizeMB", mb); }

void Settings::pruneCache() {
    int maxMb = cacheMaxSizeMB();
    if (maxMb <= 0) return;  // 0 = 無制限
    qint64 maxBytes = (qint64)maxMb * 1024 * 1024;
    QDir d(cacheDir());
    // 古い → 新しい順
    QFileInfoList files = d.entryInfoList(
        QStringList() << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp",
        QDir::Files, QDir::Time | QDir::Reversed);
    qint64 total = 0;
    for (const auto& fi : files) total += fi.size();
    int i = 0;
    while (total > maxBytes && i < files.size()) {
        qint64 sz = files[i].size();
        if (QFile::remove(files[i].absoluteFilePath())) total -= sz;
        ++i;
    }
}

int Settings::imageQuality() { return s().value("imageQuality", 90).toInt(); }
void Settings::setImageQuality(int q) { s().setValue("imageQuality", q); }

static const char* kAutostartKey =
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";

bool Settings::autostart() {
    QSettings reg(kAutostartKey, QSettings::NativeFormat);
    return reg.contains("pbShot");
}
void Settings::setAutostart(bool enable) {
    QSettings reg(kAutostartKey, QSettings::NativeFormat);
    if (enable) {
        QString path = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        reg.setValue("pbShot", QString("\"%1\"").arg(path));
    } else {
        reg.remove("pbShot");
    }
    reg.sync();
}

QKeySequence Settings::hotkeyRegion() {
    return QKeySequence(s().value("hotkeyRegion", "Ctrl+Shift+A").toString());
}
void Settings::setHotkeyRegion(const QKeySequence& k) {
    s().setValue("hotkeyRegion", k.toString(QKeySequence::PortableText));
}
QKeySequence Settings::hotkeyFullscreen() {
    return QKeySequence(s().value("hotkeyFullscreen", "Ctrl+Shift+S").toString());
}
void Settings::setHotkeyFullscreen(const QKeySequence& k) {
    s().setValue("hotkeyFullscreen", k.toString(QKeySequence::PortableText));
}
