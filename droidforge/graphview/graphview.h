#ifndef GRAPHVIEW_H
#define GRAPHVIEW_H
#include "patchview.h"
#include "graphmodeltypes.h"
#include "graphedits.h"
#include <QGraphicsView>
#include <QList>
#include <QPointF>
class QGraphicsScene;
class QGraphicsPathItem;
class GraphView : public QGraphicsView, public PatchView {
    Q_OBJECT
    QGraphicsScene *scene;
public:
    explicit GraphView(PatchEditEngine *patch, QWidget *parent=nullptr);
public slots:
    void rebuildGraphics();
protected:
    void wheelEvent(QWheelEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void contextMenuEvent(QContextMenuEvent *) override;
private:
    // Returns the pin id under a scene position, or empty. Scans NodeItems.
    QString pinAtScene(const QPointF &scenePos) const;
    QPointF pinScenePos(const QString &pinId) const; // scene pos of a pin's anchor, or null
    void endDrag();                                  // tear down any active drag (no commit)
    void commitEdit(bool ok, const QString &message);      // commit + rebuild if ok

    // Active drag state
    bool dragging = false;
    QString dragFromPin;
    // The fixed end(s) of the preview wire(s), in scene coords. The rubber-band
    // draws a line from each anchor to the cursor. For a plain connect this is
    // the dragged pin; for a sink pick-up (copy/move) it is the source feeding
    // the sink; for a source pick-up (re-home) it is every connected sink.
    QList<QPointF> dragAnchors;
    GraphEdits::DragMode dragMode = GraphEdits::DragMode::Connect;
    QGraphicsPathItem *rubber = nullptr;
};
#endif
