#include "Annotation.h"
#include <QPolygon>
#include <cmath>

void LineAnno::paint(QPainter& p) const {
    QPen pen(m_color, m_width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawLine(m_a, m_b);
}

void ArrowAnno::paint(QPainter& p) const {
    QPen pen(m_color, m_width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(m_color);
    p.drawLine(m_a, m_b);
    QLineF l(m_a, m_b);
    if (l.length() < 1.0) return;
    double ang = std::atan2(l.dy(), l.dx());
    double headLen = std::max(10.0, (double)m_width * 4.0);
    double headAng = 0.5; // rad
    QPointF h1 = m_b - QPointF(std::cos(ang - headAng) * headLen,
                                std::sin(ang - headAng) * headLen);
    QPointF h2 = m_b - QPointF(std::cos(ang + headAng) * headLen,
                                std::sin(ang + headAng) * headLen);
    QPolygonF poly;
    poly << m_b << h1 << h2;
    p.drawPolygon(poly);
}

void RectAnno::paint(QPainter& p) const {
    QPen pen(m_color, m_width);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRect(m_start, m_end).normalized());
}

void PenAnno::paint(QPainter& p) const {
    QPen pen(m_color, m_width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    if (m_points.size() < 2) {
        p.drawPoint(m_points.isEmpty() ? QPoint() : m_points.first());
        return;
    }
    for (int i = 1; i < m_points.size(); ++i)
        p.drawLine(m_points[i - 1], m_points[i]);
}

void TextAnno::paint(QPainter& p) const {
    if (m_text.isEmpty()) return;
    QFont f = p.font();
    f.setPointSize(std::max(8, m_width * 4));
    f.setBold(true);
    p.setFont(f);
    p.setPen(m_color);
    p.drawText(QRect(m_pos, m_size),
               Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
               m_text);
}

void MarkerAnno::paint(QPainter& p) const {
    QColor c = m_color;
    c.setAlpha(90);
    QPen pen(c, m_width * 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    if (m_points.size() < 2) {
        p.drawPoint(m_points.isEmpty() ? QPoint() : m_points.first());
        return;
    }
    for (int i = 1; i < m_points.size(); ++i)
        p.drawLine(m_points[i - 1], m_points[i]);
}
