#include "SettingsDialog.h"
#include "Settings.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QKeySequenceEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QLabel>
#include <QIcon>

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("pbShot 設定");
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setMinimumWidth(440);

    auto* root = new QVBoxLayout(this);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // 保存フォルダ
    auto* dirRow = new QHBoxLayout();
    m_saveDir = new QLineEdit(Settings::saveDir(), this);
    auto* browseBtn = new QPushButton("参照...", this);
    connect(browseBtn, &QPushButton::clicked, this, &SettingsDialog::browseSaveDir);
    dirRow->addWidget(m_saveDir, 1);
    dirRow->addWidget(browseBtn);
    form->addRow("保存フォルダ:", dirRow);

    // キャッシュフォルダ
    auto* cacheRow = new QHBoxLayout();
    m_cacheDir = new QLineEdit(Settings::cacheDir(), this);
    auto* cacheBtn = new QPushButton("参照...", this);
    connect(cacheBtn, &QPushButton::clicked, this, &SettingsDialog::browseCacheDir);
    cacheRow->addWidget(m_cacheDir, 1);
    cacheRow->addWidget(cacheBtn);
    form->addRow("キャッシュフォルダ:", cacheRow);

    // フォーマット
    m_format = new QComboBox(this);
    m_format->addItem("PNG", "png");
    m_format->addItem("JPEG", "jpg");
    m_format->addItem("BMP",  "bmp");
    {
        int idx = m_format->findData(Settings::format());
        if (idx < 0) idx = 0;
        m_format->setCurrentIndex(idx);
    }
    form->addRow("既定のフォーマット:", m_format);

    // 画質
    m_quality = new QSpinBox(this);
    m_quality->setRange(1, 100);
    m_quality->setSuffix("  (JPEGなど非可逆時の画質)");
    m_quality->setValue(Settings::imageQuality());
    form->addRow("画質:", m_quality);

    // キャッシュ最大サイズ
    m_cacheMaxMB = new QSpinBox(this);
    m_cacheMaxMB->setRange(0, 100000);
    m_cacheMaxMB->setSuffix(" MB   (0=無制限 / 超過時は古いものから削除)");
    m_cacheMaxMB->setValue(Settings::cacheMaxSizeMB());
    form->addRow("キャッシュ上限:", m_cacheMaxMB);

    // ホットキー
    m_hkRegion = new QKeySequenceEdit(Settings::hotkeyRegion(), this);
    m_hkRegion->setMaximumSequenceLength(1);
    form->addRow("範囲選択ホットキー:", m_hkRegion);

    m_hkFull = new QKeySequenceEdit(Settings::hotkeyFullscreen(), this);
    m_hkFull->setMaximumSequenceLength(1);
    form->addRow("全画面ホットキー:", m_hkFull);

    // クリップボード
    m_clipboardOnSave = new QCheckBox("保存時にクリップボードへもコピー", this);
    m_clipboardOnSave->setChecked(Settings::copyToClipboardOnSave());
    form->addRow("", m_clipboardOnSave);

    // 自動起動
    m_autostart = new QCheckBox("Windows 起動時に自動で起動する", this);
    m_autostart->setChecked(Settings::autostart());
    form->addRow("", m_autostart);

    root->addLayout(form);

    auto* note = new QLabel(
        "※ ホットキーが他アプリや Windows の機能と衝突している場合は登録に失敗します。\n"
        "※ Print Screen は Win11 の切り取りツールに奪われやすいので、Ctrl+Shift+ 等を推奨。",
        this);
    note->setWordWrap(true);
    note->setStyleSheet("color:#888;");
    root->addWidget(note);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

void SettingsDialog::browseSaveDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "保存フォルダを選択", m_saveDir->text());
    if (!dir.isEmpty()) m_saveDir->setText(dir);
}

void SettingsDialog::browseCacheDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "キャッシュフォルダを選択", m_cacheDir->text());
    if (!dir.isEmpty()) m_cacheDir->setText(dir);
}

void SettingsDialog::accept() {
    Settings::setSaveDir(m_saveDir->text());
    Settings::setCacheDir(m_cacheDir->text());
    Settings::setFormat(m_format->currentData().toString());
    Settings::setImageQuality(m_quality->value());
    Settings::setCacheMaxSizeMB(m_cacheMaxMB->value());
    Settings::setHotkeyRegion(m_hkRegion->keySequence());
    Settings::setHotkeyFullscreen(m_hkFull->keySequence());
    Settings::setCopyToClipboardOnSave(m_clipboardOnSave->isChecked());
    Settings::setAutostart(m_autostart->isChecked());
    // フォルダが無ければ作成
    QDir().mkpath(m_saveDir->text());
    QDir().mkpath(m_cacheDir->text());
    QDialog::accept();
}
