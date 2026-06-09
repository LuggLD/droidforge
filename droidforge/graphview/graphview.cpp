#include "graphview.h"
#include "graphmodel.h"
#include "graphlayout.h"
#include "nodeitem.h"
#include "wireitem.h"
#include "sectionframeitem.h"
#include <QGraphicsScene>
#include <QWheelEvent>
#include <QPainter>
#include <QHash>

GraphView::GraphView(PatchEditEngine *patch, QWidget *parent)
    : QGraphicsView(parent), PatchView(patch), scene(new QGraphicsScene(this))
{
    setScene(scene);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);
    rebuildGraphics();
}

void GraphView::rebuildGraphics()
{
    scene->clear();
    GraphDescription g = GraphModel::describe(patch);
    GraphLayout::layout(g);

    // Section frames behind everything else
    for (const auto &f : g.frames)
        scene->addItem(new SectionFrameItem(f));

    // Create all NodeItems first so we can resolve pin anchors
    QHash<QString, NodeItem*> nodeItems;
    for (const auto &n : g.nodes) {
        auto *it = new NodeItem(n);
        it->setPos(n.pos);
        scene->addItem(it);
        nodeItems.insert(n.id, it);
    }

    // Build a scene-space lookup table: pinId → scene position
    // We use NodeItem::pinAnchorLocal() + mapToScene() to get scene coords.
    QHash<QString, QPointF> pinScenePos;
    for (auto it = nodeItems.cbegin(); it != nodeItems.cend(); ++it) {
        NodeItem *ni = it.value();
        for (const GraphPin &p : ni->graphNode().pins) {
            QPointF local = ni->pinAnchorLocal(p.id);
            pinScenePos.insert(p.id, ni->mapToScene(local));
        }
    }

    // Create WireItems with resolved endpoints.
    // Skip wires where either endpoint pin id is missing from the scene-pos map
    // (an empty id or a non-empty id not yet in the map both mean "unresolvable").
    for (const auto &w : g.wires) {
        if (w.fromPinId.isEmpty() || !pinScenePos.contains(w.fromPinId))
            continue;
        if (w.toPinId.isEmpty() || !pinScenePos.contains(w.toPinId))
            continue;
        QPointF from = pinScenePos.value(w.fromPinId);
        QPointF to   = pinScenePos.value(w.toPinId);
        scene->addItem(new WireItem(w, from, to));
    }

    scene->setSceneRect(scene->itemsBoundingRect().adjusted(-200, -200, 200, 200));
}

void GraphView::wheelEvent(QWheelEvent *event)
{
    // Clamp the cumulative zoom so the view can never shrink to nothing (or
    // blow up). Without this, repeated zoom-out drove the scale toward 0 and
    // the graph became unrecoverable.
    constexpr double MIN_SCALE = 0.05, MAX_SCALE = 4.0;
    const double current = transform().m11();
    double f = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    const double target = current * f;
    if (target < MIN_SCALE)      f = MIN_SCALE / current;
    else if (target > MAX_SCALE) f = MAX_SCALE / current;
    scale(f, f);
}
