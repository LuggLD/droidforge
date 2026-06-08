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
    for (const auto &f : g.frames) scene->addItem(new SectionFrameItem(f));
    QHash<QString, NodeItem*> items;
    for (const auto &n : g.nodes) {
        auto *it = new NodeItem(n);
        it->setPos(n.pos);
        scene->addItem(it);
        items.insert(n.id, it);
    }
    for (const auto &w : g.wires) scene->addItem(new WireItem(w, items));
    scene->setSceneRect(scene->itemsBoundingRect().adjusted(-200, -200, 200, 200));
}

void GraphView::wheelEvent(QWheelEvent *event)
{
    double f = event->angleDelta().y() > 0 ? 1.15 : 1.0/1.15;
    scale(f, f);
}
