#include "CameraDisplayer.h"

#include <QCamera>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDebug>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QImage>
#include <QLabel>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QTransform>
#include <QVideoFrame>
#include <QVideoSink>

#include <algorithm>
#include <numeric>

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

    // --- Graphics view / scene ---
    graphicsView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    graphicsView_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    graphicsView_->setFixedSize(CANVAS_SIZE, CANVAS_SIZE);

    graphicsView_->setViewport(new QOpenGLWidget());
    graphicsView_->setRenderHints({});
    graphicsView_->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);

    scene_ = new QGraphicsScene(graphicsView_); // parent = view
    graphicsView_->setScene(scene_);

    scene_->setSceneRect(-CANVAS_SIZE/2, -CANVAS_SIZE/2, CANVAS_SIZE, CANVAS_SIZE);
    scene_->setBackgroundBrush(Qt::white);

    videoPixmapItem_ = new QGraphicsPixmapItem();
    videoPixmapItem_->setTransformationMode(Qt::FastTransformation);
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
        if (cameras_[i].description() == PRIMARY_CAMERA_NAME1) { idx = i + 1; break; }
        if (cameras_[i].description() == PRIMARY_CAMERA_NAME2) { idx = i + 1; break; }
    }
    if (idx == 0 && !cameras_.isEmpty()) idx = 0;

    deviceComboBox_->setCurrentIndex(idx);

    if(flipCheckBox_->isChecked())
    {
        isReversing_ = flipCheckBox_->isChecked();
    }
}

CameraDisplayer::~CameraDisplayer()
{
    if (camera_) {
        camera_->stop();
        camera_->deleteLater();
        camera_ = nullptr;
    }
}

void CameraDisplayer::onVideoFrame(const QVideoFrame& frame)
{
    if (!frame.isValid()) return;
    QImage img = frame.toImage();
    if (img.isNull()) return;

    if (isReversing_) img = img.mirrored(true, false);

    emit frameReady(img);
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
    QVideoFrame f(frame);

    emit onVideoFrame(f);

    if (!f.isValid()) return;

    QImage img;
    if (f.map(QVideoFrame::ReadOnly)) {
        const QImage::Format fmt = QVideoFrameFormat::imageFormatFromPixelFormat(f.pixelFormat());
        if (fmt != QImage::Format_Invalid) {
            img = QImage(f.bits(0), f.width(), f.height(), f.bytesPerLine(0), fmt).copy(); // ★安全のため最小コピー
        }
        f.unmap();
    }

    if (img.isNull())
        img = frame.toImage();

    if (img.isNull()) return;

    if (isReversing_) img = img.mirrored(true, false);

    const int angleDegrees = 0;
    if (angleDegrees % 360 != 0) {
        img = rotateImageWithWhiteBackground(img, angleDegrees);
    }

    QPixmap pix = QPixmap::fromImage(img);

    const qreal canvasW = CANVAS_SIZE, canvasH = CANVAS_SIZE;
    const qreal sx = canvasW / pix.width();
    const qreal sy = canvasH / pix.height();
    const qreal scale = std::min(sx, sy);

    videoPixmapItem_->setPixmap(pix);
    videoPixmapItem_->setScale(scale);
    videoPixmapItem_->setPos(0, 0);
    videoPixmapItem_->setOffset(-pix.width() / 2.0, -pix.height() / 2.0);

    latestImage_ = img;

    scaleX_ = float(scale);
    scaleY_ = float(scale);
}

void CameraDisplayer::SaveImage()
{
    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    const QString fileName = "./SavedImages/" + ts + ".jpg";

    QFileInfo fi(fileName);
    if (!QDir().mkpath(fi.absolutePath())) {
        qWarning() << "[ERROR] Failed to create directory:" << fi.absolutePath();
    }

    if (latestImage_.save(fileName, "JPG")) {
        qDebug() << "[INFO] Saved Image :" << fileName;
    } else {
        qDebug() << "[ERROR] Failed to Save Image :" << fileName;
    }
}

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

    return result;
}

QVector<int> CameraDisplayer::CalculateAspectRatioFromResolution(int w, int h)
{
    if (w <= 0 || h <= 0) return {0, 0};
    int g = std::gcd(w, h);
    return { w / g, h / g };
}
