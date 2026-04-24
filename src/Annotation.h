#pragma once
#include <QPainter>
#include <QColor>
#include <QPoint>
#include <QVector>
#include <QRect>
#include <memory>

enum class ToolType {
    None,
    Arrow,
    Line,
    Rect,
    Pen,
    Marker,
    Text
};

class Annotation {
public:
    Annotation(const QColor& c, int width) : m_color(c), m_width(width) {}
    virtual ~Annotation() = default;
    virtual void paint(QPainter& p) const = 0;
    virtual void extendTo(const QPoint& pt) = 0;
    QColor color() const { return m_color; }
    int width() const { return m_width; }
protected:
    QColor m_color;
    int m_width;
};

class LineAnno : public Annotation {
public:
    LineAnno(const QPoint& start, const QColor& c, int w)
        : Annotation(c, w), m_a(start), m_b(start) {}
    void paint(QPainter& p) const override;
    void extendTo(const QPoint& pt) override { m_b = pt; }
protected:
    QPoint m_a, m_b;
};

class ArrowAnno : public LineAnno {
public:
    using LineAnno::LineAnno;
    void paint(QPainter& p) const override;
};

class RectAnno : public Annotation {
public:
    RectAnno(const QPoint& start, const QColor& c, int w)
        : Annotation(c, w), m_start(start), m_end(start) {}
    void paint(QPainter& p) const override;
    void extendTo(const QPoint& pt) override { m_end = pt; }
private:
    QPoint m_start, m_end;
};

class PenAnno : public Annotation {
public:
    PenAnno(const QPoint& start, const QColor& c, int w)
        : Annotation(c, w) { m_points.append(start); }
    void paint(QPainter& p) const override;
    void extendTo(const QPoint& pt) override { m_points.append(pt); }
private:
    QVector<QPoint> m_points;
};

class MarkerAnno : public Annotation {
public:
    MarkerAnno(const QPoint& start, const QColor& c, int w)
        : Annotation(c, w) { m_points.append(start); }
    void paint(QPainter& p) const override;
    void extendTo(const QPoint& pt) override { m_points.append(pt); }
private:
    QVector<QPoint> m_points;
};

class TextAnno : public Annotation {
public:
    TextAnno(const QPoint& pos, const QSize& size, const QString& text,
             const QColor& c, int pointSize)
        : Annotation(c, pointSize), m_pos(pos), m_size(size), m_text(text) {}
    void paint(QPainter& p) const override;
    void extendTo(const QPoint&) override {}
    void setText(const QString& t) { m_text = t; }
    QString text() const { return m_text; }
private:
    QPoint m_pos;
    QSize m_size;
    QString m_text;
};
