#include "OverlayWindow.h"
#include "EditToolbar.h"
#include "ActionToolbar.h"
#include "Settings.h"
#include "Ocr.h"
#include "OcrModelManager.h"
#include "TesseractOcr.h"
#include "Toast.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QApplication>
#include <QClipboard>
#include <QShortcut>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <QDebug>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QPointer>
#include <QProgressDialog>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <QPair>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
// 対象 widget 専用の右下リサイズグリップ（QSizeGrip のように top-level を動かさない）
class BoxResizer : public QWidget {
public:
    explicit BoxResizer(QWidget* target) : QWidget(target->parentWidget()), m_target(target) {
        setFixedSize(14, 14);
        setCursor(Qt::SizeFDiagCursor);
        setAttribute(Qt::WA_StyledBackground, false);
    }
    void reposition() {
        QPoint p = m_target->geometry().bottomRight() - QPoint(width() - 1, height() - 1);
        move(p);
        raise();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setPen(QPen(QColor(255, 255, 255, 200), 1));
        for (int i = 4; i < width(); i += 3) {
            p.drawLine(i, height() - 1, width() - 1, i);
        }
    }
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            m_dragging = true;
            m_startGlobal = e->globalPosition().toPoint();
            m_startSize = m_target->size();
            e->accept();
        }
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (!m_dragging) return;
        QPoint d = e->globalPosition().toPoint() - m_startGlobal;
        int w = std::max(80,  m_startSize.width()  + d.x());
        int h = std::max(40,  m_startSize.height() + d.y());
        m_target->resize(w, h);
        reposition();
        e->accept();
    }
    void mouseReleaseEvent(QMouseEvent*) override { m_dragging = false; }
private:
    QWidget* m_target;
    bool m_dragging = false;
    QPoint m_startGlobal;
    QSize  m_startSize;
};
} // namespace

static void pbLog(const QString& s) {
    QFile f(QDir::tempPath() + "/pbShot.log");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << QDateTime::currentDateTime().toString("hh:mm:ss.zzz ") << s << "\n";
    }
}

static const int kHandleSize = 8;
static const int kHandleHit = 10;

OverlayWindow::OverlayWindow(const QPixmap& shot, const QRect& virtualRectLogical, qreal maxDpr)
    : QWidget(nullptr),
      m_shot(shot),
      m_virtualRect(virtualRectLogical),
      m_dpr(maxDpr)
{
    // フレームレス・最前面。
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus);

    // 対象モニタにぴったり収まるように論理座標で配置
    setGeometry(m_virtualRect);
    // 念のため最小・最大を固定しておく
    setFixedSize(m_virtualRect.size());
    pbLog(QString("OverlayWindow ctor vrect=%1,%2 %3x%4 dpr=%5 shot=%6x%7")
          .arg(m_virtualRect.x()).arg(m_virtualRect.y())
          .arg(m_virtualRect.width()).arg(m_virtualRect.height())
          .arg(m_dpr).arg(shot.width()).arg(shot.height()));

    m_editBar   = new EditToolbar(this);
    m_actionBar = new ActionToolbar(this);
    m_editBar->hide();
    m_actionBar->hide();

    connect(m_editBar, &EditToolbar::undoRequested, this, [this]() {
        if (!m_annots.isEmpty()) { m_annots.removeLast(); update(); }
    });
    connect(m_actionBar, &ActionToolbar::copyRequested,  this, [this](){
        copyToClipboard();
        emit finished();
        close();
    });
    connect(m_actionBar, &ActionToolbar::copyPathRequested, this, [this](){
        if (copyPathViaCache()) {
            emit finished();
            close();
        }
    });
    connect(m_actionBar, &ActionToolbar::ocrRequested, this, [this](){
        runOcrAndCopy();
    });
    connect(m_actionBar, &ActionToolbar::saveRequested,  this, [this](){
        if (saveImageToDisk(true)) {
            emit finished();
            close();
        }
    });
    connect(m_actionBar, &ActionToolbar::closeRequested, this, [this](){
        emit finished();
        close();
    });

    // ショートカット
    auto* scSave = new QShortcut(QKeySequence("Ctrl+S"), this);
    connect(scSave, &QShortcut::activated, this, [this]{
        if (m_stage != Stage::Selecting) {
            if (saveImageToDisk(true)) { emit finished(); close(); }
        }
    });
    auto* scCopy = new QShortcut(QKeySequence("Ctrl+C"), this);
    connect(scCopy, &QShortcut::activated, this, [this]{
        if (m_stage != Stage::Selecting) { copyToClipboard(); emit finished(); close(); }
    });
    auto* scUndo = new QShortcut(QKeySequence("Ctrl+Z"), this);
    connect(scUndo, &QShortcut::activated, this, [this]{
        if (!m_annots.isEmpty()) { m_annots.removeLast(); update(); }
    });
}

