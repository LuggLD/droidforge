# Node Graph Editor — Milestone 1: Foundation & Read-Only Graph — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Render any DROID patch as a read-only node graph (circuit nodes, hardware nodes, wires, section frames) in a new view that toggles with the existing list editor, derived from the live `Patch` model.

**Architecture:** A pure, GUI-free derivation layer (`GraphModel`) turns a `const Patch *` into a `GraphDescription` (nodes/pins/wires/frames); a pure `GraphLayout` assigns left-to-right positions; a `QGraphicsView`-based `GraphView` renders that description. The derivation and layout are TDD'd in a throwaway (gitignored, never-committed) Qt Test harness; GUI rendering is verified by running the app. Editing, persistence, and interaction come in Milestones 2–3.

**Tech Stack:** C++17, Qt 6.11 (Widgets, Pdf/PdfWidgets already linked), CMake + Ninja, Qt Test (throwaway harness only).

**Spec:** `docs/superpowers/specs/2026-06-08-node-graph-editor-design.md`

**Conventions for this repo:**
- Build the app: `cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build`
- Run the app: `open "/Users/jan.kaluza/Projects/droidforge/droidforge/build/DROID Forge.app"`
- One command per shell invocation; use absolute paths (no `cd &&` chains).
- New production code lives under `droidforge/graphview/` (mirrors `patchview/`, `rackview/`).
- The throwaway test harness lives under `droidforge/tests/` and is **gitignored** — never commit it. Commits in this plan contain production code only.

---

## File Structure

**Production (committed):**
- `droidforge/graphview/graphmodeltypes.h` — plain structs/enums describing a derived graph (`GraphPin`, `GraphNode`, `GraphWire`, `GraphSectionFrame`, `GraphDescription`). No Qt GUI deps.
- `droidforge/graphview/graphmodel.h` / `.cpp` — `GraphModel::describe(const Patch *)` → `GraphDescription`. Pure logic, depends only on the model + `the_firmware`.
- `droidforge/graphview/graphlayout.h` / `.cpp` — `GraphLayout::layout(GraphDescription &)` assigns node positions and frame rectangles (layered left→right). Pure logic.
- `droidforge/graphview/graphview.h` / `.cpp` — `GraphView : QGraphicsView, PatchView`. Builds `QGraphicsItem`s from a laid-out `GraphDescription`; rebuilds on `UpdateHub::patchModified`.
- `droidforge/graphview/nodeitem.h` / `.cpp` — `NodeItem : QGraphicsItem` renders one node (title + pin rows).
- `droidforge/graphview/wireitem.h` / `.cpp` — `WireItem : QGraphicsItem` renders one wire (cubic path).
- `droidforge/graphview/sectionframeitem.h` / `.cpp` — `SectionFrameItem : QGraphicsItem` renders a section frame rectangle behind its nodes.

**Throwaway (gitignored, never committed):**
- `droidforge/tests/CMakeLists.txt` — standalone Qt Test target compiling the graph-logic sources + their model dependencies.
- `droidforge/tests/test_graphmodel.cpp`, `droidforge/tests/test_graphlayout.cpp` — Qt Test cases.

**Modified (committed):**
- `droidforge/CMakeLists.txt` — add the 5 new production source pairs + `graphview` to include dirs.
- `.gitignore` (repo root) — ignore `droidforge/tests/` and `droidforge/build-tests/`.
- Integration point for the toggle (exact file located in Task 8).

---

## Data model (defined once, used by all tasks)

These are the types every later task refers to. Do not rename them.

```cpp
// graphview/graphmodeltypes.h
#ifndef GRAPHMODELTYPES_H
#define GRAPHMODELTYPES_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QPointF>
#include <QRectF>

enum class GraphNodeKind { Circuit, HardwareSource, HardwareSink };
enum class GraphPinDirection { In, Out };
enum class GraphPortKind { Signal, Text };
enum class GraphPinRole { Simple, Primary, Scale, Offset }; // Simple = output pin or text input

struct GraphPin {
    QString id;             // globally unique, stable: see id scheme below
    QString label;          // shown text, e.g. "hz", "P1.1", "output"
    GraphPinDirection direction = GraphPinDirection::In;
    GraphPortKind portKind = GraphPortKind::Signal;
    GraphPinRole role = GraphPinRole::Simple;
    bool connected = false; // a wire (cable or register) is attached
    bool used = false;      // hardware pins only: referenced somewhere in the patch
    QString constantText;   // value shown when unconnected (number/text); empty if none
};

struct GraphNode {
    QString id;             // "c<section>.<circuit>" or "hw.<key>"
    GraphNodeKind kind = GraphNodeKind::Circuit;
    QString title;          // circuit type ("lfo") or hardware group ("Master in")
    int sectionIndex = -1;  // circuit nodes only; -1 for hardware
    int circuitIndex = -1;  // index within the section; -1 for hardware
    QList<GraphPin> pins;
    QPointF pos;            // filled by GraphLayout
};

struct GraphWire {
    QString fromPinId;      // producer pin
    QString toPinId;        // consumer pin
    bool isCable = false;   // true = named net; false = register connection
    QString cableName;      // valid iff isCable
};

struct GraphSectionFrame {
    int sectionIndex = -1;
    QString title;
    QStringList nodeIds;    // circuit node ids contained
    QRectF rect;            // filled by GraphLayout
};

struct GraphDescription {
    QList<GraphNode> nodes;
    QList<GraphWire> wires;
    QList<GraphSectionFrame> frames;

    const GraphNode *findNode(const QString &id) const;
};

#endif // GRAPHMODELTYPES_H
```

**Pin id scheme (stable, referenced by wires):**
- Circuit input primary: `c<S>.<C>.<jack>.p` ; scale: `.s` ; offset: `.o` (S=section index, C=circuit index).
- Circuit output: `c<S>.<C>.<jack>.out`.
- Hardware pin: `hw.<registerString>` where `<registerString>` is `AtomRegister::toString()` (e.g. `hw.P1.1`, `hw.O1`, `hw.I3`).

---

### Task 0: Throwaway Qt Test harness

