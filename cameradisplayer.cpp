#include "CameraDisplayer.h"

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QVideoSink>
#include <QMediaPlayer>
#include <QCamera>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QVideoFrame>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QTransform>
#include <QDateTime>
#include <QDebug>
#include <QOpenGLWidget>

#include <numeric>
#include <algorithm>

CameraDisplayer::CameraDisplayer(QGraphicsView *graphicsView,
                                 QComboBox *deviceComboBox,
                                 QVector<QLabel *> labels,
                                 QPushButton *captureButton,
                                 QCheckBox *flipCheckBox,
                                 QObject* parent)
    : QObject(parent),
    graphicsView_(graphicsView),
    deviceComboBox_(deviceComboBox),
    labels_(std::move(labels)),
    captureButton_(captureButton),
    flipCheckBox_(flipCheckBox)
{
    // --- Media objects ---
    captureSession_ = new QMediaCaptureSession(this);
    videoSink_      = new QVideoSink(this);
    videoPlayer_    = new QMediaPlayer(this);
    videoPlayer_->setVideoSink(videoSink_);

    // --- Graphics view / scene（軽量化） ---
    graphicsView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    graphicsView_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    graphicsView_->setFixedSize(CANVAS_SIZE, CANVAS_SIZE);

    // ★ GPU 描画を有効化（これだけで体感が大きく上がる）
    graphicsView_->setViewport(new QOpenGLWidget());
    graphicsView_->setRenderHints({}); // 余計なヒントは外す（必要になったら個別にON）
    graphicsView_->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);

    scene_ = new QGraphicsScene(graphicsView_); // parent = view
    graphicsView_->setScene(scene_);
    // 原点を中央に（あなたの既存仕様を維持）
    scene_->setSceneRect(-CANVAS_SIZE/2, -CANVAS_SIZE/2, CANVAS_SIZE, CANVAS_SIZE);

    // 背景は白（レターボックスの白塗りを画像生成でやらず、背景描画に任せる）
    scene_->setBackgroundBrush(Qt::white);

    // 映像表示アイテム（GPU側でスケール）
    videoPixmapItem_ = new QGraphicsPixmapItem();
    videoPixmapItem_->setTransformationMode(Qt::FastTransformation); // ★高速補間
    videoPixmapItem_->setZValue(0);
    scene_->addItem(videoPixmapItem_);

    // --- Populate devices ---
    ListCameraDevices();

    // --- Connections ---
    connect(videoSink_, &QVideoSink::videoFrameChanged,
            this, &CameraDisplayer::ProcessVideoFrame);

    connect(deviceComboBox_, &QComboBox::currentIndexChanged,
            this, [this](int index){ DisplayVideo(index); });

    connect(captureButton_, &QPushButton::pressed,
            this, [this](){ SaveImage(); });

    connect(flipCheckBox_, &QCheckBox::clicked,
            this, [this](){ isReversing_ = flipCheckBox_->isChecked(); });

    // --- Initial selection: prefer PRIMARY, fallback to first real device ---
    int idx = 0;
    for (int i = 0; i < cameras_.size(); ++i) {
        if (cameras_[i].description() == PRIMARY_CAMERA_NAME) { idx = i + 1; break; }
    }
    if (idx == 0 && !cameras_.isEmpty()) idx = 1;
    deviceComboBox_->setCurrentIndex(idx);
}

CameraDisplayer::~CameraDisplayer()
{
    if (camera_) {
        camera_->stop();
        camera_->deleteLater();
        camera_ = nullptr;
    }
}

void CameraDisplayer::DisplayVideo(const int cameraIndex)
{
    if (camera_) {
        camera_->stop();
        camera_->deleteLater();
        camera_ = nullptr;
    }

    if (cameraIndex <= 0 || cameraIndex > cameras_.size())
        return;

    camera_ = new QCamera(cameras_[cameraIndex - 1], this);

    captureSession_->setCamera(camera_);
    captureSession_->setVideoSink(videoSink_);

    camera_->start();

    // Labels: resolution/aspect
    resolution_ = camera_->cameraDevice().photoResolutions();
    if (!resolution_.isEmpty()) {
        const QSize r0 = resolution_.front();
        if (!labels_.isEmpty() && labels_[0]) {
            labels_[0]->setText(QString("Resolution  %1 x %2").arg(r0.width()).arg(r0.height()));
        }
        aspectRatio_ = CalculateAspectRatioFromResolution(r0.width(), r0.height());
        if (labels_.size() > 1 && labels_[1]) {
            labels_[1]->setText(QString("Aspect Ratio  %1 : %2").arg(aspectRatio_[0]).arg(aspectRatio_[1]));
        }
    } else {
        if (!labels_.isEmpty() && labels_[0]) labels_[0]->setText("Resolution  -");
        if (labels_.size() > 1 && labels_[1]) labels_[1]->setText("Aspect Ratio  -");
    }
}

