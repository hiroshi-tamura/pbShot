#include "EditToolbar.h"
#include <QVBoxLayout>
#include <QToolButton>
#include <QButtonGroup>
#include <QIcon>
#include <QColorDialog>
#include <QPixmap>
#include <QPainter>

EditToolbar::EditToolbar(QWidget* parent) : QFrame(parent) {
    setObjectName("EditToolbar");
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::ArrowCursor);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setStyleSheet(
        "QFrame#EditToolbar{background:#1f1f1f;border:1px solid #3a3a3a;border-radius:4px;}"
        "QToolButton{background:transparent;border:none;padding:4px;margin:1px;}"
        "QToolButton:hover{background:#333;border-radius:3px;}"
        "QToolButton:checked{background:#2a6fff;border-radius:3px;}"
    );

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(3, 3, 3, 3);
    lay->setSpacing(1);

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);

    struct Def { const char* res; const char* tip; ToolType t; };
    const Def defs[] = {
        {":/icons/tool_pen.svg",    "ペン",     ToolType::Pen},
        {":/icons/tool_line.svg",   "直線",     ToolType::Line},
        {":/icons/tool_arrow.svg",  "矢印",     ToolType::Arrow},
        {":/icons/tool_rect.svg",   "四角",     ToolType::Rect},
        {":/icons/tool_marker.svg", "マーカー", ToolType::Marker},
        {":/icons/tool_text.svg",   "テキスト", ToolType::Text},
    };

    for (const auto& d : defs) {
        auto* b = makeBtn(d.res, d.tip);
        b->setCheckable(true);
        m_group->addButton(b);
        connect(b, &QToolButton::clicked, this, [this, d, b]() {
            if (!b->isChecked()) {
                m_tool = ToolType::None;
            } else {
                m_tool = d.t;
            }
            emit toolChanged(m_tool);
        });
        lay->addWidget(b);
    }

    // color
    m_colorBtn = makeBtn(":/icons/tool_color.svg", "色");
    connect(m_colorBtn, &QToolButton::clicked, this, &EditToolbar::onColorClicked);
    lay->addWidget(m_colorBtn);

    // undo
    auto* undoBtn = makeBtn(":/icons/tool_undo.svg", "元に戻す (Ctrl+Z)");
    connect(undoBtn, &QToolButton::clicked, this, &EditToolbar::undoRequested);
    lay->addWidget(undoBtn);

    // 色ボタンに現在色のスウォッチを反映
    auto updateColorIcon = [this]() {
        QPixmap px(18, 18);
        px.fill(Qt::transparent);
        QPainter p(&px);
        p.setPen(QPen(Qt::white, 1));
        p.setBrush(m_color);
        p.drawEllipse(2, 2, 14, 14);
        m_colorBtn->setIcon(QIcon(px));
    };
    updateColorIcon();
    connect(this, &EditToolbar::colorChanged, this, [updateColorIcon](QColor){
        updateColorIcon();
    });
}

QToolButton* EditToolbar::makeBtn(const QString& iconRes, const QString& tip) {
    auto* b = new QToolButton(this);
    b->setIcon(QIcon(iconRes));
    b->setIconSize(QSize(20, 20));
    b->setToolTip(tip);
    b->setAutoRaise(true);
    b->setFocusPolicy(Qt::NoFocus);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

void EditToolbar::onColorClicked() {
    QColor c = QColorDialog::getColor(m_color, this, "色を選択");
    if (c.isValid()) {
        m_color = c;
        emit colorChanged(m_color);
    }
}