OverlayWindow::~OverlayWindow() = default;

void OverlayWindow::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    pbLog(QString("showEvent geom=%1,%2 %3x%4 isVisible=%5")
          .arg(geometry().x()).arg(geometry().y())
          .arg(geometry().width()).arg(geometry().height())
          .arg(isVisible()));
#ifdef _WIN32
    HWND hwnd = reinterpret_cast<HWND>(winId());
    // 物理ピクセル基準でトップへ（仮想デスクトップ全体を物理座標で覆う）
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
    // foreground lock 回避のための AttachThreadInput パターン
    HWND fg = GetForegroundWindow();
    DWORD myTid = GetCurrentThreadId();
    DWORD fgTid = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
    if (fgTid && fgTid != myTid) AttachThreadInput(fgTid, myTid, TRUE);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    if (fgTid && fgTid != myTid) AttachThreadInput(fgTid, myTid, FALSE);
    pbLog(QString("post-WinAPI fg=%1 isActive=%2").arg((quintptr)GetForegroundWindow()).arg((quintptr)hwnd));
#endif
    raise();
    activateWindow();
    setFocus();
    // grabMouse/grabKeyboard は使わない（子のツールバークリックをブロックしてしまうため）。
    // Overlay は最前面のフルスクリーンウィンドウなので入力は自然に届く。
}

void OverlayWindow::closeEvent(QCloseEvent* e) {
    pbLog("closeEvent");
    QWidget::closeEvent(e);
}

void OverlayWindow::startTextInput(const QPoint& at) {
    if (m_textContainer) commitTextInput();
    m_textPos = at;
    QColor c = m_editBar->currentColor();
    int w = m_editBar->currentWidth();
    int pt = std::max(10, w * 4);

    // コンテナ: QTextEdit + 右下 QSizeGrip
    m_textContainer = new QWidget(this);
    m_textContainer->setObjectName("TextInputBox");
    m_textContainer->setCursor(Qt::ArrowCursor);
    m_textContainer->setAttribute(Qt::WA_StyledBackground, true);
    m_textContainer->setStyleSheet(QString(
        "QWidget#TextInputBox{background:rgba(0,0,0,150);border:1px dashed %1;}"
    ).arg(c.name()));

    auto* lay = new QVBoxLayout(m_textContainer);
    lay->setContentsMargins(2, 2, 2, 2);
    lay->setSpacing(0);

    m_textEdit = new QTextEdit(m_textContainer);
    m_textEdit->setAcceptRichText(false);
    m_textEdit->setFrameShape(QFrame::NoFrame);
    m_textEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    m_textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textEdit->setFocusPolicy(Qt::StrongFocus);
    m_textEdit->setCursor(Qt::IBeamCursor);
    QFont f = m_textEdit->font();
    f.setPointSize(pt);
    f.setBold(true);
    m_textEdit->setFont(f);
    m_textEdit->setStyleSheet(QString(
        "QTextEdit{background:transparent;color:%1;border:none;"
        "selection-background-color:#2a6fff;}"
    ).arg(c.name()));
    lay->addWidget(m_textEdit, 1);

    m_textContainer->resize(260, 90);
    m_textContainer->move(at);
    m_textContainer->show();
    m_textContainer->raise();

    // 親(this=overlay)の子として配置する専用リサイザ。top-level windowは動かさない。
    auto* resizer = new BoxResizer(m_textContainer);
    resizer->show();
    resizer->reposition();
    m_textEdit->setFocus();

    // ESC/Ctrl+Enter を textEdit で拾う
    m_textEdit->installEventFilter(this);
}

