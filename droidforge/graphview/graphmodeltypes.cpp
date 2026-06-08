#include "graphmodeltypes.h"

const GraphNode *GraphDescription::findNode(const QString &id) const
{
    for (const auto &n : nodes)
        if (n.id == id)
            return &n;
    return nullptr;
}
