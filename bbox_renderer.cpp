#include "bbox_renderer.h"
#include <algorithm>

BBoxRenderer::BBoxRenderer(QGraphicsView* canvas,
                           QCheckBox* isDisplayingCheckBox,
                           QObject* parent)
    : QObject(parent),
    canvas_(canvas),
    isDisplayingCheckBox_(isDisplayingCheckBox)
{
    // 背景透過（ビューをカメラViewに重ねる運用でも、同一scene運用でもOK）
    canvas_->setStyleSheet("background: transparent;");
    canvas_->setAttribute(Qt::WA_TranslucentBackground, true);
    canvas_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    canvas_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // 既存の Scene を使う（CameraDisplayer が setScene 済みを想定）
    scene_ = canvas_->scene();
    if (!scene_) {
        // 万一なければ自前で作る
        scene_ = new QGraphicsScene(canvas_);
        canvas_->setScene(scene_);
    }

    // 透明のオーバレイ用ピクスマップ
    overlayItem_ = new QGraphicsPixmapItem();
    overlayItem_->setZValue(1.0); // 映像より前面
    scene_->addItem(overlayItem_);

    ensureOverlayAligned_();
}

void BBoxRenderer::ensureOverlayAligned_()
{
    // CameraDisplayer 側が中央原点(-W/2,-H/2)運用なので合わせる
    const int W = canvas_->viewport()->width();
    const int H = canvas_->viewport()->height();
    overlayItem_->setOffset(-W/2.0, -H/2.0);
    overlayItem_->setPos(0, 0);
}

void BBoxRenderer::UpdateBoundingBoxes(const QVector<Detector::DetectedObject>& detectedObjects,
                                       const QSize& cameraResolution)
{
    if (!overlayItem_) return;

    if (isDisplayingCheckBox_ && !isDisplayingCheckBox_->isChecked()) {
        DeleteAllBoxes();
        return;
    }

    // 透明キャンバス作成（ビューの実サイズ）
    const int W = canvas_->viewport()->width();
    const int H = canvas_->viewport()->height();
    QImage bboxImage(W, H, QImage::Format_ARGB32_Premultiplied);
    bboxImage.fill(Qt::transparent);

    if (cameraResolution.isValid() && cameraResolution.width() > 0 && cameraResolution.height() > 0)
    {
        // カメラ画像は幅基準で縮小、上下レターボックス（あなたの計算と同じ）
        const float reductionRatio = float(W) / float(cameraResolution.width());
        const float heightOffset   = (H - cameraResolution.height() * reductionRatio) / 2.0f;

        QPainter painter(&bboxImage);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setFont(QFont("Arial Black", fontPoint_, QFont::Bold));

        for (const auto& object : detectedObjects)
        {
            // 座標変換（元画像 → 画面）
            const float x1 = object.x1 * reductionRatio;
            const float y1 = object.y1 * reductionRatio + heightOffset;
            const float x2 = object.x2 * reductionRatio;
            const float y2 = object.y2 * reductionRatio + heightOffset;

            // 色・太さ
            QColor lineColor;
            if (object.classifySize == 1) {
                lineColor.setHsv(180, 250, 250); // 水色
                thicknessAdjustment_ = 3;
            } else if (object.classifySize == 2) {
                lineColor.setHsv(object.index == 0 ? 180 : 0, 250, 250); // 水/赤
                thicknessAdjustment_ = 3;
            } else {
                int hue = (object.classifySize > 0)
                ? (object.index * 360 / object.classifySize) : 180;
                lineColor.setHsv(hue, 250, 250);
                thicknessAdjustment_ = 1;
            }

            const int thickness = std::max(1, int((object.score + 0.1f) * baseThickness_ * thicknessAdjustment_));

            // 描画
            const QRect rect(int(x1), int(y1), int(x2 - x1), int(y2 - y1));
            const QRect textRect(int(x1), int(y1), 250, fontPoint_ + 10);

            painter.setPen(QPen(lineColor, thickness));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(rect);

            painter.setBrush(QBrush(lineColor));
            painter.setPen(Qt::NoPen);
            painter.drawRect(textRect);

            painter.setPen(QPen(Qt::white));
            const QString scoreStr = QString::number(object.score, 'f', 2);
            painter.drawText(textRect.left() + 4,
                             textRect.top() + fontPoint_ + 2,
                             object.name + " : " + scoreStr);
        }
        // painter はスコープ抜けで end
    }

    overlayItem_->setPixmap(QPixmap::fromImage(bboxImage));
    // サイズが変わった可能性に備えて中央合わせ
    overlayItem_->setOffset(-bboxImage.width()/2.0, -bboxImage.height()/2.0);
    overlayItem_->setPos(0, 0);
}

void BBoxRenderer::DeleteAllBoxes()
{
    if (!overlayItem_) return;
    const int W = canvas_->viewport()->width();
    const int H = canvas_->viewport()->height();
    QImage clearImg(W, H, QImage::Format_ARGB32_Premultiplied);
    clearImg.fill(Qt::transparent);
    overlayItem_->setPixmap(QPixmap::fromImage(clearImg));
    overlayItem_->setOffset(-W/2.0, -H/2.0);
    overlayItem_->setPos(0, 0);
}