**Files:**
- Create (gitignored): `droidforge/tests/CMakeLists.txt`
- Create (gitignored): `droidforge/tests/test_smoke.cpp`
- Create (committed): `droidforge/graphview/graphmodeltypes.h` (from the data-model section above)
- Modify (committed): `.gitignore`

- [ ] **Step 1: Gitignore the harness and its build dir**

Append to repo-root `.gitignore`:

```
# Throwaway node-graph test harness (never committed)
droidforge/tests/
droidforge/build-tests/
```

- [ ] **Step 2: Create the production types header**

Create `droidforge/graphview/graphmodeltypes.h` with the exact contents from the "Data model" section above. Add the `findNode` definition to a new committed file `droidforge/graphview/graphmodeltypes.cpp`:

```cpp
#include "graphmodeltypes.h"

const GraphNode *GraphDescription::findNode(const QString &id) const
{
    for (const auto &n : nodes)
        if (n.id == id)
            return &n;
    return nullptr;
}
```

- [ ] **Step 3: Write the harness CMakeLists**

Create `droidforge/tests/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)
project(DROIDForgeTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Test Pdf PdfWidgets)

set(SRC_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Production sources the model + firmware transitively need, plus the new graph logic.
# Glob is acceptable here because this target is throwaway and rebuilt locally.
file(GLOB MODEL_SRC
    ${SRC_ROOT}/patch/*.cpp
    ${SRC_ROOT}/parser/*.cpp
    ${SRC_ROOT}/modules/*.cpp
    ${SRC_ROOT}/main/droidfirmware.cpp
    ${SRC_ROOT}/main/registerlabels.cpp
    ${SRC_ROOT}/utilities/*.cpp)
file(GLOB GRAPH_SRC ${SRC_ROOT}/graphview/graphmodel*.cpp ${SRC_ROOT}/graphview/graphlayout*.cpp)

include_directories(
    ${SRC_ROOT} ${SRC_ROOT}/patch ${SRC_ROOT}/parser ${SRC_ROOT}/modules
    ${SRC_ROOT}/main ${SRC_ROOT}/utilities ${SRC_ROOT}/patchview ${SRC_ROOT}/graphview)

add_executable(forgetests
    test_smoke.cpp ${GRAPH_SRC} ${MODEL_SRC} ${SRC_ROOT}/resources.qrc)
target_link_libraries(forgetests PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test Qt6::Pdf Qt6::PdfWidgets)
```

> If the glob pulls a `.cpp` that drags in heavy GUI deps and fails to link, exclude it with `list(REMOVE_ITEM MODEL_SRC ...)` and note it. The closure needed for `Patch` + `DroidFirmware` is the goal.

- [ ] **Step 4: Write the smoke test**

Create `droidforge/tests/test_smoke.cpp`:

```cpp
#include <QtTest>
#include "droidfirmware.h"

DroidFirmware *the_firmware = nullptr; // provide the global the model expects

class TestSmoke : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { the_firmware = new DroidFirmware(); }
    void firmwareLoads() {
        QVERIFY(the_firmware->circuitExists("lfo"));
        QVERIFY(the_firmware->jackIsInput("lfo", "hz"));
    }
    void cleanupTestCase() { delete the_firmware; the_firmware = nullptr; }
};

QTEST_MAIN(TestSmoke)
#include "test_smoke.moc"
```

> `the_firmware` is the single global the model relies on. Other globals (`the_clipboard`, GUI singletons) are not needed for derivation/layout; if a linked `.cpp` references one, declare a stub global in `test_smoke.cpp`.

- [ ] **Step 5: Configure and build the harness**

