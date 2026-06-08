#ifndef WIREITEM_H
#define WIREITEM_H
#include "graphmodeltypes.h"
#include <QGraphicsItem>
#include <QHash>
class NodeItem;
class WireItem : public QGraphicsItem {
    GraphWire wire;
public:
    explicit WireItem(const GraphWire &w, const QHash<QString, NodeItem*> &nodeItems);
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
};
#endif
