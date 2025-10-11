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

#include <numeric>  // std::gcd
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
    // --- Media objects (Qt will own/delete them via parent) ---
    captureSession_ = new QMediaCaptureSession(this);
    videoSink_      = new QVideoSink(this);
    videoPlayer_    = new QMediaPlayer(this);
    videoPlayer_->setVideoSink(videoSink_);

    // --- Graphics view / scene ---
    graphicsView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    graphicsView_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    graphicsView_->setFixedSize(CANVAS_SIZE, CANVAS_SIZE);

    scene_ = new QGraphicsScene(graphicsView_); // parent = view
    graphicsView_->setScene(scene_);
    scene_->setSceneRect(-CANVAS_SIZE/2, -CANVAS_SIZE/2, CANVAS_SIZE, CANVAS_SIZE);

    videoPixmapItem_ = new QGraphicsPixmapItem();
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
    int idx = 0; // 0 = "Select ..."
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
        camera_->deleteLater();   // safety if still owned by QObject tree
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

    // 0: "Select Camera Device or Video"
    if (cameraIndex <= 0 || cameraIndex > cameras_.size())
        return;

    camera_ = new QCamera(cameras_[cameraIndex - 1], this);

    captureSession_->setCamera(camera_);
    captureSession_->setVideoSink(videoSink_);

    camera_->start();

    // Labels: resolution/aspect (guard for empty list)
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

void CameraDisplayer::ProcessVideoFrame(const QVideoFrame &frame)
{
    QImage img = frame.toImage();
    if (img.isNull()) return;

    if (isReversing_)
        img = img.mirrored(/*h*/true, /*v*/false);

    const int angleDegrees = 0; // change if rotation is needed
    QImage rotated = rotateImageWithWhiteBackground(img, angleDegrees)
                         .convertToFormat(QImage::Format_RGB888);

    QImage canvas = letterboxToCanvas(rotated, QSize(CANVAS_SIZE, CANVAS_SIZE));

    QPixmap pixmap = QPixmap::fromImage(canvas);
    videoPixmapItem_->setPixmap(pixmap);
    videoPixmapItem_->setOffset(-pixmap.width() / 2.0, -pixmap.height() / 2.0);
    videoPixmapItem_->setPos(0, 0);

    latestImage_ = canvas;

    const QSize target = rotated.size().scaled(QSize(CANVAS_SIZE, CANVAS_SIZE), Qt::KeepAspectRatio);
    scaleX_ = static_cast<float>(target.width())  / std::max(1, rotated.width());
    scaleY_ = static_cast<float>(target.height()) / std::max(1, rotated.height());
}

void CameraDisplayer::SaveImage()
{
    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss"); // no ':' for Windows
    const QString fileName = "../SavedImages/" + ts + ".jpg";
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
    p.end();

    return result;
}

// Create a 600x600 white canvas and draw 'src' centered, scaled with aspect ratio preserved
QImage CameraDisplayer::letterboxToCanvas(const QImage& src, const QSize& canvasSize)
{
    QImage canvas(canvasSize, QImage::Format_RGB32);
    canvas.fill(Qt::white);

    if (src.isNull()) return canvas;

    const QSize target = src.size().scaled(canvasSize, Qt::KeepAspectRatio);
    const int x = (canvasSize.width()  - target.width())  / 2;
    const int y = (canvasSize.height() - target.height()) / 2;

    QPainter p(&canvas);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(QRect(x, y, target.width(), target.height()), src);
    p.end();

    return canvas;
}

QVector<int> CameraDisplayer::CalculateAspectRatioFromResolution(int w, int h)
{
    if (w <= 0 || h <= 0) return {0, 0};
    int g = std::gcd(w, h);
    return { w / g, h / g };
}