bool OverlayWindow::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == m_textEdit && ev->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(ev);
        if (ke->key() == Qt::Key_Escape) {
            cancelTextInput();
            return true;
        }
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) &&
            ke->modifiers().testFlag(Qt::ControlModifier)) {
            commitTextInput();
            return true;
        }
        // 通常 Enter は QTextEdit のデフォルト動作で改行
    }
    return QWidget::eventFilter(obj, ev);
}

void OverlayWindow::commitTextInput() {
    if (!m_textContainer) return;
    QString txt = m_textEdit->toPlainText();
    QColor c = m_editBar->currentColor();
    int w = m_editBar->currentWidth();
    QPoint pos = m_textContainer->pos() + QPoint(2, 2); // コンテナ内側余白分
    // コンテナ内の textEdit サイズ（viewport）をそのまま注釈のサイズとする
    QSize size = m_textEdit->viewport()->size();
    m_textContainer->hide();
    m_textContainer->deleteLater();
    m_textContainer = nullptr;
    m_textEdit = nullptr;
    if (!txt.isEmpty()) {
        auto a = std::make_shared<TextAnno>(pos, size, txt, c, w);
        m_annots.append(a);
    }
    m_stage = Stage::Idle;
    updateToolbarPositions();
    m_editBar->show();
    m_actionBar->show();
    setFocus();
    update();
}

void OverlayWindow::cancelTextInput() {
    if (!m_textContainer) return;
    m_textContainer->hide();
    m_textContainer->deleteLater();
    m_textContainer = nullptr;
    m_textEdit = nullptr;
    m_stage = Stage::Idle;
    updateToolbarPositions();
    m_editBar->show();
    m_actionBar->show();
    setFocus();
    update();
}

void OverlayWindow::focusOutEvent(QFocusEvent* e) {
    pbLog("focusOut");
    QWidget::focusOutEvent(e);
}

void OverlayWindow::saveFullScreenAndClose() {
    // 選択をスキップ：仮想デスクトップ全体を即保存（ダイアログ無し）
    m_selection = rect();
    saveImageToDisk(false);
    emit finished();
    close();
}

void OverlayWindow::paintEvent(QPaintEvent*) {
    static int cnt = 0;
    if (++cnt < 4) pbLog(QString("paintEvent #%1 size=%2x%3").arg(cnt).arg(width()).arg(height()));
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 背景に全画面スクショ（論理座標で描画、dpr付きなので高DPIでもくっきり）
    p.drawPixmap(rect(), m_shot);

    // 暗幕
    QColor mask(0, 0, 0, 120);
    p.fillRect(rect(), mask);

    if (!m_selection.isNull()) {
        // 選択領域だけ元画像で上書き（論理→dpr物理に変換してsrc指定）
        QRect sel = m_selection.normalized().intersected(rect());
        QRectF src(sel.x() * m_dpr, sel.y() * m_dpr,
                   sel.width() * m_dpr, sel.height() * m_dpr);
        p.drawPixmap(sel, m_shot, src);

        // アノテーション描画（選択内クリップ）
        p.save();
        p.setClipRect(sel);
        for (const auto& a : m_annots) a->paint(p);
        if (m_current) m_current->paint(p);
        p.restore();

        // 選択枠
        QPen framePen(QColor(0x2a, 0x6f, 0xff), 1);
        p.setPen(framePen);
        p.setBrush(Qt::NoBrush);
        p.drawRect(sel);

        // ハンドル
        p.setBrush(QColor(0x2a, 0x6f, 0xff));
        auto drawHandle = [&](int x, int y) {
            p.drawRect(QRect(x - kHandleSize / 2, y - kHandleSize / 2, kHandleSize, kHandleSize));
        };
        drawHandle(sel.left(),  sel.top());
        drawHandle(sel.right(), sel.top());
        drawHandle(sel.left(),  sel.bottom());
        drawHandle(sel.right(), sel.bottom());
        drawHandle(sel.center().x(), sel.top());
        drawHandle(sel.center().x(), sel.bottom());
        drawHandle(sel.left(),  sel.center().y());
        drawHandle(sel.right(), sel.center().y());

        // サイズラベル
        QString txt = QString("%1 × %2").arg(sel.width()).arg(sel.height());
        QFont f = p.font(); f.setPointSize(10); p.setFont(f);
        QFontMetrics fm(f);
        QRect tr = fm.boundingRect(txt).adjusted(-6, -3, 6, 3);
        tr.moveBottomLeft(QPoint(sel.left(), sel.top() - 4));
        if (tr.top() < 4) tr.moveTopLeft(QPoint(sel.left() + 4, sel.top() + 4));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 180));
        p.drawRoundedRect(tr, 3, 3);
        p.setPen(Qt::white);
        p.drawText(tr, Qt::AlignCenter, txt);
    }
}

