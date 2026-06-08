#ifndef WIREITEM_H
#define WIREITEM_H
#include "graphmodeltypes.h"
#include <QGraphicsItem>
#include <QPointF>

// WireItem receives its two endpoints as scene-space coordinates.
// GraphView::rebuildGraphics() resolves them from NodeItem::pinAnchorLocal()
// before constructing WireItems.
class WireItem : public QGraphicsItem {
    GraphWire wire;
    QPointF fromPt; // scene coords, endpoint at producer
    QPointF toPt;   // scene coords, endpoint at consumer
    bool valid;     // false → skip drawing

public:
    // from/to are scene-space; pass QPointF() to flag a missing endpoint.
    explicit WireItem(const GraphWire &w, const QPointF &from, const QPointF &to);
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
};
#endif
