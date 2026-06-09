#ifndef NODEITEM_H
#define NODEITEM_H
#include "graphmodeltypes.h"
#include <QGraphicsItem>
#include <QHash>
#include <QPointF>

class NodeItem : public QGraphicsItem {
    GraphNode node;

    // Computed during layout (constructed once, used by paint and anchor queries)
    mutable QHash<QString, QPointF> pinAnchors; // local coords, connector center
    mutable QRectF cachedRect;
    mutable bool layoutDirty = true;

    void doLayout() const;

public:
    // Visual metrics
    static constexpr qreal NODE_WIDTH        = 220.0;
    static constexpr qreal TITLE_HEIGHT      = 22.0;
    static constexpr qreal ROW_HEIGHT        = 20.0;
    static constexpr qreal SECONDARY_HEIGHT  = 16.0; // Scale / Offset sub-rows
    static constexpr qreal CONNECTOR_RADIUS  = 5.0;
    static constexpr qreal CONNECTOR_SQUARE  = 8.0;
    static constexpr qreal H_PADDING         = 8.0;

    explicit NodeItem(const GraphNode &n);
    const GraphNode &graphNode() const { return node; }

    // Total painted height of a node with these pins. Shared with GraphLayout so
    // vertical stacking matches the real geometry (no overlap). Single source of
    // truth for node height.
    static qreal heightFor(const GraphNode &n);

    // Returns anchor in LOCAL item coordinates; call mapToScene() for scene coords.
    QPointF pinAnchorLocal(const QString &pinId) const;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
};
#endif