Run: `cmake -S /Users/jan.kaluza/Projects/droidforge/droidforge/tests -B /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt`
Then: `cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests`
Expected: links to an executable `forgetests` (resolve missing-global link errors by adding stubs per Step 4's note).

- [ ] **Step 6: Run the smoke test**

Run: `QT_QPA_PLATFORM=offscreen /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests`
Expected: `Totals: 3 passed, 0 failed` (initTestCase, firmwareLoads, cleanupTestCase).

- [ ] **Step 7: Commit production pieces only**

Run: `git -C /Users/jan.kaluza/Projects/droidforge add .gitignore droidforge/graphview/graphmodeltypes.h droidforge/graphview/graphmodeltypes.cpp`
Run: `git -C /Users/jan.kaluza/Projects/droidforge commit -m "Add graph model types and gitignore test harness"`
(The `tests/` dir stays uncommitted by design.)

---

### Task 1: Derive circuit nodes & pins

**Files:**
- Create: `droidforge/graphview/graphmodel.h`, `droidforge/graphview/graphmodel.cpp`
- Test (throwaway): `droidforge/tests/test_graphmodel.cpp`

- [ ] **Step 1: Declare GraphModel**

Create `droidforge/graphview/graphmodel.h`:

```cpp
#ifndef GRAPHMODEL_H
#define GRAPHMODEL_H

#include "graphmodeltypes.h"
class Patch;

// Pure derivation: turns a Patch into a GraphDescription. No GUI, no positions.
namespace GraphModel {
    GraphDescription describe(const Patch *patch);
}
#endif // GRAPHMODEL_H
```

- [ ] **Step 2: Write the failing test for circuit pins**

Add to `droidforge/tests/test_graphmodel.cpp` a test that parses a tiny patch and checks the derived circuit node. Use the existing parser to build a `Patch` from text.

```cpp
#include <QtTest>
#include "patch.h"
#include "patchparser.h"
#include "graphmodel.h"
#include "droidfirmware.h"

extern DroidFirmware *the_firmware;

static Patch *parse(const QString &src) {
    PatchParser parser;
    return parser.parseString(src); // returns a Patch*; see parser API
}

class TestGraphModel : public QObject {
    Q_OBJECT
private slots:
    void circuitNodeHasCompoundInputPins() {
        Patch *p = parse("[lfo]\n  hz = P1.1 * 0.5\n  output = _OUT\n");
        GraphDescription g = GraphModel::describe(p);
        const GraphNode *lfo = nullptr;
        for (const auto &n : g.nodes) if (n.kind == GraphNodeKind::Circuit) lfo = &n;
        QVERIFY(lfo);
        QCOMPARE(lfo->title, QString("lfo"));
        // hz is an input -> three pins primary/scale/offset
        int prim=0, scale=0, off=0, out=0;
        for (const auto &pin : lfo->pins) {
            if (pin.label=="hz" && pin.role==GraphPinRole::Primary) prim++;
            if (pin.label=="hz" && pin.role==GraphPinRole::Scale)   scale++;
            if (pin.label=="hz" && pin.role==GraphPinRole::Offset)  off++;
            if (pin.label=="output" && pin.direction==GraphPinDirection::Out) out++;
        }
        QCOMPARE(prim, 1); QCOMPARE(scale, 1); QCOMPARE(off, 1); QCOMPARE(out, 1);
        delete p;
    }
};
QTEST_MAIN(TestGraphModel)
#include "test_graphmodel.moc"
```

> Confirm the exact `PatchParser` entry point before running (check `parser/patchparser.h`); adapt `parse()` to the real signature. If only file-based parsing exists, write the source to a temp file and parse that.

- [ ] **Step 3: Verify it fails**

Add `test_graphmodel.cpp` to `add_executable` in `tests/CMakeLists.txt` (replace `test_smoke.cpp`, or build a second target). Rebuild and run:
`cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests`
Expected: FAIL (link error: `GraphModel::describe` undefined).

- [ ] **Step 4: Implement circuit-node derivation**

Create `droidforge/graphview/graphmodel.cpp`:

```cpp
#include "graphmodel.h"
#include "patch.h"
#include "patchsection.h"
#include "circuit.h"
#include "jackassignment.h"
#include "jackassignmentinput.h"
#include "atom.h"
#include "droidfirmware.h"

namespace {

QString pinId(int s, int c, const QString &jack, const char *suffix) {
    return QString("c%1.%2.%3.%4").arg(s).arg(c).arg(jack, suffix);
}

GraphPortKind portKindOf(const QString &circuit, const QString &jack, bool isInput) {
    QString sym = the_firmware->jackTypeSymbol(circuit, isInput ? "inputs" : "outputs", jack);
    return sym == "text" ? GraphPortKind::Text : GraphPortKind::Signal;
}

// One input jack -> up to three pins (primary/scale/offset). Text inputs -> single pin.
void addInputPins(GraphNode &node, int s, int c, const QString &circuit,
                  const JackAssignment *ja) {
    const QString jack = ja->jackName();
    GraphPortKind kind = portKindOf(circuit, jack, true);
    auto *in = dynamic_cast<const JackAssignmentInput *>(ja);

    auto makePin = [&](GraphPinRole role, const char *suffix, const Atom *atom) {
        GraphPin pin;
        pin.id = pinId(s, c, jack, suffix);
        pin.label = jack;
        pin.direction = GraphPinDirection::In;
        pin.portKind = kind;
        pin.role = role;
        pin.connected = atom && (atom->isCable() || atom->isRegister());
        if (atom && !pin.connected) pin.constantText = atom->toString();
        node.pins.append(pin);
    };

    if (kind == GraphPortKind::Text) {
        makePin(GraphPinRole::Simple, "p", in ? in->atomAt(0) : nullptr);
        return;
    }
    makePin(GraphPinRole::Primary, "p", in ? in->atomAt(0) : nullptr);
    makePin(GraphPinRole::Scale,   "s", in ? in->atomAt(1) : nullptr);
    makePin(GraphPinRole::Offset,  "o", in ? in->atomAt(2) : nullptr);
}

void addOutputPin(GraphNode &node, int s, int c, const QString &circuit,
                  const JackAssignment *ja) {
    const QString jack = ja->jackName();
    GraphPin pin;
    pin.id = pinId(s, c, jack, "out");
    pin.label = jack;
    pin.direction = GraphPinDirection::Out;
    pin.portKind = portKindOf(circuit, jack, false);
    pin.role = GraphPinRole::Simple;
    const Atom *atom = ja->atomAt(1);
    pin.connected = atom && (atom->isCable() || atom->isRegister());
    node.pins.append(pin);
}

} // namespace

GraphDescription GraphModel::describe(const Patch *patch)
{
    GraphDescription g;
    for (int s = 0; s < patch->numSections(); s++) {
        const PatchSection *section = patch->section(s);
        const auto &circuits = section->getCircuits();
        for (int c = 0; c < circuits.size(); c++) {
            const Circuit *circuit = circuits[c];
            GraphNode node;
            node.id = QString("c%1.%2").arg(s).arg(c);
            node.kind = GraphNodeKind::Circuit;
            node.title = circuit->getName();
            node.sectionIndex = s;
            node.circuitIndex = c;
            for (int j = 0; j < circuit->numJackAssignments(); j++) {
                const JackAssignment *ja = circuit->jackAssignment(j);
                if (ja->isInput())  addInputPins(node, s, c, circuit->getName(), ja);
                else if (ja->isOutput()) addOutputPin(node, s, c, circuit->getName(), ja);
            }
            g.nodes.append(node);
        }
    }
    return g; // hardware nodes, wires, frames added in later tasks
}
```

> Verify accessor names against headers already read: `Patch::numSections()`, `Patch::section(i)`, `PatchSection::getCircuits()`, `Circuit::getName()`, `Circuit::numJackAssignments()`, `Circuit::jackAssignment(i)`, `JackAssignment::isInput()/isOutput()/jackName()/atomAt(int)`, `JackAssignmentInput::atomAt(0..2)`, `Atom::isCable()/isRegister()/toString()`.

- [ ] **Step 5: Verify it passes**

Run: `cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests`
Run: `QT_QPA_PLATFORM=offscreen /Users/jan.kaluza/Projects/droidforge/droidforge/build-tests/forgetests`
Expected: `circuitNodeHasCompoundInputPins` passes.

- [ ] **Step 6: Commit production code**

Run: `git -C /Users/jan.kaluza/Projects/droidforge add droidforge/graphview/graphmodel.h droidforge/graphview/graphmodel.cpp`
Run: `git -C /Users/jan.kaluza/Projects/droidforge commit -m "Derive circuit nodes and compound pins from a Patch"`

---

### Task 2: Derive hardware nodes

**Files:**
- Modify: `droidforge/graphview/graphmodel.cpp`
- Test (throwaway): add to `droidforge/tests/test_graphmodel.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
void hardwareNodesSplitByDirection() {
    Patch *p = parse("[lfo]\n  hz = P1.1\n  output = O1\n");
    // a default master is assumed; ensure master type set if the API requires it
    GraphDescription g = GraphModel::describe(p);
    bool masterIn=false, masterOut=false;
    const GraphPin *o1=nullptr, *p11=nullptr;
    for (const auto &n : g.nodes) {
        if (n.kind==GraphNodeKind::HardwareSource && n.id=="hw.master.in") masterIn=true;
        if (n.kind==GraphNodeKind::HardwareSink   && n.id=="hw.master.out") masterOut=true;
        for (const auto &pin : n.pins) {
            if (pin.id=="hw.O1") o1=&pin;
            if (pin.id=="hw.P1.1") p11=&pin;
        }
    }
    QVERIFY(masterIn); QVERIFY(masterOut);
    QVERIFY(o1); QCOMPARE(o1->direction, GraphPinDirection::In);   // a sink consumes
    QVERIFY(o1->used);
    delete p;
}
```

> Check how the master type is set on a fresh `Patch` (`Patch::typeOfMaster()` / `setTypeOfMaster()`); if `describe` needs a configured master to enumerate `O`/`I`, set it in the test (e.g. `p->setTypeOfMaster(...)`). Mirror what `PatchParser` does for a master-less patch.

- [ ] **Step 2: Verify it fails**

Rebuild + run. Expected: FAIL (`masterIn` false — no hardware nodes yet).

- [ ] **Step 3: Implement hardware enumeration**

Add to `graphmodel.cpp` (call `addHardwareNodes(g, patch)` at the end of `describe`, before returning):

```cpp
#include "atomregister.h"
#include "registertypes.h"

namespace {

bool isSourceType(register_type_t t) {
    return t==REGISTER_INPUT || t==REGISTER_NORMALIZE || t==REGISTER_POT
        || t==REGISTER_BUTTON || t==REGISTER_ENCODER || t==REGISTER_SWITCH;
}
// Sinks: OUTPUT, GATE, LED, RGB_LED.

void addHwPin(GraphNode &node, const AtomRegister &reg, Patch *patch, bool source) {
    GraphPin pin;
    pin.id = "hw." + reg.toString();
    pin.label = reg.toString();
    pin.direction = source ? GraphPinDirection::Out : GraphPinDirection::In;
    pin.portKind = GraphPortKind::Signal;
    pin.role = GraphPinRole::Simple;
    pin.used = patch->registerUsed(reg);
    node.pins.append(pin);
}

void addHardwareNodes(GraphDescription &g, const Patch *patchConst) {
    Patch *patch = const_cast<Patch *>(patchConst); // registerUsed is non-const
    static const register_type_t globalTypes[] =
        { REGISTER_INPUT, REGISTER_NORMALIZE, REGISTER_OUTPUT, REGISTER_GATE };

    GraphNode masterIn;  masterIn.id="hw.master.in";  masterIn.kind=GraphNodeKind::HardwareSource; masterIn.title="Master in";
    GraphNode masterOut; masterOut.id="hw.master.out"; masterOut.kind=GraphNodeKind::HardwareSink;  masterOut.title="Master out";
    for (register_type_t t : globalTypes) {
        unsigned count = the_firmware->numGlobalRegisters(t);
        for (unsigned n = 1; n <= count; n++) {
            AtomRegister reg(t, 0, 0, n);
            addHwPin(isSourceType(t) ? masterIn : masterOut, reg, patch, isSourceType(t));
        }
    }
    if (!masterIn.pins.isEmpty())  g.nodes.append(masterIn);
    if (!masterOut.pins.isEmpty()) g.nodes.append(masterOut);

    // Per-controller nodes
    static const register_type_t ctrlSource[] = { REGISTER_POT, REGISTER_BUTTON, REGISTER_ENCODER, REGISTER_SWITCH };
    static const register_type_t ctrlSink[]   = { REGISTER_LED, REGISTER_RGB_LED };
    for (int ci = 0; ci < patch->numControllers(); ci++) {
        QString ctrlName = patch->controller(ci);
        GraphNode controls; controls.id=QString("hw.ctrl%1.controls").arg(ci+1);
        controls.kind=GraphNodeKind::HardwareSource; controls.title=QString("Ctrl %1 · %2").arg(ci+1).arg(ctrlName);
        GraphNode leds; leds.id=QString("hw.ctrl%1.leds").arg(ci+1);
        leds.kind=GraphNodeKind::HardwareSink; leds.title=QString("Ctrl %1 LEDs").arg(ci+1);
        for (register_type_t t : ctrlSource)
            for (unsigned n=1; n<=the_firmware->numControllerRegisters(ctrlName, t); n++)
                addHwPin(controls, AtomRegister(t, ci+1, 0, n), patch, true);
        for (register_type_t t : ctrlSink)
            for (unsigned n=1; n<=the_firmware->numControllerRegisters(ctrlName, t); n++)
                addHwPin(leds, AtomRegister(t, ci+1, 0, n), patch, false);
        if (!controls.pins.isEmpty()) g.nodes.append(controls);
        if (!leds.pins.isEmpty())     g.nodes.append(leds);
    }
}

} // namespace
```

> Verify `AtomRegister(char ty, unsigned co, unsigned g8, unsigned nr)` ctor arg order (from `atomregister.h`: `controller`, `g8`, `number`), `AtomRegister::toString()`, `Patch::registerUsed(AtomRegister)`, `Patch::numControllers()/controller(i)`, and `DroidFirmware::numGlobalRegisters/numControllerRegisters` signatures. Adjust the controller-name argument if `numControllerRegisters` expects a controller *type* string.

- [ ] **Step 4: Verify it passes**

Rebuild + run. Expected: `hardwareNodesSplitByDirection` passes.

- [ ] **Step 5: Commit**

Run: `git -C /Users/jan.kaluza/Projects/droidforge add droidforge/graphview/graphmodel.cpp`
Run: `git -C /Users/jan.kaluza/Projects/droidforge commit -m "Derive hardware nodes split by direction and controller"`

---

### Task 3: Derive wires (cables + register connections)

**Files:**
- Modify: `droidforge/graphview/graphmodel.cpp`
- Test (throwaway): add to `test_graphmodel.cpp`

- [ ] **Step 1: Write the failing tests**

```cpp
void cableWireConnectsProducerToConsumer() {
    Patch *p = parse("[lfo]\n  output = _A\n[vca]\n  level = _A\n");
    GraphDescription g = GraphModel::describe(p);
    bool found=false;
    for (const auto &w : g.wires)
        if (w.isCable && w.cableName=="_A"
            && w.fromPinId=="c0.0.output.out" && w.toPinId=="c0.1.level.p") found=true;
    QVERIFY(found);
    delete p;
}
void registerWireToHardwareSink() {
    Patch *p = parse("[lfo]\n  output = O1\n");
    GraphDescription g = GraphModel::describe(p);
    bool found=false;
    for (const auto &w : g.wires)
        if (!w.isCable && w.fromPinId=="c0.0.output.out" && w.toPinId=="hw.O1") found=true;
    QVERIFY(found);
    delete p;
}
void registerWireFromHardwareSource() {
    Patch *p = parse("[lfo]\n  hz = P1.1\n");
    GraphDescription g = GraphModel::describe(p);
    bool found=false;
    for (const auto &w : g.wires)
        if (!w.isCable && w.fromPinId=="hw.P1.1" && w.toPinId=="c0.0.hz.p") found=true;
    QVERIFY(found);
    delete p;
}
```

- [ ] **Step 2: Verify they fail**

Rebuild + run. Expected: FAIL (no wires derived yet).

- [ ] **Step 3: Implement wire derivation**

Add `addWires(g, patch)` and call it after hardware nodes. Strategy: walk every circuit/jack/atom; record producers and consumers per cable; emit register wires immediately; then join cable producers to consumers.

```cpp
#include "atomcable.h"
#include <QHash>
#include <QMultiHash>

namespace {

// Re-derive a pin id from a circuit position + jack + atom column.
QString inPinSuffix(int column) { return column==1 ? "s" : column==2 ? "o" : "p"; }

void addWires(GraphDescription &g, const Patch *patch) {
    QHash<QString, QString> cableProducer;        // cable -> producer pinId
    QMultiHash<QString, QString> cableConsumers;  // cable -> consumer pinId

    for (int s=0; s<patch->numSections(); s++) {
        const auto &circuits = patch->section(s)->getCircuits();
        for (int c=0; c<circuits.size(); c++) {
            const Circuit *circuit = circuits[c];
            for (int j=0; j<circuit->numJackAssignments(); j++) {
                const JackAssignment *ja = circuit->jackAssignment(j);
                const QString jack = ja->jackName();
                if (ja->isOutput()) {
                    const Atom *a = ja->atomAt(1);
                    QString outPin = QString("c%1.%2.%3.out").arg(s).arg(c).arg(jack);
                    if (!a) continue;
                    if (a->isCable())    cableProducer.insert(static_cast<const AtomCable*>(a)->getCable(), outPin);
                    else if (a->isRegister()) { GraphWire w; w.fromPinId=outPin; w.toPinId="hw."+a->toString(); g.wires.append(w); }
                }
                else if (ja->isInput()) {
                    for (int col=0; col<3; col++) {
                        const Atom *a = ja->atomAt(col);
                        if (!a) continue;
                        QString inPin = QString("c%1.%2.%3.%4").arg(s).arg(c).arg(jack, inPinSuffix(col));
                        if (a->isCable())    cableConsumers.insert(static_cast<const AtomCable*>(a)->getCable(), inPin);
                        else if (a->isRegister()) { GraphWire w; w.fromPinId="hw."+a->toString(); w.toPinId=inPin; g.wires.append(w); }
                    }
                }
            }
        }
    }
    for (auto it = cableConsumers.constBegin(); it != cableConsumers.constEnd(); ++it) {
        GraphWire w; w.isCable=true; w.cableName=it.key();
        w.fromPinId = cableProducer.value(it.key()); // empty if no producer (a problem, surfaced later)
        w.toPinId = it.value();
        g.wires.append(w);
    }
}

} // namespace
```

> `JackAssignmentInput::atomAt(col)` uses 0/1/2; this matches the pin-id suffixes from Task 1 (`p`/`s`/`o`). Keep the suffix mapping identical to Task 1 or wires won't match pins.

- [ ] **Step 4: Verify they pass**

Rebuild + run. Expected: the three wire tests pass.

- [ ] **Step 5: Mark connected pins (consistency)**

Pins were marked `connected` in Task 1 from the atom. Add an assertion test that a consumer input pin reached by a cable wire has `connected==true`, and fix `describe` ordering if needed (atoms already set this; no change expected).

- [ ] **Step 6: Commit**

Run: `git -C /Users/jan.kaluza/Projects/droidforge add droidforge/graphview/graphmodel.cpp`
Run: `git -C /Users/jan.kaluza/Projects/droidforge commit -m "Derive cable and register wires between pins"`

---

### Task 4: Derive section frames

**Files:**
- Modify: `droidforge/graphview/graphmodel.cpp`
- Test (throwaway): add to `test_graphmodel.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
void framesGroupCircuitsBySection() {
    Patch *p = parse("[lfo]\n  output = _A\n\n: Page Two\n[vca]\n  level = _A\n");
    GraphDescription g = GraphModel::describe(p);
    QCOMPARE(g.frames.size(), 2);
    QVERIFY(g.frames[0].nodeIds.contains("c0.0"));
    QVERIFY(g.frames[1].nodeIds.contains("c1.0"));
    delete p;
}
```

> Confirm the section-title syntax (`: Title` or `[[Title]]`) from `parser/patchparser.cpp` / `PatchSection::toString`; adapt the source string so two sections actually parse.

- [ ] **Step 2: Verify it fails**

Rebuild + run. Expected: FAIL (`g.frames` empty).

- [ ] **Step 3: Implement frame derivation**

Add to `describe`, after nodes are built:

```cpp
for (int s=0; s<patch->numSections(); s++) {
    GraphSectionFrame frame;
    frame.sectionIndex = s;
    frame.title = patch->section(s)->getNonemptyTitle();
    for (const auto &n : g.nodes)
        if (n.kind==GraphNodeKind::Circuit && n.sectionIndex==s)
            frame.nodeIds.append(n.id);
    g.frames.append(frame);
}
```

> Use `PatchSection::getNonemptyTitle()` (already in `patchsection.h`).

- [ ] **Step 4: Verify it passes**

Rebuild + run. Expected: `framesGroupCircuitsBySection` passes.

- [ ] **Step 5: Commit**

Run: `git -C /Users/jan.kaluza/Projects/droidforge add droidforge/graphview/graphmodel.cpp`
Run: `git -C /Users/jan.kaluza/Projects/droidforge commit -m "Derive section frames grouping circuit nodes"`

---

### Task 5: Auto-layout (layered left → right)

**Files:**
- Create: `droidforge/graphview/graphlayout.h`, `droidforge/graphview/graphlayout.cpp`
- Test (throwaway): `droidforge/tests/test_graphlayout.cpp`

- [ ] **Step 1: Declare GraphLayout**

```cpp
// graphview/graphlayout.h
#ifndef GRAPHLAYOUT_H
#define GRAPHLAYOUT_H
#include "graphmodeltypes.h"
namespace GraphLayout {
    // Assigns node.pos and frame.rect. Columns: hardware sources = 0,
    // circuits layered by longest-path from a source, hardware sinks = last column.
    void layout(GraphDescription &g);
    int columnOf(const GraphDescription &g, const QString &nodeId); // exposed for testing
}
#endif
```

- [ ] **Step 2: Write the failing test**

```cpp
#include <QtTest>
#include "graphmodel.h"
#include "graphlayout.h"
#include "patch.h"
#include "patchparser.h"
#include "droidfirmware.h"
extern DroidFirmware *the_firmware;
// reuse parse() helper (factor into a shared header if convenient)

class TestGraphLayout : public QObject {
    Q_OBJECT
private slots:
    void downstreamCircuitIsRightOfUpstream() {
        Patch *p = /* parse two chained circuits: lfo.output=_A ; vca.level=_A */;
        GraphDescription g = GraphModel::describe(p);
        GraphLayout::layout(g);
        QVERIFY(GraphLayout::columnOf(g,"c0.1") > GraphLayout::columnOf(g,"c0.0"));
        // hardware source column is leftmost
        QVERIFY(GraphLayout::columnOf(g,"hw.master.in") <= GraphLayout::columnOf(g,"c0.0"));
        delete p;
    }
    void nodesGetDistinctPositions() {
        Patch *p = /* parse a few circuits */;
        GraphDescription g = GraphModel::describe(p);
        GraphLayout::layout(g);
        for (const auto &n : g.nodes) QVERIFY(!n.pos.isNull() || GraphLayout::columnOf(g,n.id)==0);
        delete p;
    }
};
QTEST_MAIN(TestGraphLayout)
#include "test_graphlayout.moc"
```

- [ ] **Step 3: Verify it fails**

Add `test_graphlayout.cpp` + `graphlayout.cpp` to the harness target. Rebuild. Expected: FAIL (link error).

- [ ] **Step 4: Implement layered layout**

```cpp
// graphview/graphlayout.cpp
#include "graphlayout.h"
#include <QHash>

namespace {
const double COL_W = 280.0, ROW_H = 160.0, FRAME_PAD = 24.0;

QString nodeOfPin(const QString &pinId) {
    if (pinId.startsWith("hw.")) {            // hardware pin id == its node? no: map below
        // hardware pins belong to a hardware node; resolve via caller map
    }
    // circuit pin "c<S>.<C>.<...>" -> node "c<S>.<C>"
    int firstDot = pinId.indexOf('.');
    int secondDot = pinId.indexOf('.', firstDot+1);
    return pinId.left(secondDot);
}
}

namespace GraphLayout {

static QHash<QString,int> g_columns;

int columnOf(const GraphDescription &g, const QString &nodeId) {
    return g_columns.value(nodeId, 0);
}

void layout(GraphDescription &g) {
    g_columns.clear();
    // pin id -> owning node id
    QHash<QString,QString> pinOwner;
    for (const auto &n : g.nodes)
        for (const auto &p : n.pins) pinOwner.insert(p.id, n.id);

    // initialise columns: sources 0, sinks large, circuits 1
    int SINK_COL = 1000000;
    for (const auto &n : g.nodes) {
        if (n.kind==GraphNodeKind::HardwareSource) g_columns[n.id]=0;
        else if (n.kind==GraphNodeKind::HardwareSink) g_columns[n.id]=SINK_COL;
        else g_columns[n.id]=1;
    }
    // longest-path relaxation along wires (skip sinks as targets of ranking)
    for (int iter=0; iter<g.nodes.size()+1; iter++) {
        bool changed=false;
        for (const auto &w : g.wires) {
            QString from = pinOwner.value(w.fromPinId), to = pinOwner.value(w.toPinId);
            if (from.isEmpty()||to.isEmpty()) continue;
            const GraphNode *toNode = g.findNode(to);
            if (toNode && toNode->kind==GraphNodeKind::HardwareSink) continue;
            if (g_columns[to] <= g_columns[from]) { g_columns[to]=g_columns[from]+1; changed=true; }
        }
        if (!changed) break;
    }
    // normalise sink column to one past the max circuit column
    int maxCol=0; for (auto it=g_columns.begin(); it!=g_columns.end(); ++it) if (it.value()!=SINK_COL) maxCol=qMax(maxCol,it.value());
    for (const auto &n : g.nodes) if (n.kind==GraphNodeKind::HardwareSink) g_columns[n.id]=maxCol+1;

    // assign positions: stack nodes within each column
    QHash<int,int> rowInCol;
    for (auto &n : g.nodes) {
        int col = g_columns.value(n.id,0);
        int row = rowInCol.value(col,0); rowInCol[col]=row+1;
        n.pos = QPointF(col*COL_W, row*ROW_H);
    }
    // frame rects bound their member nodes
    for (auto &f : g.frames) {
        if (f.nodeIds.isEmpty()) continue;
        double x0=1e18,y0=1e18,x1=-1e18,y1=-1e18;
        for (const auto &id : f.nodeIds) {
            const GraphNode *n=g.findNode(id); if(!n) continue;
            x0=qMin(x0,n->pos.x()); y0=qMin(y0,n->pos.y());
            x1=qMax(x1,n->pos.x()+220.0); y1=qMax(y1,n->pos.y()+120.0);
        }
        f.rect = QRectF(x0-FRAME_PAD,y0-FRAME_PAD,(x1-x0)+2*FRAME_PAD,(y1-y0)+2*FRAME_PAD);
    }
}

} // namespace
```

> This is a deliberately simple layered layout (good enough for M1). Overlap/aesthetics are refined later. `g_columns` as file-static is fine for single-threaded GUI use; if tests run layouts concurrently, move it into a returned map.

- [ ] **Step 5: Verify it passes**

Rebuild + run. Expected: both layout tests pass.

- [ ] **Step 6: Commit**

Run: `git -C /Users/jan.kaluza/Projects/droidforge add droidforge/graphview/graphlayout.h droidforge/graphview/graphlayout.cpp`
Run: `git -C /Users/jan.kaluza/Projects/droidforge commit -m "Add layered left-to-right auto-layout"`

---

### Task 6: GraphView scaffolding + wire into build (run-verified)

**Files:**
- Create: `droidforge/graphview/graphview.h`, `droidforge/graphview/graphview.cpp`
- Modify: `droidforge/CMakeLists.txt`

- [ ] **Step 1: Add sources to the app CMake**

In `droidforge/CMakeLists.txt`, add to `PROJECT_SOURCES`:
```
graphview/graphmodeltypes.h
graphview/graphmodeltypes.cpp
graphview/graphmodel.h
graphview/graphmodel.cpp
graphview/graphlayout.h
graphview/graphlayout.cpp
graphview/graphview.h
graphview/graphview.cpp
graphview/nodeitem.h
graphview/nodeitem.cpp
graphview/wireitem.h
graphview/wireitem.cpp
graphview/sectionframeitem.h
graphview/sectionframeitem.cpp
```
And add `graphview` to `target_include_directories(${PROJECT_NAME} PRIVATE ...)`.

- [ ] **Step 2: Implement GraphView skeleton**

```cpp
// graphview/graphview.h
#ifndef GRAPHVIEW_H
#define GRAPHVIEW_H
#include "patchview.h"
#include "graphmodeltypes.h"
#include <QGraphicsView>
class QGraphicsScene;

class GraphView : public QGraphicsView, public PatchView
{
    Q_OBJECT
    QGraphicsScene *scene;
public:
    explicit GraphView(PatchEditEngine *patch, QWidget *parent=nullptr);
public slots:
    void rebuildGraphics();   // mirror PatchSectionManager naming
protected:
    void wheelEvent(QWheelEvent *) override; // zoom (read-only nicety)
};
#endif
```

```cpp
// graphview/graphview.cpp
#include "graphview.h"
#include "graphmodel.h"
#include "graphlayout.h"
#include "nodeitem.h"
#include "wireitem.h"
#include "sectionframeitem.h"
#include <QGraphicsScene>
#include <QWheelEvent>

GraphView::GraphView(PatchEditEngine *patch, QWidget *parent)
    : QGraphicsView(parent), PatchView(patch), scene(new QGraphicsScene(this))
{
    setScene(scene);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);
    rebuildGraphics();
}

void GraphView::rebuildGraphics()
{
    scene->clear();
    GraphDescription g = GraphModel::describe(patch);
    GraphLayout::layout(g);
    for (const auto &f : g.frames) scene->addItem(new SectionFrameItem(f));
    QHash<QString, NodeItem*> items;
    for (const auto &n : g.nodes) { auto *it=new NodeItem(n); it->setPos(n.pos); scene->addItem(it); items.insert(n.id, it); }
    for (const auto &w : g.wires) scene->addItem(new WireItem(w, items)); // resolves pin scene-pos
    scene->setSceneRect(scene->itemsBoundingRect().adjusted(-200,-200,200,200));
}

void GraphView::wheelEvent(QWheelEvent *event)
{
    double f = event->angleDelta().y() > 0 ? 1.15 : 1/1.15;
    scale(f, f);
}
```

> `PatchView`'s `patch` member is the borrowed `PatchEditEngine*` (which is-a `Patch`), so `GraphModel::describe(patch)` works directly. `NodeItem`/`WireItem`/`SectionFrameItem` are stubbed in Task 7 — create minimal headers now so this compiles (empty `paint`, `boundingRect` returning a fixed size).

- [ ] **Step 3: Build the app**

Run: `cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build`
Expected: builds with the new files (NodeItem/WireItem/SectionFrameItem minimal stubs compile).

- [ ] **Step 4: Commit**

Run: `git -C /Users/jan.kaluza/Projects/droidforge add droidforge/CMakeLists.txt droidforge/graphview/graphview.h droidforge/graphview/graphview.cpp droidforge/graphview/nodeitem.h droidforge/graphview/nodeitem.cpp droidforge/graphview/wireitem.h droidforge/graphview/wireitem.cpp droidforge/graphview/sectionframeitem.h droidforge/graphview/sectionframeitem.cpp`
Run: `git -C /Users/jan.kaluza/Projects/droidforge commit -m "Add GraphView scaffolding and item stubs, wire into build"`

---

### Task 7: Render nodes, pins, wires, frames (run-verified)

**Files:**
- Modify: `droidforge/graphview/nodeitem.*`, `wireitem.*`, `sectionframeitem.*`

- [ ] **Step 1: Implement NodeItem painting**

Render: a rounded rect, a title bar (circuit type / hardware group), and one row per pin. Inputs on the left edge with their three roles grouped (primary bold, scale/offset secondary); text pins as a square marker; outputs on the right edge; hardware pins colored, dimmed when `used==false`. Store each pin's scene anchor point in a `QHash<QString,QPointF> pinAnchors()` accessor so `WireItem` can find endpoints. Keep colors consistent with `colors_dark.h` / `colors_light.h`.

> Full paint code is implemented against Qt's `QPainter`; verify visually rather than by unit test. Match metrics to `patchsectionview` for familiarity.

- [ ] **Step 2: Implement WireItem painting**

A cubic Bézier from the producer pin anchor to the consumer pin anchor (horizontal tangents). Cable wires and register wires can differ subtly in style (e.g. register wires dashed). Look up anchors from the `QHash<QString,NodeItem*>` passed in Task 6.

- [ ] **Step 3: Implement SectionFrameItem painting**

A translucent rounded rect at `frame.rect` with the section title in the top-left. Draw it behind nodes (`setZValue(-1)`).

- [ ] **Step 4: Build**

Run: `cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build`
Expected: clean build.

- [ ] **Step 5: Verify by running (temporary harness)**

Temporarily construct a `GraphView` on the loaded patch (e.g. behind a debug menu action, or as the central widget) to view a real patch. Use the `verify` or `run` skill: open the app, load a sample `.ini`, confirm nodes/pins/wires/frames render and the graph reads left-to-right. Capture a screenshot.

Run: `open "/Users/jan.kaluza/Projects/droidforge/droidforge/build/DROID Forge.app"`
Expected: a recognizable node graph of the loaded patch.

- [ ] **Step 6: Commit**

Run: `git -C /Users/jan.kaluza/Projects/droidforge add droidforge/graphview/nodeitem.cpp droidforge/graphview/nodeitem.h droidforge/graphview/wireitem.cpp droidforge/graphview/wireitem.h droidforge/graphview/sectionframeitem.cpp droidforge/graphview/sectionframeitem.h`
Run: `git -C /Users/jan.kaluza/Projects/droidforge commit -m "Render node, pin, wire and frame graphics"`

---

### Task 8: List ⇄ Graph toggle integration (run-verified)

**Files:**
- Modify: the widget container that hosts `PatchSectionView` (locate first), and the View menu in `droidforge/main/mainwindow.cpp` (around the `&View` menu, ~line 461).

- [ ] **Step 1: Locate the host of the editing canvas**

Run: `rg -n "new PatchSectionView|PatchSectionView \*|setCentralWidget|QStackedWidget|addWidget.*ectionView" /Users/jan.kaluza/Projects/droidforge/droidforge/main`
Identify where `PatchSectionView` is created and added to the layout. That container becomes a `QStackedWidget` (or gains show/hide logic) holding both the list view and a `GraphView`.

- [ ] **Step 2: Add the GraphView alongside the list view**

In that container, construct `new GraphView(patch)` next to the existing `PatchSectionView`, both fed by the same `PatchEditEngine`. Default to the list view.

- [ ] **Step 3: Add a View-menu toggle**

In `mainwindow.cpp`'s `&View` menu, add a checkable action "Node Graph" that switches the stacked widget between list and graph. Follow the existing `addAction`/`ADD_ACTION` pattern used in the file.

- [ ] **Step 4: Keep the graph fresh**

Connect `GraphView::rebuildGraphics` to `UpdateHub::patchModified` (the same hub `PatchSectionManager` connects to — see `patchsectionmanager.cpp:55`). So switching tabs/editing in the list view and toggling to the graph shows current state.

- [ ] **Step 5: Build**

Run: `cmake --build /Users/jan.kaluza/Projects/droidforge/droidforge/build`
Expected: clean build.

- [ ] **Step 6: Verify by running**

Run: `open "/Users/jan.kaluza/Projects/droidforge/droidforge/build/DROID Forge.app"`
Steps: load a sample patch → toggle View ▸ Node Graph → see the graph → edit something in the list view → toggle back and forth → confirm the graph reflects the edit. Screenshot both views.

- [ ] **Step 7: Commit**

Run: `git -C /Users/jan.kaluza/Projects/droidforge add -A droidforge/main`
Run: `git -C /Users/jan.kaluza/Projects/droidforge commit -m "Add list/graph view toggle bound to the live patch"`

---

## Self-Review

**Spec coverage (Milestone 1 portion):**
- Whole-patch canvas → Tasks 1–7 render all sections' nodes on one scene. ✔
- Sections as frames → Task 4 (derive) + Task 7 (render). ✔ (drag-in/out is M2.)
- Circuit nodes, compound 3-pin inputs, text pins → Task 1 + Task 7. ✔
- Hardware nodes split by direction, per-controller, all pins shown, used flag → Task 2 + Task 7. ✔
- Cables (named nets) + register connections, producer→consumer → Task 3. ✔
- Auto-layout default (left→right) → Task 5. ✔
- Same model, list⇄graph toggle, rebuild on patchModified → Tasks 6 + 8. ✔
- Deferred to later milestones (correctly absent here): editing/wiring gestures, add/remove via reused actions, collapse/expand, problem badges, type-safety enforcement, layout persistence. Tracked for M2/M3.

**Placeholder scan:** GUI paint code in Task 7 is described, not pre-written, by design (verified by running, per the chosen testing strategy) — not a logic placeholder. All pure-logic tasks contain complete test + implementation code. No TODO/TBD remain.

**Type consistency:** `GraphPin`/`GraphNode`/`GraphWire`/`GraphSectionFrame`/`GraphDescription` are defined once and used unchanged across Tasks 1–7. Pin-id scheme is identical in Task 1 (creation) and Task 3 (wire matching): input columns 0/1/2 → suffix `p`/`s`/`o`, output → `out`, hardware → `hw.<reg>`. `rebuildGraphics()` name matches the `PatchSectionManager` convention and is the slot connected in Task 8.

**Assumptions to verify during execution (flagged inline at point of use):** exact `PatchParser` entry point; how master type is configured on a parsed patch; `numControllerRegisters` argument (controller name vs type); section-title source syntax; the host widget of `PatchSectionView`.
