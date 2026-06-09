#include "graphlayout.h"
#include "nodeitem.h"
#include <QHash>
#include <cmath>

namespace {
const double COL_GAP     = 80.0;  // between a hardware column and the section band
const double H_GAP       = 40.0;  // between grid columns within a section
const double V_GAP       = 30.0;  // between stacked nodes / grid rows
const double SECTION_GAP = 60.0;  // between section bands (must exceed 2*FRAME_PAD)
const double FRAME_PAD   = 24.0;
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

    // Coarse role columns, only for columnOf()/tests: source=0, circuit=1, sink=2.
    for (const auto &n : g.nodes) {
        int col = 1;
        if (n.kind == GraphNodeKind::HardwareSource)    col = 0;
        else if (n.kind == GraphNodeKind::HardwareSink) col = 2;
        g_columns[n.id] = col;
    }

    // 1. Source hardware -> far-left column, stacked by actual height.
    double y = 0.0;
    for (auto &n : g.nodes) {
        if (n.kind != GraphNodeKind::HardwareSource) continue;
        n.pos = QPointF(0.0, y);
        y += NodeItem::heightFor(n) + V_GAP;
    }

    const double sectionsX = NodeItem::NODE_WIDTH + COL_GAP;

    // 2. Each section -> a compact grid; section bands stacked vertically.
    double sectionY = 0.0;
    double maxSectionRight = sectionsX;
    for (int s = 0; s < g.frames.size(); s++) {
        QList<GraphNode *> circuits;
        for (auto &n : g.nodes)
            if (n.kind == GraphNodeKind::Circuit && n.sectionIndex == s)
                circuits.append(&n);
        if (circuits.isEmpty()) continue;

        const int cnt  = circuits.size();
        const int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(cnt))));

        double rowY = sectionY;
        int idx = 0;
        while (idx < cnt) {
            double rowH = 0.0;
            for (int j = 0; j < cols && idx + j < cnt; j++)
                rowH = qMax(rowH, NodeItem::heightFor(*circuits[idx + j]));
            for (int j = 0; j < cols && idx + j < cnt; j++) {
                const double x = sectionsX + j * (NodeItem::NODE_WIDTH + H_GAP);
                circuits[idx + j]->pos = QPointF(x, rowY);
                maxSectionRight = qMax(maxSectionRight, x + NodeItem::NODE_WIDTH);
            }
            rowY += rowH + V_GAP;
            idx  += cols;
        }
        sectionY = rowY + SECTION_GAP;
    }

    // 3. Output hardware -> far-right column, stacked.
    const double sinkX = maxSectionRight + COL_GAP;
    y = 0.0;
    for (auto &n : g.nodes) {
        if (n.kind != GraphNodeKind::HardwareSink) continue;
        n.pos = QPointF(sinkX, y);
        y += NodeItem::heightFor(n) + V_GAP;
    }

    // 4. Frame rect = padded bounding box of each section's circuits. Because
    // section bands are stacked with SECTION_GAP > 2*FRAME_PAD, frames are
    // pairwise disjoint.
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
