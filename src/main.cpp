#include <QApplication>
#include <QIcon>
#include <QSettings>
#include "TrayApp.h"
#include "Settings.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    // 多重起動防止（ユーザーセッション単位の名前付きミューテックス）
    HANDLE singleInstanceMutex =
        CreateMutexW(nullptr, FALSE, L"Local\\pbShot_SingleInstance_{3f8c-4b2a-8e11}");
    if (!singleInstanceMutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        // 既存のインスタンスを前面に出す
        HWND hwnd = FindWindowW(nullptr, L"pbShot");
        if (hwnd) {
            SetForegroundWindow(hwnd);
        }
        if (singleInstanceMutex) CloseHandle(singleInstanceMutex);
        return 0;
    }
#endif

    QApplication app(argc, argv);
    QApplication::setApplicationName("pbShot");
    QApplication::setOrganizationName("pb");
    QApplication::setQuitOnLastWindowClosed(false);
    app.setWindowIcon(QIcon(":/icons/app.png"));

    // 旧バージョンで HKCU\Software\pb\pbShot に書き込まれていた設定を掃除する（初回のみ）
    {
        QSettings legacy("pb", "pbShot");
        if (!legacy.allKeys().isEmpty()) legacy.clear();
    }

    // exe 隣に pbShot.ini / Screenshots / Cache を用意
    Settings::ensureInitialized();

    TrayApp tray;
    if (!tray.start()) {
#ifdef _WIN32
        if (singleInstanceMutex) CloseHandle(singleInstanceMutex);
#endif
        return 1;
    }
    int ret = app.exec();
#ifdef _WIN32
    if (singleInstanceMutex) CloseHandle(singleInstanceMutex);
#endif
    return ret;
}
