#include "wireitem.h"
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QStyleOptionGraphicsItem>
#include <QStyle>

// Half-width of the wire's clickable hit area; boundingRect must cover it so
// the shape() never pokes outside the bounds (Qt requires shape() ⊆ boundingRect()).
static constexpr qreal WIRE_HIT_HALF_WIDTH = 5.0;        // → stroker width 10
static constexpr qreal BOUNDING_PAD_Y = WIRE_HIT_HALF_WIDTH + 1.0; // 1px margin over shape

// Tangent offset for the cubic Bézier (horizontal pull).
// Use a fraction of the horizontal distance so short wires still curve nicely.
static qreal tangentX(const QPointF &from, const QPointF &to)
{
    qreal dx = qAbs(to.x() - from.x());
    return qMax(40.0, dx * 0.45);
}

QPainterPath WireItem::curve(const QPointF &from, const QPointF &to)
{
    qreal tx = tangentX(from, to);
    QPainterPath path;
    path.moveTo(from);
    path.cubicTo(QPointF(from.x() + tx, from.y()),
                 QPointF(to.x() - tx, to.y()), to);
    return path;
}

WireItem::WireItem(const GraphWire &w, const QPointF &from, const QPointF &to)
    : wire(w), fromPt(from), toPt(to)
{
    // A wire is invalid if either endpoint is the null point (QPointF()).
    // Using isNull() would fire for (0,0) which is a valid scene pos, so we
    // treat a sentinel "both zero" as invalid only when the pinId was empty.
    valid = (!wire.fromPinId.isEmpty() || !wire.toPinId.isEmpty())
            && !(from.isNull() && wire.fromPinId.isEmpty())
            && !(to.isNull()   && wire.toPinId.isEmpty());

    // WireItem lives in scene space (no parent), so we set its pos to origin.
    setPos(0, 0);
    setFlag(QGraphicsItem::ItemIsSelectable, valid);
    setZValue(-1); // wires sit behind nodes
}

QRectF WireItem::boundingRect() const
{
    if (!valid) return QRectF();
    qreal tx = tangentX(fromPt, toPt);
    qreal minX = qMin(fromPt.x(), toPt.x()) - tx;
    qreal maxX = qMax(fromPt.x(), toPt.x()) + tx;
    qreal minY = qMin(fromPt.y(), toPt.y()) - BOUNDING_PAD_Y;
    qreal maxY = qMax(fromPt.y(), toPt.y()) + BOUNDING_PAD_Y;
    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY)).normalized();
}

QPainterPath WireItem::shape() const
{
    if (!valid) return QPainterPath();
    QPainterPathStroker stroker;
    stroker.setWidth(WIRE_HIT_HALF_WIDTH * 2.0); // generous click target
    return stroker.createStroke(curve(fromPt, toPt));
}

void WireItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    if (!valid) return;

    QPainterPath path = curve(fromPt, toPt);

    QPen pen;
    if (wire.isCable) {
        // Named cable: solid, slightly thicker, warm color
        pen.setColor(QColor(255, 196, 107));  // warm yellow-orange
        pen.setWidthF(2.0);
        pen.setStyle(Qt::SolidLine);
    } else {
        // Register connection: thinner, dashed, cooler color
        pen.setColor(QColor(131, 247, 178, 180)); // translucent green
        pen.setWidthF(1.2);
        pen.setStyle(Qt::DashLine);
    }
    pen.setCapStyle(Qt::RoundCap);

    if (option->state & QStyle::State_Selected) {
        pen.setColor(QColor(255, 255, 255));
        pen.setWidthF(pen.widthF() + 1.5);
    }

    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);
}
