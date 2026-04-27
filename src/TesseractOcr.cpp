#include "TesseractOcr.h"
#include "Settings.h"
#include "Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QDateTime>

namespace {
inline void tessLog(const QString& msg) {
    Logger::log(QStringLiteral("ocr"), QStringLiteral("[tess] ") + msg);
}
} // namespace

TesseractOcr& TesseractOcr::instance() {
    static TesseractOcr inst;
    return inst;
}

TesseractOcr::TesseractOcr() : QObject(nullptr) {}

QString TesseractOcr::tesseractExePath() const {
#ifdef _WIN32
    const QString exeName = "tesseract.exe";
#else
    const QString exeName = "tesseract";
#endif
    // 1) tesseract/ サブフォルダ（Settings::tesseractDir()）
    QString c1 = Settings::tesseractDir() + "/" + exeName;
    if (QFile::exists(c1)) return c1;
    // 2) exe と同階層
    QString c2 = QCoreApplication::applicationDirPath() + "/" + exeName;
    if (QFile::exists(c2)) return c2;
#ifdef Q_OS_MACOS
    // 3a) .app/Contents/Resources/tesseract/tesseract
    QString c3 = QCoreApplication::applicationDirPath()
                 + "/../Resources/tesseract/" + exeName;
    if (QFile::exists(c3)) return c3;
    // 3b) Homebrew (Apple Silicon / Intel)
    for (const QString& p : { QStringLiteral("/opt/homebrew/bin/tesseract"),
                              QStringLiteral("/usr/local/bin/tesseract") }) {
        if (QFile::exists(p)) return p;
    }
#endif
    // 4) PATH
    QString cP = QStandardPaths::findExecutable("tesseract");
    if (!cP.isEmpty()) return cP;
    return {};
}

QString TesseractOcr::tessdataDir() const {
    auto hasData = [](const QString& d) {
        return QFile::exists(d + "/jpn.traineddata") ||
               QFile::exists(d + "/eng.traineddata");
    };
    // 1) 明示的な tessdata/
    QString d1 = Settings::tessdataDir();
    if (hasData(d1)) return d1;
    // 2) tesseract バンドル内 tessdata/
    QString d2 = Settings::tesseractDir() + "/tessdata";
    if (hasData(d2)) return d2;
#ifdef Q_OS_MACOS
    // 3) .app/Contents/Resources/tessdata/
    QString d3 = QCoreApplication::applicationDirPath() + "/../Resources/tessdata";
    if (hasData(d3)) return d3;
    // 4) Homebrew tessdata
    for (const QString& d : { QStringLiteral("/opt/homebrew/share/tessdata"),
                              QStringLiteral("/usr/local/share/tessdata") }) {
        if (hasData(d)) return d;
    }
#endif
    // システム既定 (PATH 上の tesseract が自身で見つける)
    return {};
}

bool TesseractOcr::isReady() const {
    return !tesseractExePath().isEmpty();
}

QString TesseractOcr::recognize(const QImage& img, const QString& langs,
                                QString* errorOut) {
    emit progress(0, 0, QStringLiteral("Tesseract 起動中"));

    if (img.isNull()) {
        if (errorOut) *errorOut = QStringLiteral("入力画像が空です");
        return {};
    }
    const QString exe = tesseractExePath();
    if (exe.isEmpty()) {
        if (errorOut) *errorOut = QStringLiteral("tesseract.exe が見つかりません");
        tessLog("tesseract not found");
        return {};
    }

    // 入力 PNG を一時ファイルに書く（QProcess に直接 stdin で渡すより堅い）。
    QString tmpDir = QDir::tempPath();
    QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    QString inPath = tmpDir + "/pbShot_tess_" + stamp + ".png";
    if (!img.save(inPath, "PNG")) {
        if (errorOut) *errorOut = QStringLiteral("一時 PNG の書き出しに失敗しました");
        tessLog("PNG save failed: " + inPath);
        return {};
    }
    tessLog(QString("input PNG: %1 (%2x%3)").arg(inPath).arg(img.width()).arg(img.height()));

    // 引数組み立て
    QStringList args;
    args << inPath;
    args << QStringLiteral("stdout");
    args << QStringLiteral("-l") << (langs.isEmpty() ? QStringLiteral("jpn+eng") : langs);

    const QString tdata = tessdataDir();
    if (!tdata.isEmpty()) {
        args << QStringLiteral("--tessdata-dir") << tdata;
    }
    // PSM 6: 単一の均一なテキストブロック前提（一般的なスクショ用途で精度が出やすい）
    args << QStringLiteral("--psm") << QStringLiteral("6");

    emit progress(0, 0, QStringLiteral("文字認識中"));

    QProcess proc;
    proc.setProgram(exe);
    proc.setArguments(args);
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    // バンドル DLL を確実に解決させるため、tesseract.exe のディレクトリを
    // 作業ディレクトリにする（PATH に依存しない）。
    proc.setWorkingDirectory(QFileInfo(exe).absolutePath());

    tessLog(QString("run: %1 %2").arg(exe, args.join(" ")));
    proc.start();
    if (!proc.waitForStarted(5000)) {
        if (errorOut) *errorOut = QStringLiteral("tesseract の起動に失敗しました");
        tessLog("waitForStarted failed");
        QFile::remove(inPath);
        return {};
    }
    if (!proc.waitForFinished(60000)) {
        proc.kill();
        proc.waitForFinished(2000);
        if (errorOut) *errorOut = QStringLiteral("tesseract がタイムアウトしました");
        tessLog("waitForFinished timeout");
        QFile::remove(inPath);
        return {};
    }

    QByteArray out = proc.readAllStandardOutput();
    QByteArray err = proc.readAllStandardError();

    QFile::remove(inPath);

    const int rc = proc.exitCode();
    if (rc != 0) {
        if (errorOut) {
            *errorOut = QString::fromLocal8Bit(err).trimmed();
            if (errorOut->isEmpty()) {
                *errorOut = QStringLiteral("tesseract が終了コード %1 で失敗しました").arg(rc);
            }
        }
        tessLog(QString("rc=%1 err=%2").arg(rc).arg(QString::fromLocal8Bit(err)));
        emit progress(0, 0, QString());
        return {};
    }

    QString text = QString::fromUtf8(out);
    // 末尾の form-feed (FF) と空行を整理
    text.remove(QChar(0x000C));
    while (text.endsWith(QChar('\n')) || text.endsWith(QChar('\r'))) {
        text.chop(1);
    }
    tessLog(QString("ok len=%1").arg(text.size()));
    emit progress(0, 0, QString());
    return text;
}