OverlayWindow::HandleHit OverlayWindow::hitTest(const QPoint& p) const {
    if (m_selection.isNull()) return HandleHit::None;
    QRect sel = m_selection.normalized();
    const int corner = 12;   // 角ハンドルの当たり半径
    const int edgeH  = 10;   // 中央辺ハンドルの当たり半径
    const int band   = 6;    // 枠線の帯

    auto nearPt = [&](int x, int y, int r) {
        return QRect(x - r, y - r, r * 2, r * 2).contains(p);
    };
    // 角（リサイズ）優先
    if (nearPt(sel.left(),  sel.top(),    corner)) return HandleHit::TL;
    if (nearPt(sel.right(), sel.top(),    corner)) return HandleHit::TR;
    if (nearPt(sel.left(),  sel.bottom(), corner)) return HandleHit::BL;
    if (nearPt(sel.right(), sel.bottom(), corner)) return HandleHit::BR;
    // 中央辺（リサイズ）
    if (nearPt(sel.center().x(), sel.top(),    edgeH)) return HandleHit::T;
    if (nearPt(sel.center().x(), sel.bottom(), edgeH)) return HandleHit::B;
    if (nearPt(sel.left(),  sel.center().y(), edgeH)) return HandleHit::L;
    if (nearPt(sel.right(), sel.center().y(), edgeH)) return HandleHit::R;
    // 枠線の帯 → Move（外側・内側 bandピクセル）
    QRect outer = sel.adjusted(-band, -band,  band,  band);
    QRect inner = sel.adjusted( band,  band, -band, -band);
    if (outer.contains(p) && !inner.contains(p)) return HandleHit::Move;
    if (sel.contains(p)) return HandleHit::Body;
    return HandleHit::None;
}

void OverlayWindow::setCursorFor(HandleHit h) {
    switch (h) {
    case HandleHit::TL: case HandleHit::BR: setCursor(Qt::SizeFDiagCursor); break;
    case HandleHit::TR: case HandleHit::BL: setCursor(Qt::SizeBDiagCursor); break;
    case HandleHit::T:  case HandleHit::B:  setCursor(Qt::SizeVerCursor); break;
    case HandleHit::L:  case HandleHit::R:  setCursor(Qt::SizeHorCursor); break;
    case HandleHit::Move: setCursor(Qt::SizeAllCursor); break;     // 枠ドラッグで移動
    case HandleHit::Body: setCursor(Qt::CrossCursor); break;       // 既定（描画時）
    default: setCursor(Qt::CrossCursor); break;
    }
}

