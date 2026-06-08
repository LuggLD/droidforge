#ifndef NODEITEM_H
#define NODEITEM_H
#include "graphmodeltypes.h"
#include <QGraphicsItem>
class NodeItem : public QGraphicsItem {
    GraphNode node;
public:
    explicit NodeItem(const GraphNode &n);
    const GraphNode &graphNode() const { return node; }
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    // Task 7 will add a pin-anchor lookup: QPointF pinAnchor(const QString &pinId) const;
};
#endif
