#ifndef GRAPHMODEL_H
#define GRAPHMODEL_H

#include "graphmodeltypes.h"
class Patch;

// Pure derivation: turns a Patch into a GraphDescription. No GUI, no positions.
namespace GraphModel {
    GraphDescription describe(const Patch *patch);
}

#endif // GRAPHMODEL_H
