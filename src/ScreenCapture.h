#pragma once
#include <QPixmap>
#include <QRect>

class ScreenCapture {
public:
    // 仮想デスクトップ全体をキャプチャ。
    // outVirtualRectLogical: 論理座標での仮想デスクトップ領域（overlay window に使う）
    // 戻り値の QPixmap は devicePixelRatio が設定された状態で返される。
    static QPixmap grabAll(QRect& outVirtualRectLogical, qreal& outDpr);
};
