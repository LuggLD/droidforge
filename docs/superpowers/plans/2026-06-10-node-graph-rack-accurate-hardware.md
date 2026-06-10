# Rack-Accurate Hardware Nodes + Rack Reconciliation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The node graph's hardware nodes become per-module and rack-accurate (MASTER vs MASTER18, G8 expanders with bidirectional jacks, X7, RGB/X registers), and graph commits broadcast through the UpdateHub so the rack auto-shows/hides modules on graph edits.

**Architecture:** A new pure unit `graphview/rackmodules.{h,cpp}` mirrors the rack view's module-visibility formula and classifies bidirectional G8 jacks; `GraphModel` builds one source/sink node pair per visible module from `ModuleBuilder` enumeration; `GraphView` emits `patchModified` after commits and the existing M1 integration block in `mainwindow.cpp` wires it to the `UpdateHub`. Spec: `docs/superpowers/specs/2026-06-10-node-graph-rack-accurate-hardware-design.md`.

**Tech Stack:** Qt 6 / C++17. App build: CMake in `droidforge/build`. Tests: gitignored QTest harness in `droidforge/tests` → `droidforge/build-tests`.

---

## Standing rules (read first)

- **Purely additive (operator hard rule):** do NOT modify preexisting droidforge code (`patch/`, `rackview/`, `main/` except the graph block, `modules/`). The only existing files we touch — `main/mainwindow.cpp` (graph integration block at ~line 120) and `droidforge/CMakeLists.txt` (graphview source block at ~line 274) — are extended **inside blocks this feature branch added in M1**. Everything else is `graphview/` or the gitignored harness.
- **Command hygiene:** one command per Bash call. No `&&`/`;` compounds, no `$(...)` substitution (commit messages use two `-m` flags), absolute paths.
- **Gitignored files:** everything under `droidforge/tests/` is gitignored — edit it, never `git add` it. Commits contain only `droidforge/graphview/*`, `droidforge/CMakeLists.txt`, `droidforge/main/mainwindow.cpp`, and docs.
- **Build/test commands:**
  - Tests: `cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests` then `/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests -platform offscreen`
  - App: `cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build`
  - After adding files to a CMakeLists, re-run the configure step once: `cmake -S /Users/jan.kaluza/Projects/droidforge/droidforge -B /Users/jan.kaluza/Projects/droidforge/droidforge/build` (and the tests equivalent with `-S .../droidforge/tests -B .../droidforge/build-tests`).

## Verified facts you must not re-derive

- `register_types[]` order is I, P, B, S, E, G, N, O, L, R, X (`patch/registertypes.cpp:8`). `Module::collectAllRegisters` iterates that order.
- Real inventories (`modules/module*.cpp`): master = I×8, N×8, O×8, R×16, X×1; master18 = I×2, G×4 **bare `G1`–`G4` (g8=0!)**, O×8; g8 = G×8 (with its g8Number) + R×8 **without** rack offset (`DATA_INDEX_G8_RGB_OFFSET` is never set by `allRegistersOf`); x7 = G9–G12 + R49–R56 (offsets baked into `ModuleX7::numberOffset`).
- `AtomRegister(QString)` normalizes bare `G1`–`G8` to `g8=1` ("G1.n"); `G9+` stay `g8=0`. Round-trip all gates from ModuleBuilder through the string form.
- Rack formula (`rackview/rackview.cpp:408–440`): `g8_offset = typeOfMaster()!=16 ? 1 : 0`; `show_g8s = qMax(QSettings("show_g8s",0).toInt(), highestGatePrefix() − g8_offset)`; G8 #g added with `g8Number = g+g8_offset`, `rgbOffset = 8+g*8`; X7 shown iff `!x7OnDemand || patch->needsX7()`.
- `Patch::highestGatePrefix()` and `Patch::needsX7()` are **non-const** (`patch.h:96–97`); use the same `const_cast` pattern as `graphmodel.cpp:145` (logically const).
- `Patch::registerIsOutputOnly` (`patch.cpp:688`): N/O/L/R/X true; gates: `getNumber()>=9` true, `g8Number==1 && typeOfMaster()==18` true, else false.
- Settings are written **before** the graph slot runs: `RackView` connects the show-G8/X7 actions in its constructor (`rackview.cpp:49–53`) and its handlers write QSettings (`rackview.cpp:87–108`); our MainWindow-body connects run later in connection order.
- `mainwindow.h:92` `theActions()`, `:94` `theHub()` are public. The hub→graph rebuild connection already exists (`mainwindow.cpp:121`).

## File structure

| File | Change |
|---|---|
| `droidforge/graphview/rackmodules.h` | **Create.** `RackModuleSpec`, `RackVisibilitySettings`, `visibleRackModules()`, `registersOfModule()`, `registerIsBidirectional()` |
| `droidforge/graphview/rackmodules.cpp` | **Create.** Implementations (mirrors rack formula; wraps ModuleBuilder) |
| `droidforge/graphview/graphmodel.h` | `describe()` gains a `RackVisibilitySettings` parameter (defaulted) |
| `droidforge/graphview/graphmodel.cpp` | Rewrite `addHardwareNodes`; new `appendRegisterPins(in,out,…)`; `hwReadPinId` learns bidirectional |
| `droidforge/graphview/graphedits.cpp` | `parsePin` + `getConnectedSource`: bidirectional gates classify as HwWrite / `.read` source |
| `droidforge/graphview/graphview.h` | Add `signals: void patchModified();` |
| `droidforge/graphview/graphview.cpp` | `commitEdit` emits instead of rebuilding; `rebuildGraphics` reads QSettings → vis |
| `droidforge/main/mainwindow.cpp` | Our M1 block: graph→hub connect + 7 action→rebuild connects |
| `droidforge/CMakeLists.txt` | Our graphview block: add `rackmodules.{h,cpp}` |
| `droidforge/tests/*` (gitignored) | Stub upgrade, CMake additions, `test_rackmodules.cpp` (new), runner entry, test updates |

---

### Task 1: Harness plumbing (stub upgrade + build files)

**Files:**
- Modify: `droidforge/tests/test_stubs.cpp` (gitignored)
- Modify: `droidforge/tests/CMakeLists.txt` (gitignored)
- Modify: `droidforge/tests/test_runner.cpp` (gitignored)
- Create: `droidforge/tests/test_rackmodules.cpp` (gitignored, empty shell)
- Create: `droidforge/graphview/rackmodules.h` + `droidforge/graphview/rackmodules.cpp` (skeletons so the build links)

