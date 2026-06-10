#ifndef GRAPHMODEL_H
#define GRAPHMODEL_H

#include "graphmodeltypes.h"
#include "rackmodules.h"
class Patch;

// Pure derivation: turns a Patch into a GraphDescription. No GUI, no positions.
// The visibility settings select which rack hardware modules get nodes; the
// default mirrors the app's QSettings defaults (used G8s only, X7 always).
namespace GraphModel {
    GraphDescription describe(const Patch *patch,
                              const RackVisibilitySettings &vis = RackVisibilitySettings());
}

#endif // GRAPHMODEL_H
