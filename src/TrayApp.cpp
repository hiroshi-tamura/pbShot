#include "TrayApp.h"
#include "HotkeyManager.h"
#include "ScreenCapture.h"
#include "OverlayWindow.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QTimer>

TrayApp::TrayApp(QObject* parent) : QObject(parent) {}
TrayApp::~TrayApp() = default;

bool TrayApp::start() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(nullptr, "pbShot",
            "システムトレイが利用できません。");
        return false;
    }
    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(QIcon(":/icons/tray.png"));
    QString hkR = Settings::hotkeyRegion().toString(QKeySequence::NativeText);
    QString hkF = Settings::hotkeyFullscreen().toString(QKeySequence::NativeText);
    m_tray->setToolTip(QString("pbShot — %1 範囲 / %2 全画面").arg(hkR, hkF));

    m_menu = new QMenu();
    auto* aRegion = m_menu->addAction(QString("範囲選択でキャプチャ  (%1)").arg(hkR));
    auto* aFull   = m_menu->addAction(QString("全画面キャプチャ  (%1)").arg(hkF));
    m_menu->addSeparator();
    auto* aOpen = m_menu->addAction("保存フォルダを開く");
    auto* aSettings = m_menu->addAction("設定...");
    m_menu->addSeparator();
    auto* aQuit = m_menu->addAction("終了");

    connect(aRegion,   &QAction::triggered, this, &TrayApp::onRegion);
    connect(aFull,     &QAction::triggered, this, &TrayApp::onFullScreenInstant);
    connect(aOpen,     &QAction::triggered, this, &TrayApp::onOpenSaveDir);
    connect(aSettings, &QAction::triggered, this, &TrayApp::onSettings);
    connect(aQuit,     &QAction::triggered, this, &TrayApp::onQuit);

    m_tray->setContextMenu(m_menu);
    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger) onRegion();        // 左クリック
        else if (r == QSystemTrayIcon::DoubleClick) onRegion();
    });
    m_tray->show();

    m_hk = new HotkeyManager(this);
    connect(m_hk, &HotkeyManager::regionRequested,     this, &TrayApp::onRegion);
    connect(m_hk, &HotkeyManager::fullscreenRequested, this, &TrayApp::onFullScreenInstant);
    m_hk->registerHotkeys();

    m_tray->showMessage("pbShot",
        QString("起動しました。%1 で範囲選択、%2 で全画面保存。").arg(hkR, hkF),
        QSystemTrayIcon::Information, 3500);
    return true;
}

void TrayApp::showOverlay(bool instantFull) {
    if (m_overlay) {
        m_overlay->raise();
        return;
    }
    // キャプチャ中に再びホットキーを拾って多重起動しないよう一時解除
    if (m_hk) m_hk->unregisterHotkeys();

    QRect virtualRect;
    qreal dpr = 1.0;
    QPixmap shot = ScreenCapture::grabAll(virtualRect, dpr);
    if (shot.isNull()) {
        if (m_hk) m_hk->registerHotkeys();
        return;
    }
    auto* w = new OverlayWindow(shot, virtualRect, dpr);
    m_overlay = w;
    connect(w, &OverlayWindow::finished, this, [this]() {
        if (m_hk) {
            // キーアップの取りこぼし防止のため少し待つ
            QTimer::singleShot(200, this, [this]() {
                if (m_hk) m_hk->registerHotkeys();
            });
        }
    });
    w->show();
    if (instantFull) {
        // 全画面即保存
        QTimer::singleShot(0, w, [w]() { w->saveFullScreenAndClose(); });
    }
}

void TrayApp::onRegion() { showOverlay(false); }
void TrayApp::onFullScreenInstant() { showOverlay(true); }

void TrayApp::onOpenSaveDir() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(Settings::saveDir()));
}

void TrayApp::onSettings() {
    // ダイアログ内の QKeySequenceEdit にキー入力を届かせるため、
    // 開いている間はグローバルホットキーを一時解除する。
    if (m_hk) m_hk->unregisterHotkeys();

    SettingsDialog dlg;
    int ret = dlg.exec();

    // 新しい設定（または元の設定）で再登録
    if (m_hk) m_hk->registerHotkeys();

    if (ret == QDialog::Accepted && m_tray) {
        QString hkR = Settings::hotkeyRegion().toString(QKeySequence::NativeText);
        QString hkF = Settings::hotkeyFullscreen().toString(QKeySequence::NativeText);
        m_tray->setToolTip(QString("pbShot — %1 範囲 / %2 全画面").arg(hkR, hkF));
        if (m_menu) {
            const auto actions = m_menu->actions();
            if (actions.size() >= 2) {
                actions[0]->setText(QString("範囲選択でキャプチャ  (%1)").arg(hkR));
                actions[1]->setText(QString("全画面キャプチャ  (%1)").arg(hkF));
            }
        }
    }
}

void TrayApp::onQuit() {
    if (m_hk) m_hk->unregisterHotkeys();
    QApplication::quit();
}