- [ ] **Step 1: Upgrade the ModuleBuilder stub to mirror the real inventories**

Replace the no-op `allRegistersOf` in `droidforge/tests/test_stubs.cpp` (keep the rest of the file):

```cpp
// modules/*.cpp is excluded (module.cpp drags in mainwindow.h -> macmidihost.h).
// This stub mirrors Module::collectAllRegisters + the per-module numRegisters/
// registerAtom/numberOffset of modules/module{master,master18,g8,x7}.cpp:
//  - register_types[] iteration order (I,P,B,S,E,G,N,O,L,R,X)
//  - master18 gates come out BARE (G1..G4, g8=0) like the real ModuleMaster18
//  - g8 RGB has NO rack-position offset (DATA_INDEX_G8_RGB_OFFSET unset there)
//  - x7 offsets are baked in (gates +8, RGB +48)
#include "modulebuilder.h"
#include "registerlist.h"
#include "registertypes.h"
void ModuleBuilder::allRegistersOf(QString name, unsigned controller, unsigned g8, RegisterList &rl)
{
    Q_UNUSED(controller);
    auto count = [&name](register_type_t t) -> unsigned {
        if (name == "master")
            return (t == REGISTER_INPUT || t == REGISTER_OUTPUT || t == REGISTER_NORMALIZE) ? 8
                 : (t == REGISTER_RGB_LED) ? 16 : (t == REGISTER_EXTRA) ? 1 : 0;
        if (name == "master18")
            return (t == REGISTER_INPUT) ? 2 : (t == REGISTER_OUTPUT) ? 8
                 : (t == REGISTER_GATE) ? 4 : 0;
        if (name == "g8")
            return (t == REGISTER_GATE || t == REGISTER_RGB_LED) ? 8 : 0;
        if (name == "x7")
            return (t == REGISTER_GATE) ? 4 : (t == REGISTER_RGB_LED) ? 8 : 0;
        return 0;
    };
    auto offset = [&name](register_type_t t) -> unsigned {
        if (name == "x7")
            return (t == REGISTER_GATE) ? 8 : (t == REGISTER_RGB_LED) ? 48 : 0;
        return 0;
    };
    for (unsigned i = 0; i < NUM_REGISTER_TYPES; i++) {
        register_type_t t = register_types[i];
        unsigned g8n = (name == "g8" && t == REGISTER_GATE) ? g8 : 0;
        for (unsigned n = 1; n <= count(t); n++)
            rl.append(AtomRegister(t, 0, g8n, n + offset(t)));
    }
}
bool ModuleBuilder::controllerExists(QString) { return false; }
const QStringList &ModuleBuilder::allControllers()
{
    static const QStringList empty;
    return empty;
}
```

(The existing `#include "modulebuilder.h"` / `#include "registerlist.h"` lines and the two other stub functions are shown above so the final file state is unambiguous; `ColorScheme` stub at the top stays.)

- [ ] **Step 2: Create the rackmodules skeleton**

`droidforge/graphview/rackmodules.h`:

```cpp
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
```

`droidforge/graphview/rackmodules.cpp` (skeleton — real bodies come via TDD in Tasks 2–4):

```cpp
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
```

- [ ] **Step 3: Add rackmodules to both harness targets and the new test TU**

In `droidforge/tests/CMakeLists.txt` add `${SRC_ROOT}/graphview/rackmodules.cpp` to **both** `add_executable` source lists (forgetests and graphshot), and add `test_rackmodules.cpp` to forgetests only:

```cmake
add_executable(forgetests
    test_stubs.cpp
    test_runner.cpp
    test_graphmodel.cpp
    test_graphedits.cpp
    test_rackmodules.cpp
    ${SRC_ROOT}/graphview/graphmodeltypes.cpp
    ${SRC_ROOT}/graphview/nodeitem.cpp
    ${SRC_ROOT}/graphview/graphedits.cpp
    ${SRC_ROOT}/graphview/rackmodules.cpp
    ${GRAPH_SRC} ${MODEL_SRC} ${PATCHVIEW_SRC} ${SRC_ROOT}/resources.qrc)
```

and in the graphshot target add the one line `${SRC_ROOT}/graphview/rackmodules.cpp` after `${SRC_ROOT}/graphview/sectionframeitem.cpp`.

Create `droidforge/tests/test_rackmodules.cpp`:

```cpp
// Tests for graphview/rackmodules.{h,cpp}
// This file is gitignored — never committed.

#include <QtTest>
#include "patch.h"
#include "patchparser.h"
#include "rackmodules.h"
#include "droidfirmware.h"

extern DroidFirmware *the_firmware;

static Patch *parse(const QString &src) {
    Patch *p = new Patch();
    PatchParser parser;
    parser.parseString(src, p);
    return p;
}

class TestRackModules : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { if (!the_firmware) the_firmware = new DroidFirmware(); }

    void placeholder() { QVERIFY(true); }
};

#include "test_rackmodules.moc"

QObject *makeTestObject_RackModules() { return new TestRackModules(); }
```

In `droidforge/tests/test_runner.cpp` add the declaration and exec block:

```cpp
QObject *makeTestObject_RackModules();
// ...in main(), after the GraphEdits block:
{ QObject *t = makeTestObject_RackModules(); status |= QTest::qExec(t, argc, argv); delete t; }
```

Note: `the_firmware` is created/deleted by TestGraphModel's init/cleanupTestCase, which runs first — hence the `if (!the_firmware)` guard above; do NOT delete it in TestRackModules.

- [ ] **Step 4: Add rackmodules to the app build**

In `droidforge/CMakeLists.txt`, in the graphview block (after the `graphview/sectionframeitem.cpp` line at ~290), add:

```cmake
    graphview/rackmodules.h
    graphview/rackmodules.cpp
```

- [ ] **Step 5: Reconfigure + build both, verify all 49 existing tests still pass**

Run (separately):
`cmake -S /Users/jan.kaluza/Projects/droidforge/droidforge/tests -B /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests`
`cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests`
`/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests -platform offscreen`
Expected: all existing GraphModel + GraphEdits tests pass, plus the placeholder. (The stub upgrade must not break anything: `allRegistersOf` was previously unreachable in tested code paths.)

