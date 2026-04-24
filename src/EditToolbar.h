#pragma once
#include <QFrame>
#include <QColor>
#include "Annotation.h"

class QToolButton;
class QButtonGroup;

class EditToolbar : public QFrame {
    Q_OBJECT
public:
    explicit EditToolbar(QWidget* parent = nullptr);
    ToolType currentTool() const { return m_tool; }
    QColor currentColor() const { return m_color; }
    int currentWidth() const { return m_width; }

signals:
    void toolChanged(ToolType t);
    void colorChanged(QColor c);
    void widthChanged(int w);
    void undoRequested();

private slots:
    void onColorClicked();

private:
    QToolButton* makeBtn(const QString& iconRes, const QString& tip);
    ToolType m_tool = ToolType::None;
    QColor   m_color = QColor(0xE5, 0x39, 0x35); // 赤
    int      m_width = 3;
    QButtonGroup* m_group = nullptr;
    QToolButton* m_colorBtn = nullptr;
};
