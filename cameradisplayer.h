#pragma once

#include <QCameraDevice>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QVector>

// Forward declarations
class QCamera;
class QCheckBox;
class QComboBox;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QGraphicsView;
class QLabel;
class QMediaCaptureSession;
class QMediaPlayer;
class QPushButton;
class QVideoFrame;
class QVideoSink;

class CameraDisplayer : public QObject
{
    Q_OBJECT
public:
    explicit CameraDisplayer(QGraphicsView* graphicsView,
                             QComboBox* deviceComboBox,
                             QVector<QLabel*> labels,
                             QPushButton* captureButton,
                             QCheckBox* flipCheckBox,
                             QObject* parent = nullptr);
    ~CameraDisplayer() override;

    // Show/attach selected camera by combo index (0 = "Select ...")
    void DisplayVideo(int cameraIndex);

    // Enumerate available cameras and populate combo
    void ListCameraDevices();

    QImage LatestImage(){return latestImage_;}

    QSize OriginalResolution(){return QSize(resolution_.front());}
    int CanvasSize() noexcept {return CANVAS_SIZE;}

signals:
    void frameReady(const QImage& img);

private slots:
    // Called by QVideoSink for each new frame
    void ProcessVideoFrame(const QVideoFrame& frame);
    void onVideoFrame(const QVideoFrame& frame);

    // Save the latest frame as jpg
    void SaveImage();

private:
    // Utility: rotate with white background (no transparency)
    QImage rotateImageWithWhiteBackground(const QImage& src, int angleDegrees);

    // Utility: simplified aspect ratio using gcd
    QVector<int> CalculateAspectRatioFromResolution(int w, int h);

private:
    // UI references (not owned by this class)
    QGraphicsView*       graphicsView_   = nullptr;
    QComboBox*           deviceComboBox_ = nullptr;
    QVector<QLabel*>     labels_;
    QPushButton*         captureButton_  = nullptr;
    QCheckBox*           flipCheckBox_   = nullptr;

    // Scene graph (owned by Qt via parents)
    QGraphicsScene*      scene_          = nullptr;
    QGraphicsPixmapItem* videoPixmapItem_= nullptr;

    // Media pipeline (owned by Qt via parents)
    QMediaCaptureSession* captureSession_ = nullptr;
    QVideoSink*           videoSink_      = nullptr;
    QMediaPlayer*         videoPlayer_    = nullptr;
    QCamera*              camera_         = nullptr;

    // State
    QVector<QCameraDevice> cameras_;
    QVector<QSize>         resolution_;
    QVector<int>           aspectRatio_{1,1};
    bool                   isReversing_{false};
    QImage                 latestImage_;
    float                  scaleX_        = 1.0f;
    float                  scaleY_        = 1.0f;

    // Constants
    static constexpr int CANVAS_SIZE = 600;               // square view size (px)
    static constexpr const char* PRIMARY_CAMERA_NAME1 = "USB 2.0 Camera";
    static constexpr const char* PRIMARY_CAMERA_NAME2 = "FicUsbCamera1";
};