void OverlayWindow::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    m_ctrlOnPress = e->modifiers().testFlag(Qt::ControlModifier);

    if (m_stage == Stage::Selecting) {
        m_dragging = true;
        m_dragStart = e->pos();
        m_selection = QRect(m_dragStart, m_dragStart);
        update();
        return;
    }

    // Idle or Editing
    HandleHit h = hitTest(e->pos());
    if (h != HandleHit::None && h != HandleHit::Body) {
        m_activeHandle = h;
        m_resizeOrig = m_selection.normalized();
        m_resizeAnchor = e->pos();
        m_dragging = true;
        m_editBar->hide(); m_actionBar->hide();
        return;
    }
    if (h == HandleHit::Body) {
        ToolType t = m_editBar->currentTool();
        if (t == ToolType::None) {
            // 選択矩形全体を移動
            m_activeHandle = HandleHit::Move;
            m_resizeOrig = m_selection.normalized();
            m_resizeAnchor = e->pos();
            m_dragging = true;
            m_editBar->hide(); m_actionBar->hide();
            setCursor(Qt::SizeAllCursor);
            return;
        }
        // テキストツールの場合はインライン入力欄を出す
        if (t == ToolType::Text) {
            startTextInput(e->pos());
            return;
        }
        // アノテーション開始
        m_stage = Stage::Editing;
        m_editBar->hide(); m_actionBar->hide();
        QColor c = m_editBar->currentColor();
        int w = m_editBar->currentWidth();
        QPoint pt = e->pos();
        std::shared_ptr<Annotation> a;
        switch (t) {
        case ToolType::Line:   a = std::make_shared<LineAnno>(pt, c, w); break;
        case ToolType::Arrow:  a = std::make_shared<ArrowAnno>(pt, c, w); break;
        case ToolType::Rect:   a = std::make_shared<RectAnno>(pt, c, w); break;
        case ToolType::Pen:    a = std::make_shared<PenAnno>(pt, c, w); break;
        case ToolType::Marker: a = std::make_shared<MarkerAnno>(pt, c, w); break;
        default: return;
        }
        m_current = a;
        m_dragging = true;
        return;
    }
}

void OverlayWindow::mouseMoveEvent(QMouseEvent* e) {
    if (m_stage == Stage::Selecting && m_dragging) {
        m_selection = QRect(m_dragStart, e->pos()).normalized();
        update();
        return;
    }
    if (m_dragging && m_activeHandle != HandleHit::None) {
        QRect r = m_resizeOrig;
        QPoint d = e->pos() - m_resizeAnchor;
        switch (m_activeHandle) {
        case HandleHit::Move: r.translate(d); break;
        case HandleHit::TL: r.setTopLeft(r.topLeft() + d); break;
        case HandleHit::TR: r.setTopRight(r.topRight() + d); break;
        case HandleHit::BL: r.setBottomLeft(r.bottomLeft() + d); break;
        case HandleHit::BR: r.setBottomRight(r.bottomRight() + d); break;
        case HandleHit::T:  r.setTop(r.top() + d.y()); break;
        case HandleHit::B:  r.setBottom(r.bottom() + d.y()); break;
        case HandleHit::L:  r.setLeft(r.left() + d.x()); break;
        case HandleHit::R:  r.setRight(r.right() + d.x()); break;
        default: break;
        }
        // Move の場合は画面外クランプ、リサイズの場合は内部にとどめる
        if (m_activeHandle == HandleHit::Move) {
            QRect bounds = rect();
            if (r.left()   < bounds.left())   r.moveLeft(bounds.left());
            if (r.top()    < bounds.top())    r.moveTop(bounds.top());
            if (r.right()  > bounds.right())  r.moveRight(bounds.right());
            if (r.bottom() > bounds.bottom()) r.moveBottom(bounds.bottom());
            m_selection = r;
        } else {
            m_selection = r.normalized().intersected(rect());
        }
        update();
        return;
    }
    if (m_stage == Stage::Editing && m_dragging && m_current) {
        m_current->extendTo(e->pos());
        update();
        return;
    }
    // hover cursor — ツールバー上はそれぞれのwidgetが自前でセットするのでスキップ
    if (m_stage != Stage::Selecting) {
        QWidget* under = childAt(e->pos());
        if (under && under != this) return;
        HandleHit h = hitTest(e->pos());
        if (h == HandleHit::Body) {
            setCursor(m_editBar->currentTool() == ToolType::None
                      ? Qt::SizeAllCursor : Qt::CrossCursor);
        } else {
            setCursorFor(h);
        }
    }
}

