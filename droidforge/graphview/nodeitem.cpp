#include "nodeitem.h"
#include <QPainter>
NodeItem::NodeItem(const GraphNode &n) : node(n) {}
QRectF NodeItem::boundingRect() const { return QRectF(0, 0, 200, 120); }
void NodeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    painter->drawRoundedRect(boundingRect(), 6, 6); // placeholder; real paint in Task 7
}
