#ifndef GRAPHLAYOUT_H
#define GRAPHLAYOUT_H
#include "graphmodeltypes.h"
namespace GraphLayout {
    // Assigns node.pos and frame.rect. Columns: hardware sources = leftmost,
    // circuits layered by signal flow (longest path from sources), hardware sinks = rightmost.
    void layout(GraphDescription &g);
    int columnOf(const GraphDescription &g, const QString &nodeId); // exposed for testing
}
#endif
