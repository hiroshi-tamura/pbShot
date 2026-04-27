#pragma once
#include <QString>

// シンプルな追記ロガー。出力先は exe 隣の pbShot_<category>.log。
// 例: Logger::log("ocr", "session created") → pbShot_ocr.log
//
// 競合書き込みは想定していない（Qt 単一プロセス内で十分）。
namespace Logger {
    void log(const QString& category, const QString& msg);
}
