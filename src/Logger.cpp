#include "Logger.h"
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QDateTime>
#include <QStringConverter>
#include <QCoreApplication>

void Logger::log(const QString& category, const QString& msg) {
    const QString path = QCoreApplication::applicationDirPath()
                         + QStringLiteral("/pbShot_") + category
                         + QStringLiteral(".log");
    QFile f(path);
    if (!f.open(QIODevice::Append | QIODevice::Text)) return;
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    ts << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")
       << "  " << msg << '\n';
}
