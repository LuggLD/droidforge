#ifndef SECTIONFRAMEITEM_H
#define SECTIONFRAMEITEM_H
#include "graphmodeltypes.h"
#include <QGraphicsItem>
class SectionFrameItem : public QGraphicsItem {
    GraphSectionFrame frame;
public:
    explicit SectionFrameItem(const GraphSectionFrame &f);
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
};
#endif
