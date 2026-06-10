#include "graphedits.h"
#include "patch.h"
#include "patchsection.h"
#include "circuit.h"
#include "jackassignment.h"
#include "atom.h"
#include "atomcable.h"
#include "atomregister.h"
#include "droidfirmware.h"
#include "rackmodules.h"

extern DroidFirmware *the_firmware;

namespace GraphEdits {

PinRef parsePin(const Patch *patch, const QString &pinId)
{
    PinRef ref;
    if (pinId.startsWith("hw.")) {
        QString rest = pinId.mid(3);
        bool isRead = rest.endsWith(".read");
        if (isRead)
            rest.chop(5); // strip ".read"
        ref.reg = rest;
        ref.column = 1;
        AtomRegister areg(rest);
        if (isRead)
            ref.kind = PinRef::HwRead;
        else if (patch->registerIsOutputOnly(areg) || registerIsBidirectional(patch, areg))
            ref.kind = PinRef::HwWrite;
        else
            ref.kind = PinRef::HwSource;
        return ref;
    }
    if (pinId.startsWith("c")) {
        // c<S>.<C>.<jack>.<suffix>   (jack may itself contain '.')
        const QStringList parts = pinId.mid(1).split('.');
        if (parts.size() < 3)
            return ref; // Invalid
        bool ok1 = false, ok2 = false;
        const int s = parts.first().toInt(&ok1);
        const int c = parts.at(1).toInt(&ok2);
        if (!ok1 || !ok2)
            return ref;
        const QString suffix = parts.last();
        ref.section = s;
        ref.circuit = c;
        ref.jack = parts.mid(2, parts.size() - 3).join('.');
        if (suffix == "p")        { ref.kind = PinRef::CircuitInput;  ref.column = 1; }
        else if (suffix == "s")   { ref.kind = PinRef::CircuitInput;  ref.column = 2; }
        else if (suffix == "o")   { ref.kind = PinRef::CircuitInput;  ref.column = 3; }
        else if (suffix == "out") { ref.kind = PinRef::CircuitOutput; ref.column = 1; }
        return ref;
    }
    return ref;
}

GraphPortKind portKindOfPin(const Patch *patch, const PinRef &ref)
{
    if (ref.kind == PinRef::CircuitInput || ref.kind == PinRef::CircuitOutput) {
        const Circuit *circ = patch->section(ref.section)->circuit(ref.circuit);
        const QString sym = the_firmware->jackTypeSymbol(
            circ->getName(),
            ref.kind == PinRef::CircuitInput ? "inputs" : "outputs",
            ref.jack);
        return sym == "text" ? GraphPortKind::Text : GraphPortKind::Signal;
    }
    return GraphPortKind::Signal; // all hardware registers are signal
}

bool isValidDrop(const Patch *patch, const QString &fromPin,
                 const QString &toPin, DragMode mode)
{
    if (fromPin == toPin || fromPin.isEmpty() || toPin.isEmpty())
        return false;
    const PinRef a = parsePin(patch, fromPin);
    const PinRef b = parsePin(patch, toPin);
    if (!a.valid() || !b.valid())
        return false;

    switch (mode) {
    case DragMode::Connect:
        if (!((a.isSource() && b.isSink()) || (a.isSink() && b.isSource())))
            return false;
        // A hardware write pin can only be driven by a circuit output: the
        // connection is stored as the register atom on the producing jack, so
        // a hardware source feeding a hardware sink has no patch
        // representation (connectPins would refuse — keep the two in sync).
        {
            const PinRef &srcRef = a.isSource() ? a : b;
            const PinRef &snkRef = a.isSink()   ? a : b;
            if (snkRef.kind == PinRef::HwWrite && srcRef.kind != PinRef::CircuitOutput)
                return false;
        }
        break;
    case DragMode::Copy:
    case DragMode::Move:
        if (!(a.isSink() && b.isSink()))
            return false;
        break;
    case DragMode::Rehome:
        // Re-home moves a producer's whole net onto another circuit output. If
        // the target already produces a net, the two MERGE: every sink of both
        // sources ends up reading the target. (Target must be a re-assignable
        // producer, i.e. a circuit output, not a hardware source.)
        if (!(a.isSource() && b.kind == PinRef::CircuitOutput))
            return false;
        break;
    }

    if (portKindOfPin(patch, a) != portKindOfPin(patch, b))
        return false; // signal vs text
    return true;
}

// Resolve a parsed pin to its JackAssignment (or nullptr for hardware pins).
// Uses the public (non-const) jackAssignment(i) / numJackAssignments() path because
// Circuit::findJack(name) (non-const overload) is private.
static JackAssignment *jackFor(Patch *patch, const PinRef &ref)
{
    if (ref.kind == PinRef::CircuitInput || ref.kind == PinRef::CircuitOutput) {
        Circuit *circ = patch->section(ref.section)->circuit(ref.circuit);
        for (qsizetype i = 0; i < circ->numJackAssignments(); ++i) {
            JackAssignment *ja = circ->jackAssignment(static_cast<unsigned>(i));
            if (ja->jackName() == ref.jack)
                return ja;
        }
    }
    return nullptr;
}

// The atom a source pin offers to a circuit-input sink (cloned for the caller).
// For a circuit output this may MINT a cable and store it on the output.
static Atom *netAtomForInput(Patch *patch, const PinRef &src)
{
    if (src.kind == PinRef::HwSource || src.kind == PinRef::HwRead)
        return new AtomRegister(src.reg);

    if (src.kind == PinRef::CircuitOutput) {
        JackAssignment *out = jackFor(patch, src);
        if (!out)
            return nullptr;
        const Atom *cur = out->atomAt(1);
        if (cur)
            return cur->clone();          // reuse existing cable, or read-back a register (edge #1)
        const QString name = patch->freshCableName();
        out->replaceAtom(1, new AtomCable(name)); // mint + store on producer
        return new AtomCable(name);
    }
    return nullptr;
}

bool connectPins(Patch *patch, const QString &fromPin, const QString &toPin)
{
    PinRef a = parsePin(patch, fromPin);
    PinRef b = parsePin(patch, toPin);
    if (!a.valid() || !b.valid())
        return false;
    // identify source + sink regardless of drag order
    PinRef src = a.isSource() ? a : b;
    PinRef snk = a.isSink()   ? a : b;
    if (!src.isSource() || !snk.isSink())
        return false;

    if (snk.kind == PinRef::CircuitInput) {
        Atom *atom = netAtomForInput(patch, src);
        if (!atom)
            return false;
        JackAssignment *in = jackFor(patch, snk);
        if (!in) { delete atom; return false; }
        in->replaceAtom(snk.column, atom);
        return true;
    }
    if (snk.kind == PinRef::HwWrite) {
        // out -> hw sink: the atom lives on the producing circuit output.
        if (src.kind != PinRef::CircuitOutput)
            return false;
        JackAssignment *out = jackFor(patch, src);
        if (!out)
            return false;
        out->replaceAtom(1, new AtomRegister(snk.reg));
        return true;
    }
    return false;
}

// Build the canonical pin id for a circuit input column.
static QString inputPinId(int s, int c, const QString &jack, int column)
{
    const char *suffix = column == 1 ? "p" : column == 2 ? "s" : "o";
    return QString("c%1.%2.%3.%4").arg(s).arg(c).arg(jack).arg(suffix);
}

// The net key a source produces: the cable name, or the register string, or empty.
static QString sourceNetKey(const Patch *patch, const PinRef &src)
{
    if (src.kind == PinRef::HwSource || src.kind == PinRef::HwRead)
        return src.reg;
    if (src.kind == PinRef::CircuitOutput) {
        const JackAssignment *out =
            patch->section(src.section)->circuit(src.circuit)->findJack(src.jack);
        const Atom *a = out ? out->atomAt(1) : nullptr;
        return a ? a->toString() : QString();
    }
    return QString();
}

// Visit every circuit input column atom in the patch.
template <class Fn>
static void forEachInputAtom(const Patch *patch, Fn fn)
{
    for (qsizetype s = 0; s < patch->numSections(); s++) {
        const PatchSection *sec = patch->section(s);
        for (unsigned c = 0; c < sec->numCircuits(); c++) {
            const Circuit *circ = sec->circuit(c);
            for (qsizetype j = 0; j < circ->numJackAssignments(); j++) {
                const JackAssignment *ja = circ->jackAssignment(static_cast<unsigned>(j));
                if (!ja->isInput())
                    continue;
                for (int col = 1; col <= 3; col++) {
                    const Atom *a = ja->atomAt(col);
                    if (a)
                        fn(static_cast<int>(s), static_cast<int>(c), ja->jackName(), col, a);
                }
            }
        }
    }
}

QStringList getConnectedSinks(const Patch *patch, const QString &sourcePin)
{
    QStringList out;
    const PinRef src = parsePin(patch, sourcePin);
    if (!src.isSource())
        return out;

    // A circuit output that drives a hardware sink (atom is a register): the sink is hw.<reg>.
    if (src.kind == PinRef::CircuitOutput) {
        const JackAssignment *o =
            patch->section(src.section)->circuit(src.circuit)->findJack(src.jack);
        const Atom *a = o ? o->atomAt(1) : nullptr;
        if (a && a->isRegister()) {
            out << (QString("hw.") + a->toString());
            return out;
        }
    }

    const QString key = sourceNetKey(patch, src);
    if (key.isEmpty())
        return out;
    forEachInputAtom(patch, [&](int s, int c, const QString &jack, int col, const Atom *a) {
        if (a->toString() == key)
            out << inputPinId(s, c, jack, col);
    });
    return out;
}

// The circuit-output pin id whose atom stringifies to value, or empty.
static QString findOutputHolding(const Patch *patch, const QString &value)
{
    for (qsizetype s = 0; s < patch->numSections(); s++) {
        const PatchSection *sec = patch->section(s);
        for (unsigned c = 0; c < sec->numCircuits(); c++) {
            const Circuit *circ = sec->circuit(c);
            for (qsizetype j = 0; j < circ->numJackAssignments(); j++) {
                const JackAssignment *ja = circ->jackAssignment(static_cast<unsigned>(j));
                if (ja->isOutput() && ja->atomAt(1) && ja->atomAt(1)->toString() == value)
                    return QString("c%1.%2.%3.out").arg(s).arg(c).arg(ja->jackName());
            }
        }
    }
    return QString();
}

QString getConnectedSource(const Patch *patch, const QString &sinkPin)
{
    const PinRef snk = parsePin(patch, sinkPin);
    if (!snk.isSink())
        return QString();

    if (snk.kind == PinRef::CircuitInput) {
        const JackAssignment *in =
            patch->section(snk.section)->circuit(snk.circuit)->findJack(snk.jack);
        const Atom *a = in ? in->atomAt(snk.column) : nullptr;
        if (!a)
            return QString();
        if (a->isRegister()) {
            AtomRegister areg(a->toString());
            return (patch->registerIsOutputOnly(areg) || registerIsBidirectional(patch, areg))
                       ? (QString("hw.") + a->toString() + ".read")
                       : (QString("hw.") + a->toString());
        }
        if (a->isCable())
            return findOutputHolding(patch, a->toString());
        return QString();
    }

    // Hardware write pin: the producer is the circuit output whose atom == this register.
    if (snk.kind == PinRef::HwWrite)
        return findOutputHolding(patch, snk.reg);

    return QString();
}

// Clear the atom physically stored at a sink (circuit input) or producer (circuit output).
static void clearJackAtom(Patch *patch, const PinRef &ref)
{
    JackAssignment *ja = jackFor(patch, ref);
    if (!ja)
        return;
    ja->replaceAtom(ref.kind == PinRef::CircuitInput ? ref.column : 1, nullptr);
}

bool disconnectWire(Patch *patch, const QString &fromPin, const QString &toPin)
{
    const PinRef a = parsePin(patch, fromPin);
    const PinRef b = parsePin(patch, toPin);
    if (!a.valid() || !b.valid())
        return false;
    // The connecting atom is stored at the circuit input, if one is involved.
    if (a.kind == PinRef::CircuitInput) { clearJackAtom(patch, a); return true; }
    if (b.kind == PinRef::CircuitInput) { clearJackAtom(patch, b); return true; }
    // Otherwise it is a circuit-output -> hw-sink wire: clear the output.
    if (a.kind == PinRef::CircuitOutput) { clearJackAtom(patch, a); return true; }
    if (b.kind == PinRef::CircuitOutput) { clearJackAtom(patch, b); return true; }
    return false;
}

bool disconnectPin(Patch *patch, const QString &pin)
{
    const PinRef ref = parsePin(patch, pin);
    if (!ref.valid())
        return false;

    if (ref.kind == PinRef::CircuitInput) {
        clearJackAtom(patch, ref);
        return true;
    }
    if (ref.kind == PinRef::HwWrite) {
        const QString srcPin = getConnectedSource(patch, pin);
        if (!srcPin.isEmpty())
            clearJackAtom(patch, parsePin(patch, srcPin));
        return true;
    }
    // Source: clear every reader, then (for a circuit output) clear the producer.
    const QStringList sinks = getConnectedSinks(patch, pin);
    for (const QString &sinkPin : sinks) {
        const PinRef snk = parsePin(patch, sinkPin);
        if (snk.kind == PinRef::CircuitInput)
            clearJackAtom(patch, snk);
    }
    if (ref.kind == PinRef::CircuitOutput)
        clearJackAtom(patch, ref);
    return true;
}

bool copyWire(Patch *patch, const QString &fromSink, const QString &toSink)
{
    const QString src = getConnectedSource(patch, fromSink);
    if (src.isEmpty())
        return false;
    return connectPins(patch, src, toSink);
}

bool moveWire(Patch *patch, const QString &fromSink, const QString &toSink)
{
    if (!copyWire(patch, fromSink, toSink))
        return false;
    clearJackAtom(patch, parsePin(patch, fromSink));
    return true;
}

bool rehomeWires(Patch *patch, const QString &fromSource, const QString &toSource)
{
    const PinRef from = parsePin(patch, fromSource);
    if (!from.isSource())
        return false;
    const QStringList sinks = getConnectedSinks(patch, fromSource); // capture before mutating
    for (const QString &sinkPin : sinks)
        connectPins(patch, toSource, sinkPin);
    if (from.kind == PinRef::CircuitOutput)
        clearJackAtom(patch, from);
    return true;
}

} // namespace GraphEdits
