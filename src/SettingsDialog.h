#pragma once
#include <QDialog>

class QLineEdit;
class QKeySequenceEdit;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QLabel;
class QPushButton;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private slots:
    void browseSaveDir();
    void browseCacheDir();
    void accept() override;

private:
    void updateOcrModelStatus();

    QLineEdit*         m_saveDir = nullptr;
    QLineEdit*         m_cacheDir = nullptr;
    QKeySequenceEdit*  m_hkRegion = nullptr;
    QKeySequenceEdit*  m_hkFull   = nullptr;
    QComboBox*         m_format = nullptr;
    QCheckBox*         m_clipboardOnSave = nullptr;
    QCheckBox*         m_autostart = nullptr;
    QSpinBox*          m_quality = nullptr;
    QSpinBox*          m_cacheMaxMB = nullptr;

    // OCR モデル
    QLabel*            m_ocrStatus = nullptr;
    QPushButton*       m_ocrDownloadBtn = nullptr;
    QPushButton*       m_ocrRemoveBtn = nullptr;
    QPushButton*       m_ocrOpenDirBtn = nullptr;
};
