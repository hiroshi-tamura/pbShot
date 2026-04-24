#pragma once
#include <QObject>
#include <QPointer>

class QSystemTrayIcon;
class QMenu;
class HotkeyManager;
class OverlayWindow;

class TrayApp : public QObject {
    Q_OBJECT
public:
    explicit TrayApp(QObject* parent = nullptr);
    ~TrayApp() override;
    bool start();

private slots:
    void onRegion();
    void onFullScreenInstant();
    void onOpenSaveDir();
    void onSettings();
    void onQuit();

private:
    void showOverlay(bool instantFull);

    QSystemTrayIcon* m_tray = nullptr;
    QMenu* m_menu = nullptr;
    HotkeyManager* m_hk = nullptr;
    QPointer<OverlayWindow> m_overlay;
};
