#include "HotkeyManager.h"
#include "Settings.h"
#include <QCoreApplication>
#include <QKeySequence>
#include <QDebug>

#ifdef _WIN32
#include <windows.h>
#endif

HotkeyManager::HotkeyManager(QObject* parent) : QObject(parent) {
    QCoreApplication::instance()->installNativeEventFilter(this);
}

HotkeyManager::~HotkeyManager() {
    unregisterHotkeys();
    QCoreApplication::instance()->removeNativeEventFilter(this);
}

#ifdef _WIN32
static bool qtSeqToWin32(const QKeySequence& seq, UINT& mods, UINT& vk) {
    if (seq.isEmpty()) return false;
    QKeyCombination kc = seq[0];
    Qt::KeyboardModifiers km = kc.keyboardModifiers();
    Qt::Key key = kc.key();
    mods = MOD_NOREPEAT;
    if (km & Qt::ControlModifier) mods |= MOD_CONTROL;
    if (km & Qt::ShiftModifier)   mods |= MOD_SHIFT;
    if (km & Qt::AltModifier)     mods |= MOD_ALT;
    if (km & Qt::MetaModifier)    mods |= MOD_WIN;
    vk = 0;
    if (key >= Qt::Key_A && key <= Qt::Key_Z)   vk = 'A' + (key - Qt::Key_A);
    else if (key >= Qt::Key_0 && key <= Qt::Key_9) vk = '0' + (key - Qt::Key_0);
    else if (key >= Qt::Key_F1 && key <= Qt::Key_F24) vk = VK_F1 + (key - Qt::Key_F1);
    else if (key == Qt::Key_Print) vk = VK_SNAPSHOT;
    else if (key == Qt::Key_Insert) vk = VK_INSERT;
    else if (key == Qt::Key_Delete) vk = VK_DELETE;
    else if (key == Qt::Key_Space)  vk = VK_SPACE;
    else if (key == Qt::Key_Tab)    vk = VK_TAB;
    else if (key == Qt::Key_Escape) vk = VK_ESCAPE;
    else if (key == Qt::Key_Return || key == Qt::Key_Enter) vk = VK_RETURN;
    return vk != 0;
}
#endif

bool HotkeyManager::registerHotkeys() {
#ifdef _WIN32
    if (m_registered) unregisterHotkeys();
    UINT mR = 0, vR = 0, mF = 0, vF = 0;
    bool okR = qtSeqToWin32(Settings::hotkeyRegion(),     mR, vR);
    bool okF = qtSeqToWin32(Settings::hotkeyFullscreen(), mF, vF);
    BOOL r1 = FALSE, r2 = FALSE, r3 = FALSE, r4 = FALSE;
    if (okR) r1 = RegisterHotKey(nullptr, m_idRegionCs, mR, vR);
    if (okF) r2 = RegisterHotKey(nullptr, m_idFullCs,   mF, vF);
    // 副次: PrintScreen系（Win11 Snipping Tool が無効なら登録可）
    r3 = RegisterHotKey(nullptr, m_idRegionPrt, MOD_NOREPEAT, VK_SNAPSHOT);
    r4 = RegisterHotKey(nullptr, m_idFullPrt,   MOD_SHIFT | MOD_NOREPEAT, VK_SNAPSHOT);
    m_registered = (r1 || r2 || r3 || r4);
    if (okR && !r1) qWarning() << "RegisterHotKey region failed";
    if (okF && !r2) qWarning() << "RegisterHotKey fullscreen failed";
    return m_registered;
#else
    return false;
#endif
}

void HotkeyManager::unregisterHotkeys() {
#ifdef _WIN32
    if (!m_registered) return;
    UnregisterHotKey(nullptr, m_idRegionPrt);
    UnregisterHotKey(nullptr, m_idFullPrt);
    UnregisterHotKey(nullptr, m_idRegionCs);
    UnregisterHotKey(nullptr, m_idFullCs);
    m_registered = false;
#endif
}

bool HotkeyManager::nativeEventFilter(const QByteArray& eventType, void* message,
                                       qintptr* /*result*/) {
#ifdef _WIN32
    if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG")
        return false;
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY) {
        int id = (int)msg->wParam;
        if (id == m_idRegionPrt || id == m_idRegionCs) {
            emit regionRequested();
            return true;
        } else if (id == m_idFullPrt || id == m_idFullCs) {
            emit fullscreenRequested();
            return true;
        }
    }
#else
    Q_UNUSED(eventType); Q_UNUSED(message);
#endif
    return false;
}
