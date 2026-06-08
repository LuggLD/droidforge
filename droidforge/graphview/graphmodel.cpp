#include "graphmodel.h"
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

// Keep in sync with Patch::registerIsOutputOnly() in patch.cpp.
bool isSourceRegisterType(register_type_t t) {
    return t == REGISTER_INPUT || t == REGISTER_NORMALIZE
        || t == REGISTER_POT   || t == REGISTER_BUTTON
        || t == REGISTER_ENCODER || t == REGISTER_SWITCH;
}

void addHwPin(GraphNode &node, const AtomRegister &reg, Patch *patch, bool source)
{
    GraphPin pin;
    pin.id        = QString(kHwPrefix) + reg.toString();
    pin.label     = reg.toString();
    pin.direction = source ? GraphPinDirection::Out : GraphPinDirection::In;
    pin.portKind  = GraphPortKind::Signal;
    pin.role      = GraphPinRole::Simple;
    pin.used      = patch->registerUsed(reg);
    node.pins.append(pin);
}

void addHardwareNodes(GraphDescription &g, const Patch *patchConst)
{
    // registerUsed is logically const (only iterates, never mutates); cast is safe.
    Patch *patch = const_cast<Patch *>(patchConst);

    // Master I/O nodes
    static const register_type_t globalTypes[] = {
        REGISTER_INPUT, REGISTER_NORMALIZE, REGISTER_OUTPUT, REGISTER_GATE
    };
    GraphNode masterIn;
    masterIn.id    = "hw.master.in";
    masterIn.kind  = GraphNodeKind::HardwareSource;
    masterIn.title = "Master in";

    GraphNode masterOut;
    masterOut.id    = "hw.master.out";
    masterOut.kind  = GraphNodeKind::HardwareSink;
    masterOut.title = "Master out";

    for (register_type_t t : globalTypes) {
        unsigned count = the_firmware->numGlobalRegisters(t);
        for (unsigned n = 1; n <= count; n++) {
            AtomRegister reg(t, 0, 0, n);
            bool source = isSourceRegisterType(t);
            addHwPin(source ? masterIn : masterOut, reg, patch, source);
        }
    }
    if (!masterIn.pins.isEmpty())  g.nodes.append(masterIn);
    if (!masterOut.pins.isEmpty()) g.nodes.append(masterOut);

    // Per-controller nodes
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
                addHwPin(controls, AtomRegister(t, static_cast<unsigned>(ci + 1), 0, n), patch, true);
        }
        for (register_type_t t : ctrlSink) {
            unsigned count = the_firmware->numControllerRegisters(ctrlName, t);
            for (unsigned n = 1; n <= count; n++)
                addHwPin(leds, AtomRegister(t, static_cast<unsigned>(ci + 1), 0, n), patch, false);
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
                            GraphWire w;
                            w.fromPinId = QString(kHwPrefix) + a->toString();
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

GraphDescription GraphModel::describe(const Patch *patch)
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
    addHardwareNodes(g, patch);
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
