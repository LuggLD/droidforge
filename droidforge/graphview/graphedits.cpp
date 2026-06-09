#include "graphedits.h"
#include "patch.h"
#include "patchsection.h"
#include "circuit.h"
#include "jackassignment.h"
#include "atom.h"
#include "atomcable.h"
#include "atomregister.h"
#include "droidfirmware.h"

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
        else if (patch->registerIsOutputOnly(areg))
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

// Does a source pin currently hold a net (a producer atom)?
static bool sourceHasNet(const Patch *patch, const PinRef &ref)
{
    if (ref.kind == PinRef::CircuitOutput) {
        const Circuit *circ = patch->section(ref.section)->circuit(ref.circuit);
        const JackAssignment *ja = circ->findJack(ref.jack);
        return ja && ja->atomAt(1) != nullptr;
    }
    // Hardware sources always "exist" but can't be re-homed onto (see isValidDrop).
    return true;
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
        break;
    case DragMode::Copy:
    case DragMode::Move:
        if (!(a.isSink() && b.isSink()))
            return false;
        break;
    case DragMode::Rehome:
        // Re-home moves a producer onto another *circuit output* that is empty.
        if (!(a.isSource() && b.kind == PinRef::CircuitOutput))
            return false;
        if (sourceHasNet(patch, b))
            return false; // edge #2: never clobber an occupied source
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

} // namespace GraphEdits
