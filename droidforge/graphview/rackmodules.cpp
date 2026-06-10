#include "rackmodules.h"
#include "patch.h"
#include "modulebuilder.h"
#include "registertypes.h"

QList<RackModuleSpec> visibleRackModules(const Patch *patchConst,
                                         const RackVisibilitySettings &vis)
{
    // highestGatePrefix()/needsX7() are logically const (they only scan atoms).
    Patch *patch = const_cast<Patch *>(patchConst);

    QList<RackModuleSpec> mods;
    const unsigned masterType = patch->typeOfMaster();
    mods.append({masterType == 18 ? QStringLiteral("master18") : QStringLiteral("master"), 0, 0});

    // MIRRORS rackview.cpp:408-440 (exact expression at :412): any non-16
    // master provides built-in gates in g8 bank 1, so external G8 expanders
    // are numbered from 2.
    const int g8Offset = masterType != 16 ? 1 : 0;
    const int showG8s = qMax(vis.showG8s,
                             static_cast<int>(patch->highestGatePrefix()) - g8Offset);
    for (int g = 1; g <= showG8s; g++)
        mods.append({QStringLiteral("g8"),
                     static_cast<unsigned>(g + g8Offset),
                     static_cast<unsigned>(8 + g * 8)});

    if (!vis.x7OnDemand || patch->needsX7())
        mods.append({QStringLiteral("x7"), 0, 0});
    return mods;
}

RegisterList registersOfModule(const RackModuleSpec &)
{
    return RegisterList();
}

bool registerIsBidirectional(const Patch *, const AtomRegister &)
{
    return false;
}
