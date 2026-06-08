#include "graphlayout.h"
#include <QHash>

namespace {
const double COL_W = 280.0, ROW_H = 160.0, FRAME_PAD = 24.0;
}

namespace GraphLayout {

static QHash<QString,int> g_columns;

int columnOf(const GraphDescription &, const QString &nodeId)
{
    return g_columns.value(nodeId, 0);
}

void layout(GraphDescription &g)
{
    g_columns.clear();

    // Build pin-id → owning-node-id map by scanning every node's pins.
    QHash<QString,QString> pinOwner;
    for (const auto &n : g.nodes)
        for (const auto &p : n.pins)
            pinOwner.insert(p.id, n.id);

    const int SINK_COL = 1000000;

    // Initialise columns.
    for (const auto &n : g.nodes) {
        if (n.kind == GraphNodeKind::HardwareSource)
            g_columns[n.id] = 0;
        else if (n.kind == GraphNodeKind::HardwareSink)
            g_columns[n.id] = SINK_COL;
        else
            g_columns[n.id] = 1;
    }

    // Relax along wires (longest-path), capped at (#nodes+1) iterations.
    for (qsizetype iter = 0; iter < g.nodes.size() + 1; iter++) {
        bool changed = false;
        for (const auto &w : g.wires) {
            QString from = pinOwner.value(w.fromPinId);
            QString to   = pinOwner.value(w.toPinId);
            if (from.isEmpty() || to.isEmpty()) continue;
            const GraphNode *toNode = g.findNode(to);
            if (toNode && toNode->kind == GraphNodeKind::HardwareSink) continue;
            if (g_columns[to] <= g_columns[from]) {
                g_columns[to] = g_columns[from] + 1;
                changed = true;
            }
        }
        if (!changed) break;
    }

    // Determine max non-sentinel column, then fix sink columns.
    int maxCol = 0;
    for (auto it = g_columns.begin(); it != g_columns.end(); ++it)
        if (it.value() != SINK_COL)
            maxCol = qMax(maxCol, it.value());
    for (const auto &n : g.nodes)
        if (n.kind == GraphNodeKind::HardwareSink)
            g_columns[n.id] = maxCol + 1;

    // Assign positions: stack nodes within each column.
    QHash<int,int> rowInCol;
    for (auto &n : g.nodes) {
        int col = g_columns.value(n.id, 0);
        int row = rowInCol.value(col, 0);
        rowInCol[col] = row + 1;
        n.pos = QPointF(col * COL_W, row * ROW_H);
    }

    // Compute frame rects bounding member nodes.
    for (auto &f : g.frames) {
        if (f.nodeIds.isEmpty()) continue;
        double x0 = 1e18, y0 = 1e18, x1 = -1e18, y1 = -1e18;
        for (const auto &id : f.nodeIds) {
            const GraphNode *n = g.findNode(id);
            if (!n) continue;
            x0 = qMin(x0, n->pos.x());
            y0 = qMin(y0, n->pos.y());
            x1 = qMax(x1, n->pos.x() + 220.0);
            y1 = qMax(y1, n->pos.y() + 120.0);
        }
        f.rect = QRectF(x0 - FRAME_PAD, y0 - FRAME_PAD,
                        (x1 - x0) + 2 * FRAME_PAD,
                        (y1 - y0) + 2 * FRAME_PAD);
    }
}

} // namespace GraphLayout