void OverlayWindow::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    if (m_stage == Stage::Selecting && m_dragging) {
        m_dragging = false;
        m_selection = QRect(m_dragStart, e->pos()).normalized().intersected(rect());
        if (m_selection.width() < 3 || m_selection.height() < 3) {
            // 点クリック扱い: 何もしない
            m_selection = QRect();
            update();
            return;
        }
        m_stage = Stage::Idle;
        // Ctrl 押下ならそのままクリップボードへ
        if (m_ctrlOnPress) {
            copyToClipboard();
            emit finished();
            close();
            return;
        }
        updateToolbarPositions();
        m_editBar->show();
        m_actionBar->show();
        update();
        return;
    }
    if (m_dragging && m_activeHandle != HandleHit::None) {
        m_dragging = false;
        m_activeHandle = HandleHit::None;
        updateToolbarPositions();
        m_editBar->show();
        m_actionBar->show();
        update();
        return;
    }
    if (m_stage == Stage::Editing && m_dragging && m_current) {
        m_dragging = false;
        m_current->extendTo(e->pos());
        m_annots.append(m_current);
        m_current.reset();
        m_stage = Stage::Idle;
        updateToolbarPositions();
        m_editBar->show();
        m_actionBar->show();
        update();
        return;
    }
}

void OverlayWindow::keyPressEvent(QKeyEvent* e) {
    pbLog(QString("keyPress key=0x%1 mod=0x%2").arg(e->key(),0,16).arg((int)e->modifiers(),0,16));
    if (e->key() == Qt::Key_Escape) {
        emit finished();
        close();
        return;
    }
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        if (m_stage != Stage::Selecting && !m_selection.isNull()) {
            if (saveImageToDisk(true)) { emit finished(); close(); }
        }
        return;
    }
    QWidget::keyPressEvent(e);
}

void OverlayWindow::updateToolbarPositions() {
    if (m_selection.isNull()) return;
    QRect sel = m_selection.normalized();
    m_editBar->adjustSize();
    m_actionBar->adjustSize();
    m_actionBar->setSizeText(QString("%1 × %2").arg(sel.width()).arg(sel.height()));
    m_actionBar->adjustSize();

    // 縦ツールバー: 右、垂直中央寄せ
    int ebX = sel.right() + 6;
    if (ebX + m_editBar->width() > width() - 4)
        ebX = sel.left() - 6 - m_editBar->width();
    int ebY = sel.top();
    if (ebY + m_editBar->height() > height() - 4)
        ebY = height() - 4 - m_editBar->height();
    if (ebY < 4) ebY = 4;
    m_editBar->move(ebX, ebY);

    // 横ツールバー: 下、右寄せ
    int abY = sel.bottom() + 6;
    if (abY + m_actionBar->height() > height() - 4)
        abY = sel.top() - 6 - m_actionBar->height();
    int abX = sel.right() - m_actionBar->width();
    if (abX < 4) abX = 4;
    if (abY < 4) abY = 4;
    m_actionBar->move(abX, abY);

    m_editBar->raise();
    m_actionBar->raise();
}

QPixmap OverlayWindow::composeFinalImage() const {
    QRect sel = m_selection.normalized().intersected(rect());
    if (sel.isEmpty()) return QPixmap();

    // 物理ピクセルで切り出す
    QRect physSel(int(std::round(sel.x() * m_dpr)),
                  int(std::round(sel.y() * m_dpr)),
                  int(std::round(sel.width() * m_dpr)),
                  int(std::round(sel.height() * m_dpr)));
    QPixmap out(physSel.size());
    out.setDevicePixelRatio(m_dpr);
    out.fill(Qt::transparent);

    // 1) 背景: m_shot 物理ピクセルから切り出し、out の論理座標に描画（dprで自動スケール）
    {
        QPainter p(&out);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        QImage src = m_shot.toImage();
        src.setDevicePixelRatio(1.0);
        p.drawImage(QRect(0, 0, sel.width(), sel.height()), src, physSel);
    }
    // 2) アノテーション（論理座標 / widget-local → sel-local へ平行移動）
    {
        QPainter q(&out);
        q.setRenderHint(QPainter::Antialiasing, true);
        q.translate(-sel.topLeft());
        for (const auto& a : m_annots) a->paint(q);
    }
    return out;
}

