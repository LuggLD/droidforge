#include "wireitem.h"
#include <QPainter>
#include <QPainterPath>

// Tangent offset for the cubic Bézier (horizontal pull).
// Use a fraction of the horizontal distance so short wires still curve nicely.
static qreal tangentX(const QPointF &from, const QPointF &to)
{
    qreal dx = qAbs(to.x() - from.x());
    return qMax(40.0, dx * 0.45);
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
}

QRectF WireItem::boundingRect() const
{
    if (!valid) return QRectF();
    qreal tx = tangentX(fromPt, toPt);
    qreal minX = qMin(fromPt.x(), toPt.x()) - tx;
    qreal maxX = qMax(fromPt.x(), toPt.x()) + tx;
    qreal minY = qMin(fromPt.y(), toPt.y()) - 4;
    qreal maxY = qMax(fromPt.y(), toPt.y()) + 4;
    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY)).normalized();
}

void WireItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if (!valid) return;

    qreal tx = tangentX(fromPt, toPt);
    QPainterPath path;
    path.moveTo(fromPt);
    path.cubicTo(
        QPointF(fromPt.x() + tx, fromPt.y()),
        QPointF(toPt.x()   - tx, toPt.y()),
        toPt
    );

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

    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);
}