`cmake -S /Users/jan.kaluza/Projects/droidforge/droidforge -B /Users/jan.kaluza/Projects/droidforge/droidforge/build`
`cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build`
Expected: app builds clean (one pre-existing unrelated `colorscheme.cpp` warning is known).

- [ ] **Step 6: Commit (tracked files only)**

```bash
git -C /Users/jan.kaluza/Projects/droidforge add droidforge/graphview/rackmodules.h droidforge/graphview/rackmodules.cpp droidforge/CMakeLists.txt
git -C /Users/jan.kaluza/Projects/droidforge commit -m "Scaffold graphview/rackmodules unit" -m "Pure helpers for rack-mirrored module visibility and bidirectional-gate classification. Bodies follow TDD."
```

---

### Task 2: `visibleRackModules` (TDD)

**Files:**
- Modify: `droidforge/graphview/rackmodules.cpp`
- Test: `droidforge/tests/test_rackmodules.cpp`

- [ ] **Step 1: Write the failing tests** (replace `placeholder()` with these slots)

```cpp
    void master16DefaultShowsMasterAndX7() {
        Patch *p = parse("[lfo]\n  output = O1\n");
        auto mods = visibleRackModules(p, RackVisibilitySettings{});
        QCOMPARE(mods.size(), 2);
        QCOMPARE(mods[0].name, QString("master"));
        QCOMPARE(mods[1].name, QString("x7"));   // x7OnDemand=false -> always shown
        delete p;
    }

    void x7OnDemandHidesUnusedX7() {
        Patch *p = parse("[lfo]\n  output = O1\n");
        RackVisibilitySettings vis; vis.x7OnDemand = true;
        auto mods = visibleRackModules(p, vis);
        QCOMPARE(mods.size(), 1);                // master only
        QCOMPARE(mods[0].name, QString("master"));
        delete p;
    }

    void x7OnDemandShowsX7WhenNeeded() {
        Patch *p = parse("[lfo]\n  output = G9\n");   // G9 lives on the X7
        RackVisibilitySettings vis; vis.x7OnDemand = true;
        auto mods = visibleRackModules(p, vis);
        QCOMPARE(mods.last().name, QString("x7"));
        delete p;
    }

    void usedGatesShowG8s() {
        Patch *p = parse("[lfo]\n  hz = G2.3\n");     // highestGatePrefix = 2
        auto mods = visibleRackModules(p, RackVisibilitySettings{});
        // master, g8#1, g8#2, x7
        QCOMPARE(mods.size(), 4);
        QCOMPARE(mods[1].name, QString("g8"));
        QCOMPARE(mods[1].g8Number, 1u);
        QCOMPARE(mods[1].rgbOffset, 16u);             // rack formula 8 + g*8, g=1
        QCOMPARE(mods[2].g8Number, 2u);
        QCOMPARE(mods[2].rgbOffset, 24u);
        delete p;
    }

    void showG8sSettingForcesG8s() {
        Patch *p = parse("[lfo]\n  output = O1\n");   // no gates used
        RackVisibilitySettings vis; vis.showG8s = 1;
        auto mods = visibleRackModules(p, vis);
        QCOMPARE(mods.size(), 3);                     // master, g8#1, x7
        QCOMPARE(mods[1].name, QString("g8"));
        QCOMPARE(mods[1].g8Number, 1u);
        delete p;
    }

    void master18OffsetsG8Numbers() {
        Patch *p = parse("[lfo]\n  hz = G2.3\n");
        p->setTypeOfMaster(18);
        auto mods = visibleRackModules(p, RackVisibilitySettings{});
        // master18; G2.3 -> highestGatePrefix 2, minus g8_offset 1 -> one g8,
        // numbered from 2 (the built-in gates occupy bank 1)
        QCOMPARE(mods[0].name, QString("master18"));
        QCOMPARE(mods[1].name, QString("g8"));
        QCOMPARE(mods[1].g8Number, 2u);
        QCOMPARE(mods[1].rgbOffset, 16u);             // first shown g8: 8 + 1*8
        delete p;
    }
```

- [ ] **Step 2: Build + run, verify the new tests FAIL**

`cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests`
`/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests -platform offscreen`
Expected: the six new tests fail (empty list); everything else passes.

- [ ] **Step 3: Implement**

Replace the `visibleRackModules` body in `droidforge/graphview/rackmodules.cpp`:

```cpp
QList<RackModuleSpec> visibleRackModules(const Patch *patchConst,
                                         const RackVisibilitySettings &vis)
{
    // highestGatePrefix()/needsX7() are logically const (they only scan atoms).
    Patch *patch = const_cast<Patch *>(patchConst);

    QList<RackModuleSpec> mods;
    const bool m18 = patch->typeOfMaster() == 18;
    mods.append({m18 ? QStringLiteral("master18") : QStringLiteral("master"), 0, 0});

    // MIRRORS rackview.cpp:408-440: on a MASTER18 the built-in gates occupy
    // g8 bank 1, so external G8 expanders are numbered from 2.
    const int g8Offset = m18 ? 1 : 0;
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
```

- [ ] **Step 4: Build + run, verify all pass**

Same two commands as Step 2. Expected: all green.

- [ ] **Step 5: Commit**

```bash
git -C /Users/jan.kaluza/Projects/droidforge add droidforge/graphview/rackmodules.cpp
git -C /Users/jan.kaluza/Projects/droidforge commit -m "visibleRackModules mirrors the rack view's module visibility" -m "Master kind, show_g8s vs highestGatePrefix with master18 g8-offset, RGB offsets 8+g*8, X7 on-demand condition. Tested in the harness."
```

---

### Task 3: `registerIsBidirectional` (TDD)

**Files:**
- Modify: `droidforge/graphview/rackmodules.cpp`
- Test: `droidforge/tests/test_rackmodules.cpp`

- [ ] **Step 1: Write the failing tests** (add slots)

```cpp
    void g8ExpanderJacksAreBidirectional() {
        Patch *p = parse("[lfo]\n  output = O1\n");   // master16
        QVERIFY(registerIsBidirectional(p, AtomRegister(QString("G1.3"))));
        QVERIFY(registerIsBidirectional(p, AtomRegister(QString("G4.8"))));
        delete p;
    }

    void nonGatesAndX7GatesAreNot() {
        Patch *p = parse("[lfo]\n  output = O1\n");
        QVERIFY(!registerIsBidirectional(p, AtomRegister(QString("G9"))));   // X7
        QVERIFY(!registerIsBidirectional(p, AtomRegister(QString("O1"))));
        QVERIFY(!registerIsBidirectional(p, AtomRegister(QString("I1"))));
        delete p;
    }

    void master18BuiltinBankIsOutputOnlyNotBidirectional() {
        Patch *p = parse("[lfo]\n  output = O1\n");
        p->setTypeOfMaster(18);
        QVERIFY(!registerIsBidirectional(p, AtomRegister(QString("G1.3")))); // built-in outs
        QVERIFY(registerIsBidirectional(p, AtomRegister(QString("G2.3")))); // expander
        delete p;
    }
```