bool OverlayWindow::saveImageToDisk(bool withDialog) {
    QPixmap img = composeFinalImage();
    if (img.isNull()) return false;

    QString path;
    if (withDialog) {
        // overlay が topmost のままだとダイアログが潜るので一時解除
#ifdef _WIN32
        HWND hwnd = reinterpret_cast<HWND>(winId());
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
#endif
        QString def = Settings::nextFilename();
        QString filter = "PNG (*.png);;JPEG (*.jpg *.jpeg);;Bitmap (*.bmp)";
        path = QFileDialog::getSaveFileName(this, "スクリーンショットを保存", def, filter);
#ifdef _WIN32
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        activateWindow();
        setFocus();
#endif
        if (path.isEmpty()) return false;  // キャンセル
        Settings::setSaveDir(QFileInfo(path).absolutePath());
    } else {
        path = Settings::nextFilename();
    }

    QString ext = QFileInfo(path).suffix().toUpper();
    if (ext.isEmpty()) { ext = "PNG"; path += ".png"; }
    int q = Settings::imageQuality();
    bool ok = img.save(path, ext.toUtf8().constData(), q);
    if (ok && Settings::copyToClipboardOnSave()) {
        QGuiApplication::clipboard()->setPixmap(img);
    }
    return ok;
}

void OverlayWindow::copyToClipboard() {
    QPixmap img = composeFinalImage();
    if (!img.isNull()) QGuiApplication::clipboard()->setPixmap(img);
}

bool OverlayWindow::copyPathViaCache() {
    QPixmap img = composeFinalImage();
    if (img.isNull()) return false;
    QString path = Settings::nextCacheFilename();
    if (!img.save(path, "PNG", Settings::imageQuality())) return false;
    Settings::pruneCache();
    QString nativePath = QDir::toNativeSeparators(path);
    QGuiApplication::clipboard()->setText(nativePath);
    return true;
}

