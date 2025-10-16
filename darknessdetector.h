#ifndef DARKNESSDETECTOR_H
#define DARKNESSDETECTOR_H

#pragma once
#include <QObject>
#include <QImage>
#include <QVector>
#include <QThread>
#include <QString>

// ---- OpenCV forward decl to keep the header light ----
namespace cv { class Mat; }

class Detector
{
    public :
    struct DetectedObject {
        int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        int index = 0;
        int classifySize = 0;
        std::string name = "Path";
        float score = 0.0f;
    };
};

/**
 * Darkness detector:
 * - Synchronous: detect(QImage) -> largest black region
 * - Asynchronous: start() / submitFrame() / detectionReady(...) on a private QThread
 *
 * Threading:
 *   - Asynchronous methods hop to the worker thread via invokeMethod.
 *   - Do not touch Qt Widgets from detection callbacks; handle results on UI thread.
 */
class DarknessDetector : public QObject, Detector
{
    Q_OBJECT
public:
    explicit DarknessDetector(QObject* parent = nullptr);
    ~DarknessDetector() override;

    // ---------- Synchronous API ----------
    QVector<DetectedObject> detect(const QImage& image,
                                   float minAreaRatio = 0.01f,
                                   int blackThreshold = 30,
                                   int whiteMaskTopPct = 0,
                                   int whiteMaskRightLeftPct = 0) const;

    // ---------- Asynchronous API ----------
    void start();  // start worker loop (idle until a frame is submitted)
    void stop();   // stop/pause worker loop
    void submitFrame(const QImage& image, float scaleX = 1.f, float scaleY = 1.f);

    // Tunables (effective for both sync/async; async updates are thread-safe via invoke)
    void setMinAreaRatio(float r);
    void setBlackThreshold(int t);
    void setWhiteMask(int topPct, int rightLeftPct);

signals:
    // Emitted on the UI thread side because we use QueuedConnection by default.
    // results[0] is the black area with the largest area.
    void detectionReady(QVector<DetectedObject> results, QImage source, float scaleX, float scaleY);

private:
    // ---- Internal helpers (implemented in .cpp) ----
    static cv::Mat qimageToCvBgrOrGray(const QImage& image);
    static void coverWithWhiteMask(cv::Mat& img, int topPct, int rightLeftPct);

private slots:
    // ---- Worker-thread slots ----
    void startImpl();
    void stopImpl();
    void setMinAreaRatioImpl(float r);
    void setBlackThresholdImpl(int t);
    void setWhiteMaskImpl(int topPct, int rlPct);
    void submitFrameImpl(const QImage& img, float sx, float sy);
    void tryProcess_();

private:
    // ---- Worker-thread state ----
    QThread worker_;
    bool running_ = false;
    bool busy_    = false;
    bool pending_ = false;

    QImage latest_;
    float  scaleX_ = 1.f;
    float  scaleY_ = 1.f;

    // Tunables
    float minAreaRatio_ = 0.01f;
    int   blackThreshold_ = 30;
    int   whiteTopPct_ = 0;
    int   whiteRlPct_  = 0;
};

#endif // DARKNESSDETECTOR_H
