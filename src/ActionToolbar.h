#pragma once
#include <QFrame>

class QToolButton;
class QLabel;

class ActionToolbar : public QFrame {
    Q_OBJECT
public:
    explicit ActionToolbar(QWidget* parent = nullptr);
    void setSizeText(const QString& s);

signals:
    void copyRequested();
    void copyPathRequested();
    void ocrRequested();
    void saveRequested();
    void closeRequested();

private:
    QToolButton* makeBtn(const QString& iconRes, const QString& tip);
    QLabel* m_sizeLabel = nullptr;
};
