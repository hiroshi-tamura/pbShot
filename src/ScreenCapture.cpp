#include "ScreenCapture.h"
#include <QGuiApplication>
#include <QScreen>
#include <QCursor>
#include <QPainter>
#include <QDebug>
#include <algorithm>

QPixmap ScreenCapture::grabAll(QRect& outRectLogical, qreal& outDpr) {
    // 現在マウスカーソルがあるモニタをターゲットにする（マルチモニタ簡易対応）
    QPoint cursor = QCursor::pos();
    QScreen* target = QGuiApplication::screenAt(cursor);
    if (!target) target = QGuiApplication::primaryScreen();
    if (!target) {
        outRectLogical = QRect();
        outDpr = 1.0;
        return QPixmap();
    }
    const QRect g = target->geometry();        // 論理座標
    const qreal dpr = target->devicePixelRatio();
    outRectLogical = g;
    outDpr = dpr;
    // 戻り値は dpr 付きの pixmap（grabWindow が自動的に設定する）
    return target->grabWindow(0, 0, 0, g.width(), g.height());
}
