#include "graphmodel.h"
#include "rackmodules.h"
#include "patch.h"
#include "patchsection.h"
#include "circuit.h"
#include "jackassignment.h"
#include "jackassignmentinput.h"
#include "atom.h"
#include "atomcable.h"
#include "droidfirmware.h"
#include "atomregister.h"
#include "registertypes.h"
#include <QHash>
#include <QMultiHash>

namespace {

QString pinId(int s, int c, const QString &jack, const char *suffix)
{
    return QString("c%1.%2.%3.%4").arg(s).arg(c).arg(jack).arg(suffix);
}

// atomAt() is 1-based: column 1=PRIMARY, 2=SCALE, 3=OFFSET.
// Both addInputPins and addWires must use the same mapping.
static constexpr char kHwPrefix[] = "hw.";

const char *inputAtomSuffix(int column)
{
    switch (column) {
    case 1: return "p";
    case 2: return "s";
    case 3: return "o";
    default: return "";
    }
}

GraphPortKind portKindOf(const QString &circuit, const QString &jack, bool isInput)
{
    QString sym = the_firmware->jackTypeSymbol(circuit, isInput ? "inputs" : "outputs", jack);
    return sym == "text" ? GraphPortKind::Text : GraphPortKind::Signal;
}

void addInputPins(GraphNode &node, int s, int c, const QString &circuit, const JackAssignment *ja)
{
    const QString jack = ja->jackName();
    GraphPortKind kind = portKindOf(circuit, jack, true);
    const auto *in = dynamic_cast<const JackAssignmentInput *>(ja);

    auto makePin = [&](GraphPinRole role, const char *suffix, const Atom *atom) {
        GraphPin pin;
        pin.id = pinId(s, c, jack, suffix);
        pin.label = jack;
        pin.direction = GraphPinDirection::In;
        pin.portKind = kind;
        pin.role = role;
        pin.connected = atom && (atom->isCable() || atom->isRegister());
        if (atom && !pin.connected)
            pin.constantText = atom->toString();
        node.pins.append(pin);
    };

    if (kind == GraphPortKind::Text) {
        makePin(GraphPinRole::Simple, "p", in ? in->atomAt(1) : nullptr);
        return;
    }
    makePin(GraphPinRole::Primary, "p", in ? in->atomAt(1) : nullptr);
    makePin(GraphPinRole::Scale,   "s", in ? in->atomAt(2) : nullptr);
    makePin(GraphPinRole::Offset,  "o", in ? in->atomAt(3) : nullptr);
}

void addOutputPin(GraphNode &node, int s, int c, const QString &circuit, const JackAssignment *ja)
{
    const QString jack = ja->jackName();
    GraphPin pin;
    pin.id = pinId(s, c, jack, "out");
    pin.label = jack;
    pin.direction = GraphPinDirection::Out;
    pin.portKind = portKindOf(circuit, jack, false);
    pin.role = GraphPinRole::Simple;
    // JackAssignmentOutput::atomAt(a) returns the atom only for a >= 1
    const Atom *atom = ja->atomAt(1);
    pin.connected = atom && (atom->isCable() || atom->isRegister());
    node.pins.append(pin);
}

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

void addWires(GraphDescription &g, const Patch *patch)
{
    QHash<QString, QString> cableProducer;        // cable name -> producer pinId
    QMultiHash<QString, QString> cableConsumers;  // cable name -> consumer pinId

    for (qsizetype s = 0; s < patch->numSections(); s++) {
        const QList<Circuit *> &circuits = patch->section(s)->getCircuits();
        for (qsizetype c = 0; c < circuits.size(); c++) {
            const Circuit *circuit = circuits[c];
            for (qsizetype j = 0; j < circuit->numJackAssignments(); j++) {
                const JackAssignment *ja = circuit->jackAssignment(static_cast<unsigned>(j));
                const QString jack = ja->jackName();

                if (ja->isOutput()) {
                    const Atom *a = ja->atomAt(1);
                    if (!a) continue;
                    QString outPin = pinId(static_cast<int>(s), static_cast<int>(c), jack, "out");
                    if (a->isCable()) {
                        cableProducer.insert(static_cast<const AtomCable *>(a)->getCable(), outPin);
                    } else if (a->isRegister()) {
                        GraphWire w;
                        w.fromPinId = outPin;
                        w.toPinId   = QString(kHwPrefix) + a->toString();
                        g.wires.append(w);
                    }
                } else if (ja->isInput()) {
                    const bool isText = (portKindOf(circuit->getName(), jack, true) == GraphPortKind::Text);
                    const int maxCol = isText ? 1 : 3;
                    for (int col = 1; col <= maxCol; col++) {
                        const Atom *a = ja->atomAt(col);
                        if (!a) continue;
                        QString inPin = pinId(static_cast<int>(s), static_cast<int>(c), jack, inputAtomSuffix(col));
                        if (a->isCable()) {
                            cableConsumers.insert(static_cast<const AtomCable *>(a)->getCable(), inPin);
                        } else if (a->isRegister()) {
                            const AtomRegister &areg =
                                *static_cast<const AtomRegister *>(a);
                            GraphWire w;
                            w.fromPinId = hwReadPinId(areg, patch); // hw.<reg>.read for outputs
                            w.toPinId   = inPin;
                            g.wires.append(w);
                        }
                    }
                }
            }
        }
    }

    for (auto it = cableConsumers.constBegin(); it != cableConsumers.constEnd(); ++it) {
        GraphWire w;
        w.isCable    = true;
        w.cableName  = it.key();
        w.fromPinId  = cableProducer.value(it.key());
        w.toPinId    = it.value();
        g.wires.append(w);
    }
}

} // namespace

GraphDescription GraphModel::describe(const Patch *patch, const RackVisibilitySettings &vis)
{
    GraphDescription g;
    for (qsizetype s = 0; s < patch->numSections(); s++) {
        const PatchSection *section = patch->section(s);
        const QList<Circuit *> &circuits = section->getCircuits();
        for (qsizetype c = 0; c < circuits.size(); c++) {
            const Circuit *circuit = circuits[c];
            GraphNode node;
            node.id = QString("c%1.%2").arg(s).arg(c);
            node.kind = GraphNodeKind::Circuit;
            node.title = circuit->getName();
            node.sectionIndex = static_cast<int>(s);
            node.circuitIndex = static_cast<int>(c);
            for (qsizetype j = 0; j < circuit->numJackAssignments(); j++) {
                const JackAssignment *ja = circuit->jackAssignment(static_cast<unsigned>(j));
                if (ja->isInput())
                    addInputPins(node, static_cast<int>(s), static_cast<int>(c), circuit->getName(), ja);
                else if (ja->isOutput())
                    addOutputPin(node, static_cast<int>(s), static_cast<int>(c), circuit->getName(), ja);
            }
            g.nodes.append(node);
        }
    }
    addHardwareNodes(g, patch, vis);
    addWires(g, patch);

    for (qsizetype s = 0; s < patch->numSections(); s++) {
        GraphSectionFrame frame;
        frame.sectionIndex = static_cast<int>(s);
        frame.title = patch->section(s)->getNonemptyTitle();
        for (const auto &n : g.nodes)
            if (n.kind == GraphNodeKind::Circuit && n.sectionIndex == static_cast<int>(s))
                frame.nodeIds.append(n.id);
        g.frames.append(frame);
    }

    return g;
}
