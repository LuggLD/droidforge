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
    QString id;             // globally unique, stable: c<S>.<C>.<jack>.[p|s|o|out] or hw.<reg>
    QString label;          // shown text, e.g. "hz", "P1.1", "output"
    GraphPinDirection direction = GraphPinDirection::In;
    GraphPortKind portKind = GraphPortKind::Signal;
    GraphPinRole role = GraphPinRole::Simple;
    bool connected = false; // a wire (cable or register) is attached
    bool used = false;      // hardware pins only: referenced somewhere in the patch
    QString constantText;   // value shown when unconnected (number/text); empty if none
};

struct GraphNode {
    QString id;             // "c<S>.<C>" or "hw.<key>"
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

    // Returns a pointer into nodes; valid only while nodes is not modified.
    const GraphNode *findNode(const QString &id) const;
};

#endif // GRAPHMODELTYPES_H
