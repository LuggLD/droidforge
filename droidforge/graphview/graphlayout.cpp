#include "graphlayout.h"
#include "nodeitem.h"
#include <QHash>

namespace {
const double COL_W = 280.0, V_GAP = 30.0, FRAME_PAD = 24.0;
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

    // Node-kind lookup by id (avoids repeated linear findNode() scans below).
    QHash<QString, GraphNodeKind> kindOf;
    for (const auto &n : g.nodes)
        kindOf.insert(n.id, n.kind);

    // Initialise columns: hardware nodes at 0 (sinks are re-pinned to the far
    // right at the end), circuits at 1 so they sit right of source hardware.
    for (const auto &n : g.nodes)
        g_columns[n.id] = (n.kind == GraphNodeKind::Circuit) ? 1 : 0;

    // Longest-path relaxation, capped at (#nodes+1) iterations. Only forward
    // edges into circuits drive layering. We deliberately skip:
    //   - producers that are sinks: an output register read back as an input
    //     (e.g. fold.input = O1) is a visual back-edge, not a layer driver, and
    //     the sink has no meaningful column until it is pinned at the end;
    //   - consumers that are not circuits: sources stay at column 0, sinks are
    //     pinned afterwards. Relaxing them would drag hardware out of place.
    // This keeps every column bounded by the node count regardless of
    // read-backs, feedback loops, or output-to-normalize writes.
    for (qsizetype iter = 0; iter < g.nodes.size() + 1; iter++) {
        bool changed = false;
        for (const auto &w : g.wires) {
            const QString from = pinOwner.value(w.fromPinId);
            const QString to   = pinOwner.value(w.toPinId);
            if (from.isEmpty() || to.isEmpty()) continue;
            if (kindOf.value(from) == GraphNodeKind::HardwareSink) continue;
            if (kindOf.value(to) != GraphNodeKind::Circuit) continue;
            if (g_columns[to] <= g_columns[from]) {
                g_columns[to] = g_columns[from] + 1;
                changed = true;
            }
        }
        if (!changed) break;
    }

    // Pin every sink one column past the rightmost non-sink node.
    int maxCol = 0;
    for (const auto &n : g.nodes)
        if (n.kind != GraphNodeKind::HardwareSink)
            maxCol = qMax(maxCol, g_columns.value(n.id, 0));
    for (const auto &n : g.nodes)
        if (n.kind == GraphNodeKind::HardwareSink)
            g_columns[n.id] = maxCol + 1;

    // Assign positions: stack nodes within each column by their actual heights
    // plus a fixed gap, so tall nodes (many pins) never overlap their neighbours.
    QHash<int,double> yInCol;
    for (auto &n : g.nodes) {
        int col = g_columns.value(n.id, 0);
        double y = yInCol.value(col, 0.0);
        n.pos = QPointF(col * COL_W, y);
        yInCol[col] = y + NodeItem::heightFor(n) + V_GAP;
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
            x1 = qMax(x1, n->pos.x() + NodeItem::NODE_WIDTH);
            y1 = qMax(y1, n->pos.y() + NodeItem::heightFor(*n));
        }
        f.rect = QRectF(x0 - FRAME_PAD, y0 - FRAME_PAD,
                        (x1 - x0) + 2 * FRAME_PAD,
                        (y1 - y0) + 2 * FRAME_PAD);
    }
}

} // namespace GraphLayout
