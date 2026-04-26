#include "ActionToolbar.h"
#include <QHBoxLayout>
#include <QToolButton>
#include <QIcon>
#include <QLabel>

ActionToolbar::ActionToolbar(QWidget* parent) : QFrame(parent) {
    setObjectName("ActionToolbar");
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::ArrowCursor);
    setStyleSheet(
        "QFrame#ActionToolbar{background:#1f1f1f;border:1px solid #3a3a3a;border-radius:4px;}"
        "QToolButton{color:#e8e8e8;background:transparent;border:none;padding:4px 8px;margin:1px;}"
        "QToolButton:hover{background:#333;border-radius:3px;}"
        "QLabel{color:#bcbcbc;padding:0 8px;}"
    );

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(4, 3, 4, 3);
    lay->setSpacing(1);

    m_sizeLabel = new QLabel(this);
    m_sizeLabel->setText("0 × 0");
    lay->addWidget(m_sizeLabel);
    lay->addSpacing(6);

    auto* ocrBtn = makeBtn(":/icons/act_ocr.svg",
                            "選択範囲の文字を抽出してクリップボードにコピー (OCR)");
    connect(ocrBtn, &QToolButton::clicked, this, &ActionToolbar::ocrRequested);
    lay->addWidget(ocrBtn);

    auto* pathBtn = makeBtn(":/icons/act_pathcopy.svg",
                             "画像をキャッシュに保存してパスをクリップボードにコピー");
    connect(pathBtn, &QToolButton::clicked, this, &ActionToolbar::copyPathRequested);
    lay->addWidget(pathBtn);

    auto* copyBtn = makeBtn(":/icons/act_copy.svg", "クリップボードにコピー (Ctrl+C)");
    connect(copyBtn, &QToolButton::clicked, this, &ActionToolbar::copyRequested);
    lay->addWidget(copyBtn);

    auto* saveBtn = makeBtn(":/icons/act_save.svg", "保存 (Ctrl+S / Enter)");
    connect(saveBtn, &QToolButton::clicked, this, &ActionToolbar::saveRequested);
    lay->addWidget(saveBtn);

    auto* closeBtn = makeBtn(":/icons/act_close.svg", "閉じる (ESC)");
    connect(closeBtn, &QToolButton::clicked, this, &ActionToolbar::closeRequested);
    lay->addWidget(closeBtn);
}

void ActionToolbar::setSizeText(const QString& s) { m_sizeLabel->setText(s); }

QToolButton* ActionToolbar::makeBtn(const QString& iconRes, const QString& tip) {
    auto* b = new QToolButton(this);
    b->setIcon(QIcon(iconRes));
    b->setIconSize(QSize(20, 20));
    b->setToolTip(tip);
    b->setAutoRaise(true);
    b->setFocusPolicy(Qt::NoFocus);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}
