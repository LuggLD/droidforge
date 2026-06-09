#ifndef GRAPHEDITS_H
#define GRAPHEDITS_H

#include "graphmodeltypes.h"   // GraphPortKind
#include <QString>
#include <QStringList>

class Patch;

namespace GraphEdits {

// Which gesture is being performed; set by GraphView from keyboard modifiers.
enum class DragMode { Connect, Copy, Move, Rehome };

// Parsed + classified pin id. Sources fan out; sinks hold one wire.
struct PinRef {
    enum Kind { CircuitInput, CircuitOutput, HwWrite, HwRead, HwSource, Invalid };
    Kind kind = Invalid;
    int section = -1;     // circuit pins only
    int circuit = -1;     // circuit pins only
    QString jack;         // circuit pins only
    int column = 1;       // circuit inputs: 1=primary/2=scale/3=offset; else 1
    QString reg;          // hardware pins only (register string, e.g. "O1", "P1.1")

    bool isSource() const { return kind == CircuitOutput || kind == HwRead || kind == HwSource; }
    bool isSink()   const { return kind == CircuitInput  || kind == HwWrite; }
    bool valid()    const { return kind != Invalid; }
};

// --- classification / validity (const, pure) ---
PinRef        parsePin(const Patch *patch, const QString &pinId);
GraphPortKind portKindOfPin(const Patch *patch, const PinRef &ref);
bool          isValidDrop(const Patch *patch, const QString &fromPin,
                          const QString &toPin, DragMode mode);

// --- queries (const, pure) ---
QStringList getConnectedSinks(const Patch *patch, const QString &sourcePin);
QString     getConnectedSource(const Patch *patch, const QString &sinkPin);

// --- mutations (modify the patch model; return true on success; DO NOT commit) ---
bool connectPins(Patch *patch, const QString &fromPin, const QString &toPin);
bool copyWire(Patch *patch, const QString &fromSink, const QString &toSink);
bool moveWire(Patch *patch, const QString &fromSink, const QString &toSink);
bool rehomeWires(Patch *patch, const QString &fromSource, const QString &toSource);
bool disconnectWire(Patch *patch, const QString &fromPin, const QString &toPin);
bool disconnectPin(Patch *patch, const QString &pin);

} // namespace GraphEdits

#endif
