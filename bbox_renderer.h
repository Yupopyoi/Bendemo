#pragma once
#include <QObject>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QCheckBox>
#include <QVector>
#include <QImage>
#include <QPixmap>
#include <QPainter>

#include "darknessdetector.h" // DetectedObject

class BBoxRenderer : public QObject
{
    Q_OBJECT
public:
    explicit BBoxRenderer(QGraphicsView* canvas,
                          QCheckBox* isDisplayingCheckBox,
                          QObject* parent = nullptr);

    void UpdateBoundingBoxes(const QVector<Detector::DetectedObject>& detectedObjects,
                             const QSize& cameraResolution);

    void DeleteAllBoxes();

    void setThicknessBase(int v) { baseThickness_ = v; }
    void setFontPoint(int pt)    { fontPoint_ = pt;   }

private:
    void ensureOverlayAligned_();

private:
    QGraphicsView*        canvas_ = nullptr;
    QGraphicsScene*       scene_  = nullptr;
    QGraphicsPixmapItem*  overlayItem_ = nullptr;
    QCheckBox*            isDisplayingCheckBox_ = nullptr;

    int baseThickness_ = 5;
    int thicknessAdjustment_ = 1;
    int fontPoint_ = 20;
};