- [ ] **Step 2: Build + run, verify the new tests FAIL** (stub returns false → first test fails)

- [ ] **Step 3: Implement**

```cpp
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
```

- [ ] **Step 4: Build + run, verify all pass**

- [ ] **Step 5: Commit**

```bash
git -C /Users/jan.kaluza/Projects/droidforge add droidforge/graphview/rackmodules.cpp
git -C /Users/jan.kaluza/Projects/droidforge commit -m "Classify G8 expander jacks as bidirectional" -m "Graph-side third classification next to registerIsOutputOnly: gates on a G8 expander can be input or output depending on use. Master18's built-in bank and X7 gates stay output-only."
```

---

### Task 4: `registersOfModule` (TDD)

**Files:**
- Modify: `droidforge/graphview/rackmodules.cpp`
- Test: `droidforge/tests/test_rackmodules.cpp`

- [ ] **Step 1: Write the failing tests** (add slots)

```cpp
    void masterRegisters() {
        RegisterList rl = registersOfModule({QString("master"), 0, 0});
        // I,P,B,S,E,G,N,O,L,R,X order -> I1..I8, N1..N8, O1..O8, R1..R16, X1
        QCOMPARE(rl.size(), 8 + 8 + 8 + 16 + 1);
        QCOMPARE(rl.first().toString(), QString("I1"));
        QCOMPARE(rl.last().toString(), QString("X1"));
    }

    void master18GatesAreCanonicalized() {
        RegisterList rl = registersOfModule({QString("master18"), 0, 0});
        QStringList names;
        for (const AtomRegister &r : rl) names << r.toString();
        QVERIFY(names.contains("I1"));
        QVERIFY(names.contains("I2"));
        QVERIFY(!names.contains("I3"));
        QVERIFY(names.contains("G1.1"));   // bare "G1" canonicalized to g8=1
        QVERIFY(names.contains("G1.4"));
        QVERIFY(!names.contains("G1"));    // the broken bare form must not survive
        QVERIFY(!names.contains("N1"));    // master18 has no normalization
    }

    void g8RegistersGetRgbOffsetApplied() {
        RegisterList rl = registersOfModule({QString("g8"), 2, 24});
        QStringList names;
        for (const AtomRegister &r : rl) names << r.toString();
        QVERIFY(names.contains("G2.1"));
        QVERIFY(names.contains("G2.8"));
        QVERIFY(names.contains("R25"));    // 1 + rgbOffset 24
        QVERIFY(names.contains("R32"));
        QVERIFY(!names.contains("R1"));    // unshifted form must not survive
    }

    void x7RegistersHaveBakedOffsets() {
        RegisterList rl = registersOfModule({QString("x7"), 0, 0});
        QStringList names;
        for (const AtomRegister &r : rl) names << r.toString();
        QVERIFY(names.contains("G9"));
        QVERIFY(names.contains("G12"));
        QVERIFY(names.contains("R49"));
        QVERIFY(names.contains("R56"));
    }
```


- [ ] **Step 2: Build + run, verify the new tests FAIL** (empty RegisterList)

- [ ] **Step 3: Implement**

```cpp
RegisterList registersOfModule(const RackModuleSpec &spec)
{
    RegisterList raw;
    ModuleBuilder::allRegistersOf(spec.name, 0, spec.g8Number, raw);

    RegisterList out;
    for (const AtomRegister &reg : raw) {
        if (reg.getRegisterType() == REGISTER_GATE)
            // Canonicalize via the string parser: ModuleMaster18 emits bare
            // G1..G4 (g8=0), but the canonical patch form is G1.1..G1.4 — the
            // exact mismatch behind the 2026-06-10 vanishing-wire bug.
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
```

- [ ] **Step 4: Build + run, verify all pass**

- [ ] **Step 5: Commit**

```bash
git -C /Users/jan.kaluza/Projects/droidforge add droidforge/graphview/rackmodules.cpp
git -C /Users/jan.kaluza/Projects/droidforge commit -m "registersOfModule: ModuleBuilder enumeration, canonicalized for the graph" -m "Gates round-trip through the string form (master18's bare G1..G4 -> G1.n); g8 RGB registers get the rack-position offset from the module spec."
```

---

### Task 5: Per-module hardware nodes in GraphModel (TDD)

**Files:**
- Modify: `droidforge/graphview/graphmodel.h`
- Modify: `droidforge/graphview/graphmodel.cpp`
- Test: `droidforge/tests/test_graphmodel.cpp`, `droidforge/tests/test_graphedits.cpp`

- [ ] **Step 1: Write the failing tests**

Add to `test_graphmodel.cpp` (new slots before `cleanupTestCase`):

