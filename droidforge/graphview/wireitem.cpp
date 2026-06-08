#include "wireitem.h"
#include "nodeitem.h"
#include <QPainter>
WireItem::WireItem(const GraphWire &w, const QHash<QString, NodeItem*> &nodeItems)
    : wire(w)
{
    Q_UNUSED(nodeItems) // endpoint resolution is Task 7
}
QRectF WireItem::boundingRect() const { return QRectF(); }
void WireItem::paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) {
    // placeholder; real paint in Task 7
}
