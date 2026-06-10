#include "graphview.h"
#include "graphmodel.h"
#include "graphlayout.h"
#include "rackmodules.h"
#include "nodeitem.h"
#include "wireitem.h"
#include "sectionframeitem.h"
#include <QGraphicsScene>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <cmath>
#include <QGraphicsPathItem>
#include <QPen>
#include <QPainterPath>
#include <QPainter>
#include <QHash>
#include <QKeyEvent>
#include <QMenu>
#include <QContextMenuEvent>
#include <QSettings>

GraphView::GraphView(PatchEditEngine *patch, QWidget *parent)
    : QGraphicsView(parent), PatchView(patch), scene(new QGraphicsScene(this))
{
    setScene(scene);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse); // zoom toward cursor
    // The graph owns its background. Without this the canvas inherits the OS
    // palette (white in macOS light mode), where the hand-picked node/wire
    // colors — designed dark-first like most node editors — become illegible.
    // Hardcoded rather than COLOR(): the graphshot harness has no colorscheme.
    scene->setBackgroundBrush(QColor(30, 30, 30));
    rebuildGraphics();
}

void GraphView::rebuildGraphics()
{
    // Rebuilds can arrive mid-drag (hub broadcasts from other views, View-menu
    // toggles, colorscheme changes). scene->clear() would delete the live
    // rubber item under us — tear the drag down first.
    if (dragging)
        endDrag();
    scene->clear();
    // Same settings the rack view honors; written by RackView::showG8s/showX7
    // before our slot runs (its action connections predate ours).
    QSettings settings;
    RackVisibilitySettings vis;
    vis.showG8s    = settings.value("show_g8s", 0).toInt();
    vis.x7OnDemand = settings.value("show_x7_on_demand").toBool();
    GraphDescription g = GraphModel::describe(patch, vis);
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
        if (event->modifiers() & Qt::AltModifier) {
            const QString pin = pinAtScene(mapToScene(event->pos()));
            if (!pin.isEmpty()) {
                commitEdit(GraphEdits::disconnectPin(patch, pin), tr("disconnect all"));
                event->accept();
                return;
            }
        }
        const QPointF scenePos = mapToScene(event->pos());
        const QString pin = pinAtScene(scenePos);
        if (!pin.isEmpty()) {
            GraphEdits::PinRef ref = GraphEdits::parsePin(patch, pin);
            dragMode = modeFor(ref, event->modifiers());
            dragFromPin = pin;
            // Anchor the preview at the *fixed* end(s) of what's being dragged,
            // so the rubber-band reads as the user's intent (not pin->same-kind):
            //  - copy/move from a sink: the source currently feeding that sink
            //  - re-home from a source: every sink the source currently feeds
            //  - plain connect: the dragged pin itself
            dragAnchors.clear();
            if (dragMode == GraphEdits::DragMode::Copy
                || dragMode == GraphEdits::DragMode::Move) {
                const QString src = GraphEdits::getConnectedSource(patch, pin);
                dragAnchors.append(src.isEmpty() ? pinScenePos(pin) : pinScenePos(src));
            }
            else if (dragMode == GraphEdits::DragMode::Rehome) {
                const QStringList sinks = GraphEdits::getConnectedSinks(patch, pin);
                for (const QString &s : sinks)
                    dragAnchors.append(pinScenePos(s));
                if (dragAnchors.isEmpty())
                    dragAnchors.append(pinScenePos(pin));
            }
            else {
                dragAnchors.append(pinScenePos(pin));
            }
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
        const QPointF scp = mapToScene(event->pos());
        for (QGraphicsItem *it : scene->items(scp)) {
            if (auto *wi = dynamic_cast<WireItem *>(it)) {
                scene->clearSelection();
                wi->setSelected(true);
                viewport()->update();
                event->accept();
                return;
            }
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
        for (const QPointF &anchor : dragAnchors)
            path.addPath(WireItem::curve(anchor, scenePos));
        rubber->setPath(path);

        const bool ok = !target.isEmpty()
                        && GraphEdits::isValidDrop(patch, dragFromPin, target, dragMode);
        viewport()->setCursor(target.isEmpty() ? Qt::ArrowCursor
                              : ok ? Qt::PointingHandCursor : Qt::ForbiddenCursor);
        event->accept();
        return;
    }
    // Hover feedback: pins are interactive (wire drags start there) — show a
    // pointing hand instead of ScrollHandDrag's grab hand. Only while no
    // button is down, so we never fight the closed-hand cursor mid-pan.
    if (!event->buttons()) {
        const bool overPin = !pinAtScene(mapToScene(event->pos())).isEmpty();
        viewport()->setCursor(overPin ? Qt::PointingHandCursor : Qt::OpenHandCursor);
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
    emit patchModified();   // hub fans out; our rebuild returns via the hub
}

void GraphView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        const QList<QGraphicsItem *> sel = scene->selectedItems();
        bool any = false;
        for (QGraphicsItem *it : sel) {
            if (auto *wi = dynamic_cast<WireItem *>(it)) {
                const GraphWire &w = wi->graphWire();
                if (GraphEdits::disconnectWire(patch, w.fromPinId, w.toPinId))
                    any = true;
            }
        }
        if (any) {
            commitEdit(true, tr("delete wire"));
            event->accept();
            return;
        }
    }
    if (event->key() == Qt::Key_Escape && dragging) {
        endDrag(); // cancel an in-progress wire drag
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void GraphView::contextMenuEvent(QContextMenuEvent *event)
{
    if (dragging) { // never open a menu mid-drag (would strand the rubber-band)
        event->ignore();
        return;
    }
    const QPointF scenePos = mapToScene(event->pos());

    // Wire under the cursor? Offer Delete.
    for (QGraphicsItem *it : scene->items(scenePos)) {
        if (auto *wi = dynamic_cast<WireItem *>(it)) {
            const GraphWire w = wi->graphWire();
            QMenu menu(this);
            QAction *del = menu.addAction(tr("Delete connection"));
            if (menu.exec(event->globalPos()) == del)
                commitEdit(GraphEdits::disconnectWire(patch, w.fromPinId, w.toPinId), tr("delete wire"));
            event->accept();
            return;
        }
    }

    // Pin under the cursor? Offer per-wire + "Disconnect all".
    const QString pin = pinAtScene(scenePos);
    if (!pin.isEmpty()) {
        GraphEdits::PinRef ref = GraphEdits::parsePin(patch, pin);
        QMenu menu(this);
        QList<QPair<QString, QString>> targets; // (label, otherPin) for disconnectWire
        if (ref.isSource()) {
            const QStringList sinks = GraphEdits::getConnectedSinks(patch, pin);
            for (const QString &s : sinks)
                targets.append({tr("Disconnect from %1").arg(s), s});
        } else {
            const QString src = GraphEdits::getConnectedSource(patch, pin);
            if (!src.isEmpty())
                targets.append({tr("Disconnect from %1").arg(src), src});
        }
        QList<QAction *> acts;
        for (const auto &t : targets)
            acts.append(menu.addAction(t.first));
        QAction *all = nullptr;
        if (!targets.isEmpty()) {
            menu.addSeparator();
            all = menu.addAction(tr("Disconnect all"));
        }
        if (menu.isEmpty()) { event->accept(); return; }
        QAction *chosen = menu.exec(event->globalPos());
        if (chosen) {
            bool ok = false;
            if (chosen == all) {
                ok = GraphEdits::disconnectPin(patch, pin);
            } else {
                const int idx = acts.indexOf(chosen);
                if (idx >= 0)
                    ok = GraphEdits::disconnectWire(patch, pin, targets[idx].second);
            }
            commitEdit(ok, tr("disconnect"));
        }
        event->accept();
        return;
    }

    QGraphicsView::contextMenuEvent(event);
}

void GraphView::applyZoom(double factor)
{
    // Clamp the cumulative zoom so the view can never shrink to nothing (or
    // blow up). Without this, repeated zoom-out drove the scale toward 0 and
    // the graph became unrecoverable.
    constexpr double MIN_SCALE = 0.05, MAX_SCALE = 4.0;
    const double current = transform().m11();
    const double target = current * factor;
    if (target < MIN_SCALE)      factor = MIN_SCALE / current;
    else if (target > MAX_SCALE) factor = MAX_SCALE / current;
    scale(factor, factor);
}

void GraphView::wheelEvent(QWheelEvent *event)
{
    // Exponential zoom scaled by the actual wheel delta: ~7% per mouse-wheel
    // notch (angleDelta 120), proportionally less for the fine-grained events
    // trackpad scrolling emits. The old fixed 15% per event ignored the delta
    // magnitude and made trackpads zoom wildly.
    const double dy = event->angleDelta().y();
    if (dy != 0.0)
        applyZoom(std::pow(2.0, dy / 1200.0));
}

bool GraphView::viewportEvent(QEvent *event)
{
    // Trackpad pinch (macOS delivers it as a native zoom gesture on the
    // viewport): value() is the incremental magnification per event.
    if (event->type() == QEvent::NativeGesture) {
        auto *gesture = static_cast<QNativeGestureEvent *>(event);
        if (gesture->gestureType() == Qt::ZoomNativeGesture) {
            applyZoom(1.0 + gesture->value());
            return true;
        }
    }
    return QGraphicsView::viewportEvent(event);
}
