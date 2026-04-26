#pragma once
#include <QWidget>
#include <QPixmap>
#include <QRect>
#include <QList>
#include <memory>
#include "Annotation.h"

class EditToolbar;
class ActionToolbar;
class QTextEdit;

class OverlayWindow : public QWidget {
    Q_OBJECT
public:
    // shotは既にdevicePixelRatio設定済み。virtualRect は論理座標での仮想デスクトップ。
    OverlayWindow(const QPixmap& shot, const QRect& virtualRectLogical, qreal maxDpr);
    ~OverlayWindow() override;

    // Shift+PrtSc 相当: 選択をスキップして全体を対象にし即保存
    void saveFullScreenAndClose();

signals:
    void finished();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void showEvent(QShowEvent*) override;
    void closeEvent(QCloseEvent*) override;
    void focusOutEvent(QFocusEvent*) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    enum class Stage { Selecting, Idle, Editing };
    enum class HandleHit { None, Move, Body,
                            TL, TR, BL, BR, T, B, L, R };

    void updateToolbarPositions();
    void redraw();
    void addAnnotation(const QPoint& local);
    QPixmap composeFinalImage() const;
    bool saveImageToDisk(bool withDialog);
    void copyToClipboard();
    bool copyPathViaCache();
    void runOcrAndCopy();
    void startAsyncOcr(const QImage& qimg);
    HandleHit hitTest(const QPoint& p) const;
    void setCursorFor(HandleHit h);

    QPixmap m_shot;              // 全画面スクショ（dpr付き）
    QRect   m_virtualRect;       // 論理座標の仮想デスクトップ
    qreal   m_dpr;

    Stage   m_stage = Stage::Selecting;
    QRect   m_selection;         // ウィジェットローカル（=論理座標-topLeft）
    bool    m_dragging = false;
    QPoint  m_dragStart;
    bool    m_ctrlOnPress = false;
    HandleHit m_activeHandle = HandleHit::None;
    QPoint  m_resizeAnchor;      // リサイズ起点
    QRect   m_resizeOrig;

    // アノテーション
    QList<std::shared_ptr<Annotation>> m_annots;
    std::shared_ptr<Annotation> m_current;

    EditToolbar*   m_editBar = nullptr;
    ActionToolbar* m_actionBar = nullptr;

    // テキスト入力
    void startTextInput(const QPoint& at);
    void commitTextInput();
    void cancelTextInput();
    QWidget*   m_textContainer = nullptr;
    QTextEdit* m_textEdit = nullptr;
    QPoint     m_textPos;
    std::shared_ptr<TextAnno> m_pendingText;
};
