#pragma once
#include <QString>
class QSystemTrayIcon;
class Toast {
public:
    static void setTrayIcon(QSystemTrayIcon* icon);
    static void show(const QString& title, const QString& msg, int durationMs = 2500);
};
