#include "graphmodel.h"
#include "patch.h"
#include "patchsection.h"
#include "circuit.h"
#include "jackassignment.h"
#include "jackassignmentinput.h"
#include "atom.h"
#include "droidfirmware.h"

namespace {

QString pinId(int s, int c, const QString &jack, const char *suffix)
{
    return QString("c%1.%2.%3.%4").arg(s).arg(c).arg(jack).arg(suffix);
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
        makePin(GraphPinRole::Simple, "p", in ? in->atomAt(0) : nullptr);
        return;
    }
    makePin(GraphPinRole::Primary, "p", in ? in->atomAt(0) : nullptr);
    makePin(GraphPinRole::Scale,   "s", in ? in->atomAt(1) : nullptr);
    makePin(GraphPinRole::Offset,  "o", in ? in->atomAt(2) : nullptr);
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
    return g;
}
