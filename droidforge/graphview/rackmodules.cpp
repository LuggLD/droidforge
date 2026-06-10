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

RegisterList registersOfModule(const RackModuleSpec &spec)
{
    RegisterList raw;
    ModuleBuilder::allRegistersOf(spec.name, 0, spec.g8Number, raw);

    RegisterList out;
    for (const AtomRegister &reg : raw) {
        if (reg.getRegisterType() == REGISTER_GATE)
            // Canonicalize via the string parser: ModuleMaster18 emits bare
            // G1..G4 (g8=0), but the canonical patch form is G1.1..G1.4 — the
            // exact mismatch behind the 2026-06-10 vanishing-wire bug. (For
            // g8/x7 gates the round-trip is a no-op; the uniform path is
            // deliberate — don't "optimize" it away, master18 breaks.)
            out.append(AtomRegister(reg.toString()));
        else if (spec.name == QStringLiteral("g8")
                 && reg.getRegisterType() == REGISTER_RGB_LED)
            // allRegistersOf can't know the rack-position R offset (it is
            // injected by RackView::addModule, not the module type).
            out.append(AtomRegister(REGISTER_RGB_LED, 0, 0,
                                    reg.getNumber() + spec.rgbOffset));
        else
            out.append(reg);
    }
    return out;
}

bool registerIsBidirectional(const Patch *patch, const AtomRegister &reg)
{
    if (reg.getRegisterType() != REGISTER_GATE)
        return false;
    const unsigned g8 = reg.getG8Number();
    if (g8 == 0)
        return false;                          // X7 gates (G9+) are output-only
    if (patch->typeOfMaster() == 18 && g8 == 1)
        return false;                          // MASTER18 built-in gate outputs
    return true;                               // a G8 expander jack
}