```cpp
    void g8NodesAppearWhenGatesUsed() {
        Patch *p = parse("[lfo]\n  hz = G1.3\n");
        GraphDescription g = GraphModel::describe(p);
        const GraphNode *in = g.findNode("hw.g8.1.in");
        const GraphNode *out = g.findNode("hw.g8.1.out");
        QVERIFY(in);
        QVERIFY(out);
        QCOMPARE(in->kind, GraphNodeKind::HardwareSource);
        QCOMPARE(out->kind, GraphNodeKind::HardwareSink);
        const GraphPin *readPin = nullptr, *writePin = nullptr;
        for (const auto &pin : in->pins)  if (pin.id == "hw.G1.3.read") readPin = &pin;
        for (const auto &pin : out->pins) if (pin.id == "hw.G1.3")      writePin = &pin;
        QVERIFY(readPin);
        QCOMPARE(readPin->direction, GraphPinDirection::Out);
        QVERIFY(readPin->used);
        QVERIFY(writePin);
        QCOMPARE(writePin->direction, GraphPinDirection::In);
        delete p;
    }

    void noG8NodeWithoutGates() {
        Patch *p = parse("[lfo]\n  output = O1\n");
        GraphDescription g = GraphModel::describe(p);   // default vis: showG8s=0
        QVERIFY(!g.findNode("hw.g8.1.in"));
        QVERIFY(!g.findNode("hw.g8.1.out"));
        QVERIFY(g.findNode("hw.x7.out"));               // x7OnDemand=false -> shown
        delete p;
    }

    void visibilitySettingsControlNodes() {
        Patch *p = parse("[lfo]\n  output = O1\n");
        RackVisibilitySettings vis; vis.showG8s = 2; vis.x7OnDemand = true;
        GraphDescription g = GraphModel::describe(p, vis);
        QVERIFY(g.findNode("hw.g8.1.out"));
        QVERIFY(g.findNode("hw.g8.2.out"));
        QVERIFY(!g.findNode("hw.x7.out"));              // on demand + unused
        delete p;
    }

    void x7NodeHasGateAndRgbPins() {
        Patch *p = parse("[lfo]\n  output = G9\n");
        GraphDescription g = GraphModel::describe(p);
        const GraphNode *x7 = g.findNode("hw.x7.out");
        QVERIFY(x7);
        QSet<QString> ids;
        for (const auto &pin : x7->pins) ids.insert(pin.id);
        QVERIFY(ids.contains("hw.G9"));
        QVERIFY(ids.contains("hw.G9.read"));   // output register read-back pin
        QVERIFY(ids.contains("hw.R49"));
        QVERIFY(ids.contains("hw.R56"));
        delete p;
    }

    void masterOutGainsRgbAndExtraPins() {
        Patch *p = parse("[lfo]\n  output = O1\n");
        GraphDescription g = GraphModel::describe(p);
        const GraphNode *out = g.findNode("hw.master.out");
        QVERIFY(out);
        QSet<QString> ids;
        for (const auto &pin : out->pins) ids.insert(pin.id);
        QVERIFY(ids.contains("hw.R1"));
        QVERIFY(ids.contains("hw.R16"));
        QVERIFY(ids.contains("hw.X1"));
        QVERIFY(ids.contains("hw.R1.read"));
        delete p;
    }

    void g8RgbPinsCarryRackOffset() {
        Patch *p = parse("[lfo]\n  hz = G1.3\n");       // shows G8 #1
        GraphDescription g = GraphModel::describe(p);
        const GraphNode *out = g.findNode("hw.g8.1.out");
        QVERIFY(out);
        QSet<QString> ids;
        for (const auto &pin : out->pins) ids.insert(pin.id);
        QVERIFY(ids.contains("hw.R17"));                // 8 + 1*8 offset
        QVERIFY(ids.contains("hw.R24"));
        QVERIFY(!ids.contains("hw.R1"));
        delete p;
    }

    void master18NodesAreAccurate() {
        Patch *p = parse("[lfo]\n  output = O1\n");
        p->setTypeOfMaster(18);
        GraphDescription g = GraphModel::describe(p);
        const GraphNode *in = g.findNode("hw.master.in");
        const GraphNode *out = g.findNode("hw.master.out");
        QVERIFY(in);
        QVERIFY(out);
        QSet<QString> inIds, outIds;
        for (const auto &pin : in->pins)  inIds.insert(pin.id);
        for (const auto &pin : out->pins) outIds.insert(pin.id);
        QVERIFY(inIds.contains("hw.I1"));
        QVERIFY(inIds.contains("hw.I2"));
        QVERIFY(!inIds.contains("hw.I3"));      // master18 has only 2 inputs
        QVERIFY(outIds.contains("hw.G1.1"));    // built-in gate outs (write pins)
        QVERIFY(outIds.contains("hw.G1.4"));
        QVERIFY(!outIds.contains("hw.N1"));     // no normalization registers
        delete p;
    }

    void gateInputWireComesFromReadPin() {
        Patch *p = parse("[lfo]\n  hz = G1.5\n");
        GraphDescription g = GraphModel::describe(p);
        bool found = false;
        for (const auto &w : g.wires)
            if (!w.isCable && w.fromPinId == "hw.G1.5.read" && w.toPinId == "c0.0.hz.p")
                found = true;
        QVERIFY(found);
        delete p;
    }
```

Update the two existing gate tests in `test_graphedits.cpp` to the new model (replace their bodies):

```cpp
    // Gate pins must use the canonical register form so their ids match the
    // ids wires use (a bare "G5" normalizes to g8=1 "G1.5"). Otherwise a gate
    // wire's endpoint id never matches the pin and rebuildGraphics drops it.
    void gatePinsUseCanonicalIds() {
        Patch *p = parse("[lfo]\n  output = 0\n");
        RackVisibilitySettings vis; vis.showG8s = 1;     // force G8 #1 visible
        GraphDescription g = GraphModel::describe(p, vis);
        QSet<QString> hwPins;
        for (const auto &n : g.nodes)
            for (const auto &pin : n.pins)
                if (pin.id.startsWith("hw.") && pin.label.startsWith("G"))
                    hwPins.insert(pin.id);
        // G8 #1 jacks: write pin + read pin per jack. X7 gates: write + read.
        QVERIFY(hwPins.contains("hw.G1.1"));
        QVERIFY(hwPins.contains("hw.G1.1.read"));
        QVERIFY(hwPins.contains("hw.G1.8"));
        QVERIFY(hwPins.contains("hw.G9"));
        QVERIFY(hwPins.contains("hw.G12"));
        // The broken g8=0 forms for 1..8 must NOT appear.
        QVERIFY(!hwPins.contains("hw.G5"));
        QVERIFY(!hwPins.contains("hw.G1"));
        delete p;
    }

    // A circuit reading a gate produces a wire whose source endpoint matches an
    // actual gate pin id (round-trip: pin id == wire id). Bidirectional jacks
    // read from their .read pin on the G8-in node.
    void gateReadWireMatchesPin() {
        Patch *p = parse("[lfo]\n  hz = G1.5\n");
        GraphDescription g = GraphModel::describe(p);
        QSet<QString> hwPins;
        for (const auto &n : g.nodes)
            for (const auto &pin : n.pins)
                hwPins.insert(pin.id);
        bool foundGateWire = false;
        for (const auto &w : g.wires) {
            if (w.fromPinId == "hw.G1.5.read") {
                foundGateWire = true;
                QVERIFY(hwPins.contains(w.fromPinId)); // endpoint resolves to a real pin
            }
        }
        QVERIFY(foundGateWire);
        delete p;
    }
```