void CameraDisplayer::ListCameraDevices()
{
    cameras_ = QMediaDevices::videoInputs();

    deviceComboBox_->blockSignals(true);
    deviceComboBox_->clear();
    deviceComboBox_->addItem("Select Camera Device or Video");
    for (const QCameraDevice& cam : cameras_) {
        deviceComboBox_->addItem(cam.description());
    }
    deviceComboBox_->blockSignals(false);
}

void CameraDisplayer::ProcessVideoFrame(const QVideoFrame& frame)
{
    // ---- 軽量化の肝：CPU側の画像生成を最小限に ----
    QVideoFrame f(frame);
    if (!f.isValid()) return;

    // 可能なら map() + 参照 QImage でゼロコピーに近づける
    QImage img;
    if (f.map(QVideoFrame::ReadOnly)) {
        const QImage::Format fmt = QVideoFrameFormat::imageFormatFromPixelFormat(f.pixelFormat());
        if (fmt != QImage::Format_Invalid) {
            img = QImage(f.bits(0), f.width(), f.height(), f.bytesPerLine(0), fmt).copy(); // ★安全のため最小コピー
        }
        f.unmap();
    }
    // map できない/非対応フォーマット → 素直に toImage()（Qt 内部での最小コピー）
    if (img.isNull())
        img = frame.toImage();

    if (img.isNull()) return;

    if (isReversing_) img = img.mirrored(true, false);

    // 回転が 0 なら一切回さない（不要な全画素処理を回避）
    const int angleDegrees = 0;
    if (angleDegrees % 360 != 0) {
        img = rotateImageWithWhiteBackground(img, angleDegrees);
    }

    // ★ レターボックス用の 600x600 画像は作らない。GPUで拡縮表示する。
    QPixmap pix = QPixmap::fromImage(img);

    // 600x600 中央に KeepAspectRatio でフィット（GPUスケーリング）
    const qreal canvasW = CANVAS_SIZE, canvasH = CANVAS_SIZE;
    const qreal sx = canvasW / pix.width();
    const qreal sy = canvasH / pix.height();
    const qreal scale = std::min(sx, sy);

    videoPixmapItem_->setPixmap(pix);
    videoPixmapItem_->setScale(scale);
    videoPixmapItem_->setPos(0, 0);
    videoPixmapItem_->setOffset(-pix.width() / 2.0, -pix.height() / 2.0);

    // 検出/保存用の最新フレーム（加工前の生画像）
    latestImage_ = img;

    // BBox との整合用（必要に応じて利用）
    scaleX_ = float(scale);
    scaleY_ = float(scale);
}

void CameraDisplayer::SaveImage()
{
    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    const QString fileName = "../SavedImages/" + ts + ".jpg";
    if (latestImage_.save(fileName, "JPG")) {
        qDebug() << "[INFO] Saved Image :" << fileName;
    } else {
        qDebug() << "[ERROR] Failed to Save Image :" << fileName;
    }
}

// --- 以下は既存関数名を維持（中では可能な限り軽量化） ---

QImage CameraDisplayer::rotateImageWithWhiteBackground(const QImage& src, const int angleDegrees)
{
    if (src.isNull() || angleDegrees % 360 == 0)
        return src;

    QTransform transform;
    transform.rotate(angleDegrees);

    QRectF bounds = transform.mapRect(QRectF(src.rect()));
    const QSize newSize = bounds.size().toSize();

    QImage result(newSize, QImage::Format_RGB32);
    result.fill(Qt::white);

    QPainter p(&result);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.translate(newSize.width() / 2.0, newSize.height() / 2.0);
    p.rotate(angleDegrees);
    p.translate(-src.width() / 2.0, -src.height() / 2.0);
    p.drawImage(0, 0, src);
    // p.end(); // 自動
    return result;
}

QImage CameraDisplayer::letterboxToCanvas(const QImage& src, const QSize& canvasSize)
{
    // 互換性のため残すが、通常は呼ばない運用に切替。
    // どうしても必要な場合のみ最小コストで実施。

    if (src.isNull()) {
        QImage c(canvasSize, QImage::Format_RGB32);
        c.fill(Qt::white);
        return c;
    }

    // 既に同じサイズ＆余白不要ならそのまま返す（コピー回避）
    if (src.size() == canvasSize) {
        return src;
    }

    // 1回の描画で完了（白紙→スケーリング描画）
    QImage canvas(canvasSize, QImage::Format_RGB32);
    canvas.fill(Qt::white);

    const QSize target = src.size().scaled(canvasSize, Qt::KeepAspectRatio);
    const int x = (canvasSize.width()  - target.width())  / 2;
    const int y = (canvasSize.height() - target.height()) / 2;

    QPainter p(&canvas);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(QRect(x, y, target.width(), target.height()), src);
    return canvas;
}

QVector<int> CameraDisplayer::CalculateAspectRatioFromResolution(int w, int h)
{
    if (w <= 0 || h <= 0) return {0, 0};
    int g = std::gcd(w, h);
    return { w / g, h / g };
}
