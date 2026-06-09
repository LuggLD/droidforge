#include "graphview.h"
#include "graphmodel.h"
#include "graphlayout.h"
#include "nodeitem.h"
#include "wireitem.h"
#include "sectionframeitem.h"
#include <QGraphicsScene>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QGraphicsPathItem>
#include <QPen>
#include <QPainterPath>
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

QString GraphView::pinAtScene(const QPointF &scenePos) const
{
    const QList<QGraphicsItem *> items = scene->items(scenePos);
    for (QGraphicsItem *it : items) {
        if (auto *ni = dynamic_cast<NodeItem *>(it)) {
            const QString pin = ni->pinAt(ni->mapFromScene(scenePos));
            if (!pin.isEmpty())
                return pin;
        }
    }
    // Fall back to a small search around the point (connectors sit on node edges).
    const QList<QGraphicsItem *> near =
        scene->items(QRectF(scenePos.x() - 8, scenePos.y() - 8, 16, 16));
    for (QGraphicsItem *it : near) {
        if (auto *ni = dynamic_cast<NodeItem *>(it)) {
            const QString pin = ni->pinAt(ni->mapFromScene(scenePos));
            if (!pin.isEmpty())
                return pin;
        }
    }
    return QString();
}

QPointF GraphView::pinScenePos(const QString &pinId) const
{
    for (QGraphicsItem *it : scene->items()) {
        if (auto *ni = dynamic_cast<NodeItem *>(it)) {
            for (const GraphPin &p : ni->graphNode().pins) {
                if (p.id == pinId)
                    return ni->mapToScene(ni->pinAnchorLocal(pinId));
            }
        }
    }
    return QPointF();
}

void GraphView::endDrag()
{
    if (rubber) { scene->removeItem(rubber); delete rubber; rubber = nullptr; }
    dragging = false;
    dragFromPin.clear();
    viewport()->setCursor(Qt::ArrowCursor);
    setDragMode(QGraphicsView::ScrollHandDrag);
}

static GraphEdits::DragMode modeFor(const GraphEdits::PinRef &ref, Qt::KeyboardModifiers mods)
{
    using M = GraphEdits::DragMode;
    if (ref.isSource())
        return (mods & Qt::ControlModifier) ? M::Rehome : M::Connect;
    // sink
    if (mods & Qt::ShiftModifier)   return M::Copy;
    if (mods & Qt::ControlModifier) return M::Move;
    return M::Connect;
}

void GraphView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (dragging)
            endDrag(); // cancel any stuck/active drag before starting a new one
        const QPointF scenePos = mapToScene(event->pos());
        const QString pin = pinAtScene(scenePos);
        if (!pin.isEmpty()) {
            GraphEdits::PinRef ref = GraphEdits::parsePin(patch, pin);
            dragMode = modeFor(ref, event->modifiers());
            dragFromPin = pin;
            dragFromScenePos = pinScenePos(pin);
            dragging = true;
            setDragMode(QGraphicsView::NoDrag); // suspend pan while wiring
            rubber = new QGraphicsPathItem();
            QPen pen(QColor(255, 255, 255, 200));
            pen.setWidthF(1.5);
            pen.setStyle(Qt::DashLine);
            rubber->setPen(pen);
            rubber->setZValue(10);
            scene->addItem(rubber);
            event->accept();
            return;
        }
    }
    QGraphicsView::mousePressEvent(event);
}

void GraphView::mouseMoveEvent(QMouseEvent *event)
{
    if (dragging) {
        const QPointF scenePos = mapToScene(event->pos());
        const QString target = pinAtScene(scenePos);
        QPainterPath path;
        path.moveTo(dragFromScenePos);
        path.lineTo(scenePos);
        rubber->setPath(path);

        const bool ok = !target.isEmpty()
                        && GraphEdits::isValidDrop(patch, dragFromPin, target, dragMode);
        viewport()->setCursor(target.isEmpty() ? Qt::ArrowCursor
                              : ok ? Qt::PointingHandCursor : Qt::ForbiddenCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void GraphView::mouseReleaseEvent(QMouseEvent *event)
{
    if (dragging && event->button() == Qt::LeftButton) {
        const QPointF scenePos = mapToScene(event->pos());
        const QString target = pinAtScene(scenePos);
        const QString from = dragFromPin;
        const GraphEdits::DragMode mode = dragMode;

        // tear down preview first
        endDrag();

        if (!target.isEmpty() && GraphEdits::isValidDrop(patch, from, target, mode)) {
            bool ok = false;
            QString msg;
            switch (mode) {
            case GraphEdits::DragMode::Connect:
                ok = GraphEdits::connectPins(patch, from, target); msg = tr("connect wire"); break;
            case GraphEdits::DragMode::Copy:
                ok = GraphEdits::copyWire(patch, from, target);    msg = tr("copy wire"); break;
            case GraphEdits::DragMode::Move:
                ok = GraphEdits::moveWire(patch, from, target);    msg = tr("move wire"); break;
            case GraphEdits::DragMode::Rehome:
                ok = GraphEdits::rehomeWires(patch, from, target); msg = tr("re-home wires"); break;
            }
            commitEdit(ok, msg);
        }
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void GraphView::commitEdit(bool ok, const QString &message)
{
    if (!ok)
        return;
    patch->commit(message);
    rebuildGraphics();
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