`test_graphedits.cpp` needs `#include "rackmodules.h"` for `RackVisibilitySettings` (add next to its `#include "graphmodel.h"`).

- [ ] **Step 2: Build + run, verify the new/updated tests FAIL**

Expected failures: all new graphmodel slots (no g8/x7 nodes exist yet), the two updated graphedits gate tests, and `gateInputWireComesFromReadPin` (wire still sources from `hw.G1.5`).

- [ ] **Step 3: Implement the GraphModel changes**

`droidforge/graphview/graphmodel.h`:

```cpp
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
```

`droidforge/graphview/graphmodel.cpp` — replace `hwReadPinId`, `appendRegisterPins`, and `addHardwareNodes` (the block from line ~85 to ~215) with:

```cpp
// Read-pin id for a register: output-only and bidirectional registers expose a
// distinct read pin (hw.<reg>.read); plain inputs are read straight from their
// single source pin (hw.<reg>).
QString hwReadPinId(const AtomRegister &reg, const Patch *patch)
{
    const QString base = QString(kHwPrefix) + reg.toString();
    return (patch->registerIsOutputOnly(reg) || registerIsBidirectional(patch, reg))
               ? base + ".read" : base;
}

// Append the pin(s) for one hardware register, routed to the module's source
// (in) node and/or sink (out) node:
//  - plain input        -> read pin hw.<reg> (Out) on the in-node
//  - output register    -> write pin hw.<reg> (In) + read pin hw.<reg>.read
//                          (Out), both on the out-node. Exception: N gets no
//                          read pin (you read the corresponding I, never N).
//  - bidirectional gate -> read pin hw.<reg>.read (Out) on the in-node, write
//                          pin hw.<reg> (In) on the out-node. Using both at
//                          once is legal: writing makes the jack an output and
//                          the read becomes a read-back (manual semantics).
void appendRegisterPins(GraphNode &inNode, GraphNode &outNode,
                        const AtomRegister &reg, Patch *patch)
{
    const QString base = QString(kHwPrefix) + reg.toString();
    const bool used = patch->registerUsed(reg);

    auto makePin = [&](const QString &id, GraphPinDirection dir) {
        GraphPin p;
        p.id        = id;
        p.label     = reg.toString();
        p.direction = dir;
        p.portKind  = GraphPortKind::Signal;
        p.role      = GraphPinRole::Simple;
        p.used      = used;
        return p;
    };

    if (registerIsBidirectional(patch, reg)) {
        inNode.pins.append(makePin(base + ".read", GraphPinDirection::Out));
        outNode.pins.append(makePin(base, GraphPinDirection::In));
        return;
    }
    if (!patch->registerIsOutputOnly(reg)) {
        inNode.pins.append(makePin(base, GraphPinDirection::Out));
        return;
    }
    outNode.pins.append(makePin(base, GraphPinDirection::In));
    if (reg.getRegisterType() == REGISTER_NORMALIZE)
        return;
    outNode.pins.append(makePin(hwReadPinId(reg, patch), GraphPinDirection::Out));
}

void addHardwareNodes(GraphDescription &g, const Patch *patchConst,
                      const RackVisibilitySettings &vis)
{
    // registerUsed is logically const (only iterates, never mutates); cast is safe.
    Patch *patch = const_cast<Patch *>(patchConst);

    // One source/sink node pair per visible rack module (mirrors the rack view).
    for (const RackModuleSpec &spec : visibleRackModules(patchConst, vis)) {
        GraphNode in, out;
        if (spec.name == QStringLiteral("g8")) {
            in.id     = QString("hw.g8.%1.in").arg(spec.g8Number);
            in.title  = QString("G8 #%1 in").arg(spec.g8Number);
            out.id    = QString("hw.g8.%1.out").arg(spec.g8Number);
            out.title = QString("G8 #%1 out").arg(spec.g8Number);
        } else if (spec.name == QStringLiteral("x7")) {
            in.id     = "hw.x7.in";       // never gets pins; dropped below
            in.title  = "X7 in";
            out.id    = "hw.x7.out";
            out.title = "X7";
        } else { // master / master18
            in.id     = "hw.master.in";
            in.title  = "Master in";
            out.id    = "hw.master.out";
            out.title = "Master out";
        }
        in.kind  = GraphNodeKind::HardwareSource;
        out.kind = GraphNodeKind::HardwareSink;

        for (const AtomRegister &reg : registersOfModule(spec))
            appendRegisterPins(in, out, reg, patch);

        if (!in.pins.isEmpty())  g.nodes.append(in);
        if (!out.pins.isEmpty()) g.nodes.append(out);
    }

    // Per-controller nodes (firmware enumeration, unchanged from M1)
    static const register_type_t ctrlSource[] = {
        REGISTER_POT, REGISTER_BUTTON, REGISTER_ENCODER, REGISTER_SWITCH
    };
    static const register_type_t ctrlSink[] = {
        REGISTER_LED, REGISTER_RGB_LED
    };

    for (qsizetype ci = 0; ci < patch->numControllers(); ci++) {
        QString ctrlName = patch->controller(ci);

        GraphNode controls;
        controls.id    = QString("hw.ctrl%1.controls").arg(ci + 1);
        controls.kind  = GraphNodeKind::HardwareSource;
        controls.title = QString("Ctrl %1 · %2").arg(ci + 1).arg(ctrlName);

        GraphNode leds;
        leds.id    = QString("hw.ctrl%1.leds").arg(ci + 1);
        leds.kind  = GraphNodeKind::HardwareSink;
        leds.title = QString("Ctrl %1 · %2 LEDs").arg(ci + 1).arg(ctrlName);

        for (register_type_t t : ctrlSource) {
            unsigned count = the_firmware->numControllerRegisters(ctrlName, t);
            for (unsigned n = 1; n <= count; n++)
                appendRegisterPins(controls, leds, AtomRegister(t, static_cast<unsigned>(ci + 1), 0, n), patch);
        }
        for (register_type_t t : ctrlSink) {
            unsigned count = the_firmware->numControllerRegisters(ctrlName, t);
            for (unsigned n = 1; n <= count; n++)
                appendRegisterPins(controls, leds, AtomRegister(t, static_cast<unsigned>(ci + 1), 0, n), patch);
        }

        if (!controls.pins.isEmpty()) g.nodes.append(controls);
        if (!leds.pins.isEmpty())     g.nodes.append(leds);
    }
}
```

