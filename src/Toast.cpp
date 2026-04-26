#include "Toast.h"
#include <QSystemTrayIcon>

namespace {
    QSystemTrayIcon* g_trayIcon = nullptr;
}

void Toast::setTrayIcon(QSystemTrayIcon* icon) {
    g_trayIcon = icon;
}

void Toast::show(const QString& title, const QString& msg, int durationMs) {
    if (!g_trayIcon) return;
    g_trayIcon->showMessage(title, msg, QSystemTrayIcon::Information, durationMs);
}