void OverlayWindow::runOcrAndCopy() {
    QPixmap img = composeFinalImage();
    if (img.isNull()) return;
    QImage qimg = img.toImage();

    const QString engine = Settings::ocrEngine();

    // Tesseract 経路: モデル DL 不要、tesseract.exe があれば即実行。
    if (engine == "tesseract") {
        if (!TesseractOcr::instance().isReady()) {
            QMessageBox::warning(this, "OCR",
                "Tesseract が見つかりません。\n"
                "exe と同じフォルダに tesseract/ サブフォルダを配置するか、"
                "PATH 上に tesseract.exe を置いてください。");
            return;
        }
        startAsyncOcr(qimg);
        return;
    }

    // PP-OCR 経路: モデルが揃っていれば直行、無ければ DL してから推論。
    auto* models = Ocr::instance().models();
    if (models->isReady()) {
        startAsyncOcr(qimg);
        return;
    }

    // モデル不足 → DL → ready で非同期 OCR 開始
#ifdef _WIN32
    HWND hwnd = reinterpret_cast<HWND>(winId());
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
#endif

    QPointer<OverlayWindow> self(this);
    auto* nReady  = new QMetaObject::Connection;
    auto* nFailed = new QMetaObject::Connection;
    *nReady = connect(models, &OcrModelManager::ready, this,
        [self, qimg, nReady, nFailed]() {
            QObject::disconnect(*nReady);
            QObject::disconnect(*nFailed);
            delete nReady; delete nFailed;
            if (self) self->startAsyncOcr(qimg);
        });
    *nFailed = connect(models, &OcrModelManager::failed, this,
        [self, nReady, nFailed](const QString& msg) {
            QObject::disconnect(*nReady);
            QObject::disconnect(*nFailed);
            delete nReady; delete nFailed;
            if (!self) return;
            QMessageBox::warning(self, "OCR モデル",
                QString("モデルのダウンロードに失敗しました:\n%1").arg(msg));
#ifdef _WIN32
            HWND h = reinterpret_cast<HWND>(self->winId());
            SetWindowPos(h, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            self->activateWindow(); self->setFocus();
#endif
        });

    models->ensureModelsAsync(this);
}

void OverlayWindow::startAsyncOcr(const QImage& qimg) {
    const QString engine = Settings::ocrEngine();
    const bool useTesseract = (engine == "tesseract");

    // 1) オーバーレイを即閉じて UI ブロック感を解消
    emit finished();
    close();

    // 2) 進捗ダイアログ（最初は busy 表示、進捗が来たら確定値に切替）
    auto* dlg = new QProgressDialog(
        QStringLiteral("OCR 処理中…"),
        QString(),
        0, 0, nullptr);
    dlg->setWindowTitle(useTesseract ? QStringLiteral("OCR (Tesseract)")
                                     : QStringLiteral("OCR (PP-OCR)"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowModality(Qt::NonModal);
    dlg->setCancelButton(nullptr);
    dlg->setMinimumDuration(0);
    dlg->setRange(0, 0);
    dlg->setWindowFlags(dlg->windowFlags() | Qt::WindowStaysOnTopHint);
    dlg->show();
    dlg->raise();
    QPointer<QProgressDialog> dlgPtr(dlg);

    // 3) 進捗シグナルをダイアログに反映（別スレッド→UIスレッド自動 queued）
    auto applyProgress = [dlgPtr](int current, int total, const QString& stage) {
        if (!dlgPtr) return;
        if (total > 0) {
            if (dlgPtr->maximum() != total) dlgPtr->setRange(0, total);
            dlgPtr->setValue(current);
            dlgPtr->setLabelText(QString("%1  %2 / %3")
                                 .arg(stage).arg(current).arg(total));
        } else {
            dlgPtr->setRange(0, 0);
            dlgPtr->setLabelText(stage.isEmpty()
                                 ? QStringLiteral("OCR 処理中…") : stage);
        }
    };

    QMetaObject::Connection progConn;
    if (useTesseract) {
        progConn = QObject::connect(&TesseractOcr::instance(),
                                    &TesseractOcr::progress, dlg, applyProgress);
    } else {
        progConn = QObject::connect(&Ocr::instance(),
                                    &Ocr::progress, dlg, applyProgress);
    }

    // 4) 別スレッドで recognize 実行
    using OcrPair = QPair<QString, QString>;  // {text, err}
    auto future = QtConcurrent::run([qimg, useTesseract]() -> OcrPair {
        QString err;
        QString text;
        if (useTesseract) {
            text = TesseractOcr::instance().recognize(qimg, "jpn+eng", &err);
        } else {
            text = Ocr::instance().recognize(qimg, &err);
        }
        return qMakePair(text, err);
    });

    // 5) 完了時に UI スレッドで結果反映
    auto* watcher = new QFutureWatcher<OcrPair>(qApp);
    QObject::connect(watcher, &QFutureWatcher<OcrPair>::finished, qApp,
        [watcher, dlgPtr, progConn, useTesseract]() {
            QObject::disconnect(progConn);
            const OcrPair r = watcher->result();
            watcher->deleteLater();
            if (dlgPtr) dlgPtr->close();

            const QString& text = r.first;
            const QString& err  = r.second;

            const QString title = useTesseract
                ? QStringLiteral("OCR 完了 (Tesseract)")
                : QStringLiteral("OCR 完了 (PP-OCR)");
            const QString failTitle = useTesseract
                ? QStringLiteral("OCR 失敗 (Tesseract)")
                : QStringLiteral("OCR 失敗 (PP-OCR)");

            if (!text.isEmpty()) {
                QGuiApplication::clipboard()->setText(text);
                const int chars = text.length();
                const int lines = text.count(QLatin1Char('\n')) + 1;
                Toast::show(
                    title,
                    QStringLiteral("%1 文字 / %2 行をクリップボードにコピーしました")
                        .arg(chars).arg(lines));
            } else {
                Toast::show(
                    failTitle,
                    err.isEmpty()
                        ? QStringLiteral("テキストが検出できませんでした")
                        : err);
            }
        });
    watcher->setFuture(future);
}