Notes for the implementer:
- Add `#include "rackmodules.h"` to graphmodel.cpp's includes.
- Controller registers route correctly through the two-node `appendRegisterPins`: P/B/E/S are plain inputs → controls node; L/R are output-only → leds node. The two loops are kept (preserving pin order: pots/buttons/encoders/switches, then LEDs) but both calls now pass `(controls, leds, …)`.
- In `GraphModel::describe`, change the signature to `describe(const Patch *patch, const RackVisibilitySettings &vis)` (the default lives in the header) and pass `vis` to `addHardwareNodes(g, patch, vis)`. `addWires` is untouched — it picks up gate read pins automatically via `hwReadPinId`.

- [ ] **Step 4: Build + run, verify everything passes**

All previous tests plus the new ones. Watch for these intentional behavior changes if anything else fails: a gateless master16 patch no longer has any `hw.G*` pins by default (no G8 node), and `hw.master.out` gained R/X pins.

- [ ] **Step 5: Visual check via graphshot**

`cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests --target graphshot`
`/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/graphshot -platform offscreen`
Open `/tmp/graphshot.png` and confirm per-module nodes render (sources left, sinks right, no overlap disasters).

- [ ] **Step 6: Commit**

```bash
git -C /Users/jan.kaluza/Projects/droidforge add droidforge/graphview/graphmodel.h droidforge/graphview/graphmodel.cpp
git -C /Users/jan.kaluza/Projects/droidforge commit -m "Build per-module rack-accurate hardware nodes" -m "Master/Master18, one in+out node pair per visible G8 expander, X7 — driven by visibleRackModules + ModuleBuilder enumeration. Bidirectional G8 jacks get a read pin on the in-node and a write pin on the out-node; master gains R/X pins."
```

---

### Task 6: GraphEdits learns bidirectional gates (TDD)

**Files:**
- Modify: `droidforge/graphview/graphedits.cpp`
- Test: `droidforge/tests/test_graphedits.cpp`

- [ ] **Step 1: Write the failing tests** (add slots to `test_graphedits.cpp`)

```cpp
    // Bidirectional G8 jacks: hw.<reg> is the write pin (sink), hw.<reg>.read
    // the read pin (source).
    void gateWritePinIsSink() {
        Patch *p = parse("[lfo]\n  output = _A\n");
        PinRef w = parsePin(p, "hw.G1.3");
        QCOMPARE(w.kind, PinRef::HwWrite);
        QVERIFY(w.isSink());
        PinRef r = parsePin(p, "hw.G1.3.read");
        QCOMPARE(r.kind, PinRef::HwRead);
        QVERIFY(r.isSource());
        delete p;
    }

    void circuitOutputDrivesGateJack() {
        Patch *p = parse("[lfo]\n  output = _A\n");
        QVERIFY(isValidDrop(p, "c0.0.output.out", "hw.G1.3", DragMode::Connect));
        QVERIFY(connectPins(p, "c0.0.output.out", "hw.G1.3"));
        QCOMPARE(outAtom(p, 0, 0, "output")->toString(), QString("G1.3"));
        delete p;
    }

    void gateReadPinFeedsCircuitInput() {
        Patch *p = parse("[lfo]\n  hz = 0\n");
        QVERIFY(isValidDrop(p, "hw.G1.3.read", "c0.0.hz.p", DragMode::Connect));
        QVERIFY(connectPins(p, "hw.G1.3.read", "c0.0.hz.p"));
        QCOMPARE(inAtom(p, 0, 0, "hz", 1)->toString(), QString("G1.3"));
        delete p;
    }

    void gateInputResolvesToReadPinSource() {
        Patch *p = parse("[lfo]\n  hz = G1.3\n");
        QCOMPARE(getConnectedSource(p, "c0.0.hz.p"), QString("hw.G1.3.read"));
        delete p;
    }

    void x7GateKeepsOutputOnlyClassification() {
        Patch *p = parse("[lfo]\n  output = _A\n");
        PinRef w = parsePin(p, "hw.G9");
        QCOMPARE(w.kind, PinRef::HwWrite);     // unchanged: output-only
        delete p;
    }
```

