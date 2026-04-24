#pragma once
#include <QObject>
#include <QAbstractNativeEventFilter>

class HotkeyManager : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit HotkeyManager(QObject* parent = nullptr);
    ~HotkeyManager() override;

    bool registerHotkeys();
    void unregisterHotkeys();

    bool nativeEventFilter(const QByteArray& eventType, void* message,
                           qintptr* result) override;

signals:
    void regionRequested();
    void fullscreenRequested();

private:
    int m_idRegionPrt  = 1;  // PrintScreen
    int m_idFullPrt    = 2;  // Shift+PrintScreen
    int m_idRegionCs   = 3;  // Ctrl+Shift+A
    int m_idFullCs     = 4;  // Ctrl+Shift+S
    bool m_registered = false;
};
