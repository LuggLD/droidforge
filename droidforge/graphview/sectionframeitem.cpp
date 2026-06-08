#include "sectionframeitem.h"
#include <QPainter>
SectionFrameItem::SectionFrameItem(const GraphSectionFrame &f) : frame(f) {
    setZValue(-1);
}
QRectF SectionFrameItem::boundingRect() const { return frame.rect; }
void SectionFrameItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    painter->drawRoundedRect(frame.rect, 8, 8); // placeholder; real styling in Task 7
}
