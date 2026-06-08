#ifndef GRAPHVIEW_H
#define GRAPHVIEW_H
#include "patchview.h"
#include "graphmodeltypes.h"
#include <QGraphicsView>
class QGraphicsScene;
class GraphView : public QGraphicsView, public PatchView {
    Q_OBJECT
    QGraphicsScene *scene;
public:
    explicit GraphView(PatchEditEngine *patch, QWidget *parent=nullptr);
public slots:
    void rebuildGraphics();
protected:
    void wheelEvent(QWheelEvent *) override;
};
#endif
