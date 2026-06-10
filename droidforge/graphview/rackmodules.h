#ifndef RACKMODULES_H
#define RACKMODULES_H

#include "atomregister.h"
#include "registerlist.h"
#include <QList>
#include <QString>

class Patch;

// One hardware module the rack view currently displays.
struct RackModuleSpec {
    QString  name;          // "master" | "master18" | "g8" | "x7"
    unsigned g8Number  = 0; // "g8" only: bank number (its gates are G<g8Number>.x)
    unsigned rgbOffset = 0; // "g8" only: rack-position offset for its R registers
};

// The two user settings that influence module visibility. App callers read
// them from QSettings ("show_g8s", "show_x7_on_demand"); tests construct
// them directly.
struct RackVisibilitySettings {
    int  showG8s    = 0;     // 0 = only G8s the patch uses
    bool x7OnDemand = false; // false = always show the X7
};

// Which hardware modules the rack view shows, in master→G8s→X7 order.
// MIRRORS RackView::refreshScene (rackview.cpp:408-440) — kept separate
// because the graph must not modify preexisting code (operator rule); if the
// rack rules change upstream, this must be updated to match.
QList<RackModuleSpec> visibleRackModules(const Patch *patch,
                                         const RackVisibilitySettings &vis);

// All registers of one module, canonicalized for graph use: gates are
// round-tripped through their string form (bare G1..G8 -> G1.n) and a g8
// module's R registers get the spec's rack-position offset applied.
RegisterList registersOfModule(const RackModuleSpec &spec);

// True for registers the hardware uses as input OR output depending on the
// patch: exactly the gate jacks on a G8 expander (master16: g8 1..4;
// master18: g8 >= 2 — its built-in g8==1 bank is output-only, like X7's G9+).
bool registerIsBidirectional(const Patch *patch, const AtomRegister &reg);

#endif // RACKMODULES_H
