#include "sectionframeitem.h"
#include <QPainter>
#include <QPainterPath>

SectionFrameItem::SectionFrameItem(const GraphSectionFrame &f) : frame(f)
{
    setZValue(-1);
}

QRectF SectionFrameItem::boundingRect() const
{
    // Add a small margin so the outline pen is fully inside the bounding rect.
    return frame.rect.adjusted(-1, -1, 1, 1);
}

void SectionFrameItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    const QRectF r = frame.rect;

    // Translucent fill
    painter->setBrush(QColor(255, 255, 255, 14));   // very subtle white tint
    painter->setPen(QPen(QColor(130, 130, 130, 80), 1.0, Qt::SolidLine));
    painter->drawRoundedRect(r, 10, 10);

    // Title label near top-left
    if (!frame.title.isEmpty()) {
        QFont f = painter->font();
        f.setBold(true);
        f.setPointSizeF(8.5);
        painter->setFont(f);
        painter->setPen(QColor(180, 180, 180, 160));
        painter->drawText(QRectF(r.left() + 10, r.top() + 4, r.width() - 20, 20),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          frame.title);
    }
}
