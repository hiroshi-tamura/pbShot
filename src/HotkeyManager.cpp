#include "HotkeyManager.h"
#include "Settings.h"
#include <QCoreApplication>
#include <QKeySequence>
#include <QDebug>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __APPLE__
#include <Carbon/Carbon.h>
namespace {
// Carbon のグローバルホットキーは C コールバック方式なので、
// シグナル emit のために HotkeyManager のポインタを保持する必要がある。
HotkeyManager* g_macInstance = nullptr;
EventHotKeyRef g_macRegionRef = nullptr;
EventHotKeyRef g_macFullRef   = nullptr;
EventHandlerRef g_macHandler  = nullptr;

OSStatus macHotkeyHandler(EventHandlerCallRef /*nextHandler*/, EventRef event,
                          void* /*userdata*/) {
    EventHotKeyID hkID;
    GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID, nullptr,
                      sizeof(hkID), nullptr, &hkID);
    if (!g_macInstance) return noErr;
    // signature 'pbSh' = region(1) / fullscreen(2)
    if (hkID.id == 1) {
        QMetaObject::invokeMethod(g_macInstance, "regionRequested", Qt::QueuedConnection);
    } else if (hkID.id == 2) {
        QMetaObject::invokeMethod(g_macInstance, "fullscreenRequested", Qt::QueuedConnection);
    }
    return noErr;
}

bool qtSeqToCarbon(const QKeySequence& seq, UInt32& mods, UInt32& vk) {
    if (seq.isEmpty()) return false;
    QKeyCombination kc = seq[0];
    Qt::KeyboardModifiers km = kc.keyboardModifiers();
    Qt::Key key = kc.key();
    mods = 0;
    if (km & Qt::ControlModifier) mods |= cmdKey;       // mac の "Ctrl" は実用上 Command にマップ
    if (km & Qt::ShiftModifier)   mods |= shiftKey;
    if (km & Qt::AltModifier)     mods |= optionKey;
    if (km & Qt::MetaModifier)    mods |= controlKey;   // Qt::MetaModifier == Mac Control
    vk = UInt32(-1);
    // 主要キーの virtual key code（USB HID ではなく macOS の virtual key）
    switch (key) {
    case Qt::Key_A: vk = kVK_ANSI_A; break;
    case Qt::Key_S: vk = kVK_ANSI_S; break;
    case Qt::Key_D: vk = kVK_ANSI_D; break;
    case Qt::Key_F: vk = kVK_ANSI_F; break;
    case Qt::Key_G: vk = kVK_ANSI_G; break;
    case Qt::Key_H: vk = kVK_ANSI_H; break;
    case Qt::Key_C: vk = kVK_ANSI_C; break;
    case Qt::Key_V: vk = kVK_ANSI_V; break;
    case Qt::Key_X: vk = kVK_ANSI_X; break;
    case Qt::Key_Z: vk = kVK_ANSI_Z; break;
    case Qt::Key_R: vk = kVK_ANSI_R; break;
    case Qt::Key_T: vk = kVK_ANSI_T; break;
    case Qt::Key_Q: vk = kVK_ANSI_Q; break;
    case Qt::Key_W: vk = kVK_ANSI_W; break;
    case Qt::Key_E: vk = kVK_ANSI_E; break;
    case Qt::Key_Y: vk = kVK_ANSI_Y; break;
    case Qt::Key_U: vk = kVK_ANSI_U; break;
    case Qt::Key_I: vk = kVK_ANSI_I; break;
    case Qt::Key_O: vk = kVK_ANSI_O; break;
    case Qt::Key_P: vk = kVK_ANSI_P; break;
    case Qt::Key_Space:  vk = kVK_Space; break;
    case Qt::Key_Return: vk = kVK_Return; break;
    case Qt::Key_F1:  vk = kVK_F1; break;
    case Qt::Key_F2:  vk = kVK_F2; break;
    case Qt::Key_F3:  vk = kVK_F3; break;
    case Qt::Key_F4:  vk = kVK_F4; break;
    case Qt::Key_F5:  vk = kVK_F5; break;
    case Qt::Key_F6:  vk = kVK_F6; break;
    case Qt::Key_F7:  vk = kVK_F7; break;
    case Qt::Key_F8:  vk = kVK_F8; break;
    case Qt::Key_F9:  vk = kVK_F9; break;
    case Qt::Key_F10: vk = kVK_F10; break;
    case Qt::Key_F11: vk = kVK_F11; break;
    case Qt::Key_F12: vk = kVK_F12; break;
    default: return false;
    }
    return true;
}
} // namespace
#endif

HotkeyManager::HotkeyManager(QObject* parent) : QObject(parent) {
#ifdef __APPLE__
    g_macInstance = this;
    EventTypeSpec evt;
    evt.eventClass = kEventClassKeyboard;
    evt.eventKind  = kEventHotKeyPressed;
    InstallApplicationEventHandler(&macHotkeyHandler, 1, &evt, nullptr,
                                   &g_macHandler);
#else
    QCoreApplication::instance()->installNativeEventFilter(this);
#endif
}

HotkeyManager::~HotkeyManager() {
    unregisterHotkeys();
#ifdef __APPLE__
    if (g_macHandler) { RemoveEventHandler(g_macHandler); g_macHandler = nullptr; }
    g_macInstance = nullptr;
#else
    QCoreApplication::instance()->removeNativeEventFilter(this);
#endif
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
#elif defined(__APPLE__)
    if (m_registered) unregisterHotkeys();
    UInt32 mR = 0, vR = 0, mF = 0, vF = 0;
    bool okR = qtSeqToCarbon(Settings::hotkeyRegion(),     mR, vR);
    bool okF = qtSeqToCarbon(Settings::hotkeyFullscreen(), mF, vF);
    bool any = false;
    if (okR) {
        EventHotKeyID id; id.signature = 'pbSh'; id.id = 1;
        if (RegisterEventHotKey(vR, mR, id, GetApplicationEventTarget(),
                                0, &g_macRegionRef) == noErr) any = true;
        else qWarning() << "Carbon RegisterEventHotKey region failed";
    }
    if (okF) {
        EventHotKeyID id; id.signature = 'pbSh'; id.id = 2;
        if (RegisterEventHotKey(vF, mF, id, GetApplicationEventTarget(),
                                0, &g_macFullRef) == noErr) any = true;
        else qWarning() << "Carbon RegisterEventHotKey fullscreen failed";
    }
    m_registered = any;
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
#elif defined(__APPLE__)
    if (g_macRegionRef) { UnregisterEventHotKey(g_macRegionRef); g_macRegionRef = nullptr; }
    if (g_macFullRef)   { UnregisterEventHotKey(g_macFullRef);   g_macFullRef   = nullptr; }
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
