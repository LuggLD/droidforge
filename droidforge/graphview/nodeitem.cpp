#include "nodeitem.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>

// ─── Color helpers ────────────────────────────────────────────────────────────
// We use hand-picked QColors that are legible on any background. These are
// loosely derived from the app's dark palette values noted in colors_dark.h.

static QColor nodeBg()          { return QColor(43, 43, 43);   }  // #350 ≈
static QColor nodeOutline()     { return QColor(103, 103, 103); }  // #351
static QColor titleTextColor()  { return QColor(255, 255, 255); }
static QColor bodyTextColor()   { return QColor(205, 205, 205); }  // #405
static QColor dimTextColor()    { return QColor(103, 103, 103); }  // #351

// Title-bar accent colors per node kind
static QColor accentCircuit()   { return QColor(56, 146, 28);   }  // green  #101
static QColor accentHwSource()  { return QColor(0, 120, 148);   }  // blue   #201-ish
static QColor accentHwSink()    { return QColor(215, 124, 0);   }  // orange #709

// Connector colors
static QColor connectorSignal()       { return QColor(131, 247, 178); }  // #706 light green
static QColor connectorText()         { return QColor(201, 146, 254); }  // #705 purple
static QColor connectorHwUsed()       { return QColor(69, 225, 254);  }  // #702 cyan
static QColor connectorHwUnused()     { return QColor(92, 92, 93);    }  // #708 grey
static QColor connectorSecondary()    { return QColor(131, 247, 178, 140); }

// Frame for compound-input groups
static QColor compoundGroupBg()  { return QColor(255, 255, 255, 18); }
static QColor constantTextColor(){ return QColor(255, 253, 221);     }  // #701

// ─── Helpers ──────────────────────────────────────────────────────────────────

static QColor accentFor(GraphNodeKind k)
{
    switch (k) {
    case GraphNodeKind::Circuit:       return accentCircuit();
    case GraphNodeKind::HardwareSource:return accentHwSource();
    case GraphNodeKind::HardwareSink:  return accentHwSink();
    }
    return accentCircuit();
}

// ─── Layout computation ───────────────────────────────────────────────────────
//
// We lay out pins top-to-bottom.  Compound inputs (Primary + Scale + Offset
// rows sharing the same label) are gathered into groups and drawn together.
//
// The layout builds:
//   pinAnchors[pinId] = local connector-centre QPointF
//   cachedRect        = the full node bounding rect (origin at 0,0)

struct PinGroup {
    const GraphPin *primary = nullptr;
    const GraphPin *scale   = nullptr;
    const GraphPin *offset  = nullptr;
    bool isCompound() const { return scale || offset; }
    // Height in pixels consumed by this group
    qreal height() const {
        qreal h = NodeItem::ROW_HEIGHT;
        if (scale)  h += NodeItem::SECONDARY_HEIGHT;
        if (offset) h += NodeItem::SECONDARY_HEIGHT;
        return h;
    }
};

// Build a list of PinGroups from an ordered list of Input pins.
// Simple / Output pins are each their own single-entry group (primary only).
static QList<PinGroup> buildGroups(const QList<const GraphPin *> &pins)
{
    QList<PinGroup> groups;
    for (int i = 0; i < pins.size(); ) {
        const GraphPin *p = pins[i];
        if (p->direction == GraphPinDirection::In &&
            p->role == GraphPinRole::Primary)
        {
            PinGroup g;
            g.primary = p;
            ++i;
            while (i < pins.size()) {
                const GraphPin *q = pins[i];
                if (q->direction == GraphPinDirection::In &&
                    q->label == p->label)
                {
                    if (q->role == GraphPinRole::Scale)  g.scale  = q;
                    if (q->role == GraphPinRole::Offset) g.offset = q;
                    ++i;
                } else {
                    break;
                }
            }
            groups.append(g);
        } else {
            PinGroup g;
            g.primary = p;
            groups.append(g);
            ++i;
        }
    }
    return groups;
}

qreal NodeItem::heightFor(const GraphNode &n)
{
    QList<const GraphPin *> inPins, outPins;
    for (const auto &p : n.pins) {
        if (p.direction == GraphPinDirection::In)
            inPins.append(&p);
        else
            outPins.append(&p);
    }
    qreal inH = 0;
    for (const auto &g : buildGroups(inPins))  inH  += g.height();
    qreal outH = 0;
    for (const auto &g : buildGroups(outPins)) outH += g.height();
    return TITLE_HEIGHT + qMax(inH, outH) + 6.0; // 6px bottom padding
}