(Uses the file's existing helpers, defined at `test_graphedits.cpp:22–39`: `parse(src)`, `inAtom(p, s, c, jack, col)`, `outAtom(p, s, c, jack)` — signatures verified.)

- [ ] **Step 2: Build + run, verify the new tests FAIL**

`gateWritePinIsSink` fails first: `parsePin(p, "hw.G1.3")` currently returns `HwSource`.

- [ ] **Step 3: Implement**

In `droidforge/graphview/graphedits.cpp`:

1. Add `#include "rackmodules.h"` to the includes.
2. In `parsePin` (graphedits.cpp:15), change the classification else-chain to:

```cpp
        if (isRead)
            ref.kind = PinRef::HwRead;
        else if (patch->registerIsOutputOnly(areg) || registerIsBidirectional(patch, areg))
            ref.kind = PinRef::HwWrite;
        else
            ref.kind = PinRef::HwSource;
```

3. In `getConnectedSource` (graphedits.cpp:276), change the register branch to:

```cpp
        if (a->isRegister()) {
            AtomRegister areg(a->toString());
            return (patch->registerIsOutputOnly(areg) || registerIsBidirectional(patch, areg))
                       ? (QString("hw.") + a->toString() + ".read")
                       : (QString("hw.") + a->toString());
        }
```

- [ ] **Step 4: Build + run, verify all pass**

- [ ] **Step 5: Commit**

```bash
git -C /Users/jan.kaluza/Projects/droidforge add droidforge/graphview/graphedits.cpp
git -C /Users/jan.kaluza/Projects/droidforge commit -m "GraphEdits: bidirectional gate jacks wire both ways" -m "hw.<gate> classifies as a write pin (sink) and hw.<gate>.read as its read source, matching the new G8 in/out nodes. X7 and master18 built-in gates keep their output-only classification."
```

---

### Task 7: Hub broadcast + visibility-aware rebuild (app integration)

**Files:**
- Modify: `droidforge/graphview/graphview.h`
- Modify: `droidforge/graphview/graphview.cpp`
- Modify: `droidforge/main/mainwindow.cpp` (our M1 block only, ~line 120)

- [ ] **Step 1: Add the signal and emit on commit**

`droidforge/graphview/graphview.h` — add a signals section after the `public slots:` block:

```cpp
signals:
    // Emitted after a successful commit. MainWindow routes it into the
    // UpdateHub so the rack (auto show/hide of X7/G8s), the list editor and
    // this view itself (rebuild via the hub->rebuildGraphics connection) all
    // update — the same path list-editor edits take.
    void patchModified();
```

`droidforge/graphview/graphview.cpp` — `commitEdit` (graphview.cpp:248) becomes:

```cpp
void GraphView::commitEdit(bool ok, const QString &message)
{
    if (!ok)
        return;
    patch->commit(message);
    emit patchModified();   // hub fans out; our rebuild returns via the hub
}
```

(If `commitEdit` currently calls `rebuildGraphics()` after the commit, delete that call — the hub connection in `mainwindow.cpp:121` rebuilds us. In the graphshot harness nothing connects the signal, which is fine: graphshot never commits.)

- [ ] **Step 2: Read visibility settings at rebuild time**

In `droidforge/graphview/graphview.cpp`, add `#include "rackmodules.h"` and `#include <QSettings>`, and in `rebuildGraphics()` replace `GraphDescription g = GraphModel::describe(patch);` with:

```cpp
    // Same settings the rack view honors; written by RackView::showG8s/showX7
    // before our slot runs (its action connections predate ours).
    QSettings settings;
    RackVisibilitySettings vis;
    vis.showG8s    = settings.value("show_g8s", 0).toInt();
    vis.x7OnDemand = settings.value("show_x7_on_demand").toBool();
    GraphDescription g = GraphModel::describe(patch, vis);
```

- [ ] **Step 3: Wire it up in our M1 integration block**

In `droidforge/main/mainwindow.cpp`, directly after the existing line 121 (`connect(theHub(), &UpdateHub::patchModified, &graphView, &GraphView::rebuildGraphics);`), add:

```cpp
    connect(&graphView, &GraphView::patchModified, theHub(), &UpdateHub::modifyPatch);
    // Rebuild the graph when the View menu changes which rack modules exist.
    // RackView's handlers (connected earlier, in its constructor) write the
    // QSettings these rebuild from.
    for (auto a : {ACTION_SHOW_USED_G8s, ACTION_SHOW_ONE_G8, ACTION_SHOW_TWO_G8,
                   ACTION_SHOW_THREE_G8, ACTION_SHOW_FOUR_G8,
                   ACTION_SHOW_X7_ON_DEMAND, ACTION_SHOW_X7_ALWAYS})
        connect(theActions()->action(a), &QAction::triggered,
                &graphView, &GraphView::rebuildGraphics);
```

- [ ] **Step 4: Build the app and the harness**

`cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build`
Expected: clean build.
`cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests`
`/Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests -platform offscreen`
Expected: all tests still green (graphview.cpp changes also compile in graphshot).

- [ ] **Step 5: Commit**

```bash
git -C /Users/jan.kaluza/Projects/droidforge add droidforge/graphview/graphview.h droidforge/graphview/graphview.cpp droidforge/main/mainwindow.cpp
git -C /Users/jan.kaluza/Projects/droidforge commit -m "Graph commits broadcast through the UpdateHub" -m "commitEdit emits patchModified -> hub -> rack auto show/hide + list editor + our own rebuild. Graph rebuilds honor the show_g8s/show_x7_on_demand settings and refresh on the View-menu module toggles."
```

---

### Task 8: Backlog + docs closure, operator checklist

**Files:**
- Delete: `docs/backlog/node-graph-rack-accurate-hardware-nodes.md`, `docs/backlog/node-graph-rack-auto-show-hide.md`
- Modify: `docs/backlog/README.md`

- [ ] **Step 1: Remove the two shipped backlog items and update the README index**

Delete the two files; remove their two rows from the table in `docs/backlog/README.md`.

- [ ] **Step 2: Commit**

```bash
git -C /Users/jan.kaluza/Projects/droidforge add docs/backlog
git -C /Users/jan.kaluza/Projects/droidforge commit -m "Close rack-accurate-hardware and rack-auto-show-hide backlog items" -m "Both shipped via docs/superpowers/specs/2026-06-10-node-graph-rack-accurate-hardware-design.md."
```

- [ ] **Step 3: Hand the operator the interactive checklist**

Build + open the app, then ask the operator to verify:

1. **Default master16 patch:** Master in (I1–I8) left; Master out (N, O, R1–R16, X1) right; X7 node right; **no G8 node** until a gate is used or "Show at least one G8" is set (behavior change: gates no longer always-visible).
2. **Gate as input:** View menu → Show at least one G8 → drag `G8 #1 in`'s `G1.3` read pin → a circuit input. Wire appears; list editor shows `G1.3` on that input.
3. **Gate as output:** drag a circuit output → `G8 #1 out`'s `G1.3` write pin. Works; using the same jack's read pin simultaneously is allowed (read-back).
4. **Auto-show G8:** with "show used G8s" (default), wiring `G2.x` (force two G8s visible first to reach the pin, then set back) — simpler: in the list editor add `hz = G2.1`, confirm graph + rack both show G8 #2; delete it, both hide it.
5. **Auto-show X7:** enable "Show X7 if needed by the current patch". In the **graph**, wire a circuit output → X7 `G9` write pin… (X7 hidden? then first do it while X7 is shown always, toggle to on-demand, X7 stays because needed). Then delete the G9 wire **in the graph** → X7 disappears from rack *and* graph. This is the headline auto-show/hide fix.
6. **MASTER18:** Rack menu → switch master to MASTER18. Master in shows I1/I2 only; Master out gains G1.1–G1.4; no N pins; G8 numbering starts at #2.
7. **View toggles:** Show one/two/three/four G8s and X7 always/on-demand update the graph immediately.
8. **Undo:** a graph wiring edit is one Cmd-Z step; undo updates rack and graph.
9. **List-editor edits** still rebuild the graph (regression check on the hub path).

---

## Self-review notes (already applied)

- Spec §1–§5 all map to tasks: visibility helper (T2), per-module nodes + R/X + canonicalization (T4/T5), bidirectional jacks (T3/T5/T6), hub broadcast + action subscriptions (T7), testing strategy (T1 harness, TDD throughout, checklist T8).
- Known intentional behavior changes called out in T5 Step 4 and T8 checklist item 1.
- `registerIsBidirectional` takes `const Patch*` and only calls `typeOfMaster()` (const) — no cast needed; `visibleRackModules` const_casts for `highestGatePrefix`/`needsX7` (non-const, logically const).
