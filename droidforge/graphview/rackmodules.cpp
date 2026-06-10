#include "rackmodules.h"
#include "patch.h"
#include "modulebuilder.h"
#include "registertypes.h"

QList<RackModuleSpec> visibleRackModules(const Patch *, const RackVisibilitySettings &)
{
    return {};
}

RegisterList registersOfModule(const RackModuleSpec &)
{
    return RegisterList();
}

bool registerIsBidirectional(const Patch *, const AtomRegister &)
{
    return false;
}
