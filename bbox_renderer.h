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

    // 検出結果を反映（cameraResolution = 元フレームの解像度）
    void UpdateBoundingBoxes(const QVector<Detector::DetectedObject>& detectedObjects,
                             const QSize& cameraResolution);

    // すべて消去（透明化）
    void DeleteAllBoxes();

    // 任意：枠の見た目調整
    void setThicknessBase(int v) { baseThickness_ = v; }
    void setFontPoint(int pt)    { fontPoint_ = pt;   }

private:
    void ensureOverlayAligned_();

private:
    QGraphicsView*        canvas_ = nullptr;
    QGraphicsScene*       scene_  = nullptr;     // 既存sceneを使う
    QGraphicsPixmapItem*  overlayItem_ = nullptr; // 透明の上物
    QCheckBox*            isDisplayingCheckBox_ = nullptr;

    int baseThickness_ = 5; // スコア係数のベース
    int thicknessAdjustment_ = 1;
    int fontPoint_ = 20;
};