void NodeItem::doLayout() const
{
    pinAnchors.clear();

    // Separate pins by direction
    QList<const GraphPin *> inPins, outPins;
    for (const auto &p : node.pins) {
        if (p.direction == GraphPinDirection::In)
            inPins.append(&p);
        else
            outPins.append(&p);
    }

    QList<PinGroup> inGroups  = buildGroups(inPins);
    QList<PinGroup> outGroups = buildGroups(outPins);

    cachedRect = QRectF(0, 0, NODE_WIDTH, heightFor(node));

    // Left side (inputs)
    qreal y = TITLE_HEIGHT;
    for (const auto &g : inGroups) {
        qreal midY = y + ROW_HEIGHT * 0.5;
        pinAnchors[g.primary->id] = QPointF(0.0, midY);
        if (g.scale) {
            qreal sy = y + ROW_HEIGHT + SECONDARY_HEIGHT * 0.5;
            pinAnchors[g.scale->id] = QPointF(0.0, sy);
        }
        if (g.offset) {
            qreal base = y + ROW_HEIGHT + (g.scale ? SECONDARY_HEIGHT : 0.0);
            qreal oy = base + SECONDARY_HEIGHT * 0.5;
            pinAnchors[g.offset->id] = QPointF(0.0, oy);
        }
        y += g.height();
    }

    // Right side (outputs)
    y = TITLE_HEIGHT;
    for (const auto &g : outGroups) {
        qreal midY = y + ROW_HEIGHT * 0.5;
        pinAnchors[g.primary->id] = QPointF(NODE_WIDTH, midY);
        y += g.height();
    }

    layoutDirty = false;
}

// ─── NodeItem implementation ──────────────────────────────────────────────────

NodeItem::NodeItem(const GraphNode &n) : node(n) {}

QPointF NodeItem::pinAnchorLocal(const QString &pinId) const
{
    if (layoutDirty) doLayout();
    return pinAnchors.value(pinId, QPointF());
}

QRectF NodeItem::boundingRect() const
{
    if (layoutDirty) doLayout();
    // Connectors are painted centred ON the left/right edges, extending outside
    // cachedRect.  The largest horizontal overhang is CONNECTOR_RADIUS (circle)
    // which equals 5 px; add 1 px for the 1-px outline pen on all sides.
    constexpr qreal CONNECTOR_EXTENT = CONNECTOR_RADIUS; // 5.0 px
    return cachedRect.adjusted(-(CONNECTOR_EXTENT + 1), -1, (CONNECTOR_EXTENT + 1), 1);
}

void NodeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if (layoutDirty) doLayout();

    const QRectF r = cachedRect;
    const QRectF titleRect(r.left(), r.top(), r.width(), TITLE_HEIGHT);
    const QColor accent = accentFor(node.kind);

    // ── Node body ─────────────────────────────────────────────────────────────
    QPainterPath bodyPath;
    bodyPath.addRoundedRect(r, 6, 6);

    painter->setPen(QPen(nodeOutline(), 1.0));
    painter->fillPath(bodyPath, nodeBg());
    painter->drawPath(bodyPath);

    // ── Title bar ─────────────────────────────────────────────────────────────
    QPainterPath titlePath;
    titlePath.addRoundedRect(titleRect, 6, 6);
    // Square off the bottom corners
    titlePath.addRect(QRectF(r.left(), r.top() + 6, r.width(), TITLE_HEIGHT - 6));
    painter->fillPath(titlePath, accent);

    painter->setPen(titleTextColor());
    QFont f = painter->font();
    f.setBold(true);
    f.setPointSizeF(9.0);
    painter->setFont(f);
    painter->drawText(QRectF(r.left() + H_PADDING, r.top(), r.width() - 2 * H_PADDING, TITLE_HEIGHT),
                      Qt::AlignVCenter | Qt::AlignLeft,
                      node.title);

    // ── Separate pins by direction and build groups (same as doLayout) ────────
    QList<const GraphPin *> inPins, outPins;
    for (const auto &p : node.pins) {
        if (p.direction == GraphPinDirection::In)
            inPins.append(&p);
        else
            outPins.append(&p);
    }
    QList<PinGroup> inGroups  = buildGroups(inPins);
    QList<PinGroup> outGroups = buildGroups(outPins);

    QFont labelFont = painter->font();
    labelFont.setBold(false);
    labelFont.setPointSizeF(8.0);

    QFont smallFont = labelFont;
    smallFont.setPointSizeF(7.0);

    // ── Helper lambda: draw a single pin connector + label ───────────────────
    // anchorX: 0 = left edge, NODE_WIDTH = right edge
    // cx, cy: connector centre in local coords (same as pinAnchors)
    // isSecondary: smaller connector for Scale/Offset
    // isRight: label drawn to the left of connector (for output side)

    auto drawConnector = [&](const GraphPin *pin, qreal cx, qreal cy,
                              bool isSecondary, bool isRight)
    {
        bool isHw = (node.kind == GraphNodeKind::HardwareSource ||
                     node.kind == GraphNodeKind::HardwareSink);

        QColor fillColor, penColor;
        if (isHw) {
            fillColor = pin->used ? connectorHwUsed()   : connectorHwUnused();
            penColor  = pin->used ? connectorHwUsed()   : connectorHwUnused();
        } else if (pin->portKind == GraphPortKind::Text) {
            fillColor = connectorText();
            penColor  = connectorText();
        } else if (isSecondary) {
            fillColor = Qt::transparent;
            penColor  = connectorSecondary();
        } else {
            fillColor = pin->connected ? connectorSignal() : Qt::transparent;
            penColor  = connectorSignal();
        }

        painter->setPen(QPen(penColor, 1.2));
        painter->setBrush(fillColor);

        if (pin->portKind == GraphPortKind::Text) {
            // Square connector
            qreal s = isSecondary ? (CONNECTOR_SQUARE * 0.75) : CONNECTOR_SQUARE;
            painter->drawRect(QRectF(cx - s * 0.5, cy - s * 0.5, s, s));
        } else {
            // Round connector
            qreal r = isSecondary ? (CONNECTOR_RADIUS * 0.7) : CONNECTOR_RADIUS;
            painter->drawEllipse(QPointF(cx, cy), r, r);
        }

        // Label text area
        constexpr qreal connGap = 4.0;
        QString labelText = pin->label;

        // For unconnected pins with a constant, show the constant inline
        if (!pin->connected && !pin->constantText.isEmpty()) {
            labelText = pin->label.isEmpty()
                        ? pin->constantText
                        : pin->label + " = " + pin->constantText;
        }

        if (labelText.isEmpty())
            return;

        painter->setFont(isSecondary ? smallFont : labelFont);
        painter->setPen(isHw && !pin->used ? dimTextColor() : bodyTextColor());

        qreal rowH = isSecondary ? SECONDARY_HEIGHT : ROW_HEIGHT;
        qreal textY = cy - rowH * 0.5;

        if (isRight) {
            // Output pin: label to the left of the connector
            QRectF tr(H_PADDING, textY, NODE_WIDTH - H_PADDING - CONNECTOR_RADIUS - connGap, rowH);
            painter->drawText(tr, Qt::AlignVCenter | Qt::AlignRight, labelText);
        } else {
            // Input pin: label to the right of the connector
            qreal lx = cx + CONNECTOR_RADIUS + connGap;
            qreal availW = NODE_WIDTH - lx - H_PADDING;
            if (!pin->connected && !pin->constantText.isEmpty()) {
                painter->setPen(constantTextColor());
            }
            painter->drawText(QRectF(lx, textY, availW, rowH),
                              Qt::AlignVCenter | Qt::AlignLeft, labelText);
        }
    };

    // ── Draw input pin groups ─────────────────────────────────────────────────
    qreal y = TITLE_HEIGHT;
    for (const auto &g : inGroups) {
        // Compound group background
        if (g.isCompound()) {
            QRectF groupRect(2, y, NODE_WIDTH * 0.5 - 2, g.height());
            painter->fillRect(groupRect, compoundGroupBg());
        }

        // Primary row
        qreal cy = y + ROW_HEIGHT * 0.5;
        drawConnector(g.primary, 0.0, cy, false, false);

        // Scale sub-row
        if (g.scale) {
            qreal sy = y + ROW_HEIGHT + SECONDARY_HEIGHT * 0.5;
            drawConnector(g.scale, 0.0, sy, true, false);
        }
        // Offset sub-row
        if (g.offset) {
            qreal base = y + ROW_HEIGHT + (g.scale ? SECONDARY_HEIGHT : 0.0);
            qreal oy = base + SECONDARY_HEIGHT * 0.5;
            drawConnector(g.offset, 0.0, oy, true, false);
        }

        y += g.height();
    }

    // ── Draw output pin groups ────────────────────────────────────────────────
    y = TITLE_HEIGHT;
    for (const auto &g : outGroups) {
        qreal cy = y + ROW_HEIGHT * 0.5;
        drawConnector(g.primary, NODE_WIDTH, cy, false, true);
        y += g.height();
    }

    // ── Divider below title ───────────────────────────────────────────────────
    painter->setPen(QPen(accent.darker(150), 0.5));
    painter->drawLine(QPointF(r.left(), TITLE_HEIGHT), QPointF(r.right(), TITLE_HEIGHT));
}
