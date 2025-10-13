#include "DarknessDetector.h"
#include <QMetaObject>
#include <QDebug>

// OpenCV
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>

// ======================== Helpers (private static) ========================

static cv::Mat toGray(const cv::Mat& bgrOrGray) {
    if (bgrOrGray.channels() == 1) return bgrOrGray.clone();
    cv::Mat gray;
    cv::cvtColor(bgrOrGray, gray, cv::COLOR_BGR2GRAY);
    return gray;
}

cv::Mat DarknessDetector::qimageToCvBgrOrGray(const QImage& image)
{
    switch (image.format()) {
    case QImage::Format_RGB888: {
        cv::Mat rgb(image.height(), image.width(), CV_8UC3,
                    const_cast<uchar*>(image.bits()),
                    image.bytesPerLine());
        cv::Mat bgr;
        cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
        return bgr;
    }
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
    case QImage::Format_RGBA8888:
    case QImage::Format_RGBA8888_Premultiplied: {
        cv::Mat rgba(image.height(), image.width(), CV_8UC4,
                     const_cast<uchar*>(image.bits()),
                     image.bytesPerLine());
        cv::Mat bgr;
        cv::cvtColor(rgba, bgr, cv::COLOR_RGBA2BGR);
        return bgr;
    }
    case QImage::Format_RGB32: {
        cv::Mat bgra(image.height(), image.width(), CV_8UC4,
                     const_cast<uchar*>(image.bits()),
                     image.bytesPerLine());
        cv::Mat bgr;
        cv::cvtColor(bgra, bgr, cv::COLOR_BGRA2BGR);
        return bgr;
    }
    case QImage::Format_Grayscale8: {
        cv::Mat gray(image.height(), image.width(), CV_8UC1,
                     const_cast<uchar*>(image.bits()),
                     image.bytesPerLine());
        return gray.clone();
    }
    default:
        qWarning() << "[DarknessDetector] Unsupported QImage::Format =" << image.format();
        return cv::Mat();
    }
}

void DarknessDetector::coverWithWhiteMask(cv::Mat& img, int topPct, int rightLeftPct)
{
    if (img.empty()) return;
    const int h = img.rows, w = img.cols;
    const int top    = std::max(0, (h * topPct) / 100);
    const int bottom = h - top;
    const int left   = std::max(0, (w * rightLeftPct) / 100);
    const int right  = w - left;

    const cv::Scalar white = (img.channels() == 1) ? cv::Scalar(255) : cv::Scalar(255,255,255);

    if (top > 0) {
        cv::rectangle(img, {0,0}, {w, top}, white, cv::FILLED);
        cv::rectangle(img, {0,bottom}, {w, h}, white, cv::FILLED);
    }
    if (left > 0) {
        cv::rectangle(img, {0,0}, {left, h}, white, cv::FILLED);
        cv::rectangle(img, {right,0}, {w, h}, white, cv::FILLED);
    }
}

// ======================== Public: ctor / dtor ========================

DarknessDetector::DarknessDetector(QObject* parent)
    : QObject(parent)
{
    // Move this QObject to the worker thread on start(); keep now on UI thread.
    // We run detection methods on the worker by using invokeMethod to slots.
    connect(&worker_, &QThread::finished, &worker_, &QObject::deleteLater);
    // The object itself will be moved with moveToThread in startImpl()
}

DarknessDetector::~DarknessDetector()
{
    stop();
    worker_.quit();
    worker_.wait();
}

// ======================== Synchronous API ========================

QVector<Detector::DetectedObject> DarknessDetector::detect(const QImage& image,
                                                 float minAreaRatio,
                                                 int blackThreshold,
                                                 int whiteMaskTopPct,
                                                 int whiteMaskRightLeftPct) const
{

    // Demonstration bypass. This is used when detecting areas that are not black.
    const bool BYPASS = true;
    if (BYPASS)
    {
        auto out = detectBypass(image,minAreaRatio,blackThreshold,whiteMaskTopPct,whiteMaskRightLeftPct);
        return out;
    }

    QVector<Detector::DetectedObject> out;
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        qWarning() << "[DarknessDetector] Invalid image.";
        return out;
    }

    cv::Mat src = qimageToCvBgrOrGray(image);
    if (src.empty()) {
        qWarning() << "[DarknessDetector] Unsupported format.";
        return out;
    }

    if (whiteMaskTopPct > 0 || whiteMaskRightLeftPct > 0) {
        coverWithWhiteMask(src, whiteMaskTopPct, whiteMaskRightLeftPct);
    }

    cv::Mat gray = toGray(src);
    cv::Mat mask;
    cv::threshold(gray, mask, blackThreshold, 255, cv::THRESH_BINARY_INV);

    cv::Mat labels, stats, centroids;
    int num = cv::connectedComponentsWithStats(mask, labels, stats, centroids);
    if (num <= 1) return out; // background only

    int maxLabel = -1, maxArea = 0;
    for (int i = 1; i < num; ++i) {
        int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area > maxArea) { maxArea = area; maxLabel = i; }
    }
    if (maxLabel < 0) return out;

    const double imgArea = double(image.width()) * image.height();
    const float sizeRatio = float(maxArea / imgArea);
    if (sizeRatio < minAreaRatio) return out;

    DetectedObject obj;
    const int left   = stats.at<int>(maxLabel, cv::CC_STAT_LEFT);
    const int top    = stats.at<int>(maxLabel, cv::CC_STAT_TOP);
    const int width  = stats.at<int>(maxLabel, cv::CC_STAT_WIDTH);
    const int height = stats.at<int>(maxLabel, cv::CC_STAT_HEIGHT);
    obj.x1 = left; obj.y1 = top; obj.x2 = left + width; obj.y2 = top + height;
    obj.index = maxLabel; obj.classifySize = 1; obj.name = "Path"; obj.score = sizeRatio;

    out.push_back(obj);
    return out;
}

// ======================== Asynchronous API (public) ========================

void DarknessDetector::start()
{
    QMetaObject::invokeMethod(this, "startImpl", Qt::QueuedConnection);
}

void DarknessDetector::stop()
{
    QMetaObject::invokeMethod(this, "stopImpl", Qt::BlockingQueuedConnection);
}

void DarknessDetector::submitFrame(const QImage& image, float scaleX, float scaleY)
{
    QMetaObject::invokeMethod(this, "submitFrameImpl", Qt::QueuedConnection,
                              Q_ARG(QImage, image), Q_ARG(float, scaleX), Q_ARG(float, scaleY));
}

void DarknessDetector::setMinAreaRatio(float r)
{
    QMetaObject::invokeMethod(this, "setMinAreaRatioImpl", Qt::QueuedConnection, Q_ARG(float, r));
}

void DarknessDetector::setBlackThreshold(int t)
{
    QMetaObject::invokeMethod(this, "setBlackThresholdImpl", Qt::QueuedConnection, Q_ARG(int, t));
}

void DarknessDetector::setWhiteMask(int topPct, int rightLeftPct)
{
    QMetaObject::invokeMethod(this, "setWhiteMaskImpl", Qt::QueuedConnection,
                              Q_ARG(int, topPct), Q_ARG(int, rightLeftPct));
}

// ======================== Worker-thread impl ========================

void DarknessDetector::startImpl()
{
    if (running_) return;

    if (!worker_.isRunning()) {
        worker_.start();
    }
    // move this object to worker thread for its internal slots
    if (thread() != &worker_) {
        moveToThread(&worker_);
    }
    running_ = true;
    busy_    = false;
    pending_ = false;
}

void DarknessDetector::stopImpl()
{
    running_ = false;
    busy_    = false;
    pending_ = false;
    latest_  = QImage();
}

void DarknessDetector::setMinAreaRatioImpl(float r)
{
    if (r < 0.f) r = 0.f;
    if (r > 1.f) r = 1.f;
    minAreaRatio_ = r;
}

void DarknessDetector::setBlackThresholdImpl(int t)
{
    if (t < 0) t = 0;
    if (t > 255) t = 255;
    blackThreshold_ = t;
}

void DarknessDetector::setWhiteMaskImpl(int topPct, int rlPct)
{
    whiteTopPct_ = topPct;
    whiteRlPct_  = rlPct;
}

void DarknessDetector::submitFrameImpl(const QImage& img, float sx, float sy)
{
    latest_ = img;
    scaleX_ = sx;
    scaleY_ = sy;
    pending_ = true;
    tryProcess_();
}

void DarknessDetector::tryProcess_()
{
    if (!running_ || busy_ || !pending_ || latest_.isNull()) return;

    busy_ = true;
    pending_ = false;

    // Run sync detection on the worker thread
    QVector<Detector::DetectedObject> res = detect(latest_, minAreaRatio_, blackThreshold_, whiteTopPct_, whiteRlPct_);

    // Emit to whoever connected (likely UI thread via queued connection)
    emit detectionReady(res, latest_, scaleX_, scaleY_);

    busy_ = false;

    // If a newer frame arrived while we were busy, process immediately again
    if (pending_) {
        QMetaObject::invokeMethod(this, "tryProcess_", Qt::QueuedConnection);
    }
}

// Bypass

QVector<Detector::DetectedObject> DarknessDetector::detectBypass(const QImage& image,
                                                           float minAreaRatio,
                                                           int /*blackThreshold*/,
                                                           int whiteMaskTopPct,
                                                           int whiteMaskRightLeftPct) const
{
    QVector<Detector::DetectedObject> out;
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        qWarning() << "[DarknessDetector] Invalid image.";
        return out;
    }

    cv::Mat bgr = qimageToCvBgrOrGray(image);
    if (bgr.empty()) {
        qWarning() << "[DarknessDetector] Unsupported format.";
        return out;
    }

    if (whiteMaskTopPct > 0 || whiteMaskRightLeftPct > 0) {
        coverWithWhiteMask(bgr, whiteMaskTopPct, whiteMaskRightLeftPct);
    }

    // ============================================================
    const int  kRMin        = 120; // Rの絶対値（0..255）
    const int  kRDom        = 40;  // R が G/B よりどれだけ優勢か (R - max(G,B))
    const int  kMorphKernel = 3;   // ノイズ除去用モルフォロジ
    // ============================================================

    // BGR -> R,G,B チャンネル分離
    std::array<cv::Mat, 3> ch;
    cv::split(bgr, ch.data());         // ch[0]=B, ch[1]=G, ch[2]=R
    cv::Mat r = ch[2], g = ch[1], b = ch[0];

    // max(G,B)
    cv::Mat gbMax;
    cv::max(g, b, gbMax);

    // 条件：R >= kRMin かつ (R - max(G,B)) >= kRDom
    cv::Mat cond1, cond2, redMask;
    cv::compare(r, kRMin, cond1, cv::CMP_GE);
    cv::Mat rd;
    cv::subtract(r, gbMax, rd, cv::noArray(), CV_16S);        // 16bitで安全に差分
    cv::compare(rd, kRDom, cond2, cv::CMP_GE);
    cv::bitwise_and(cond1, cond2, redMask);
    redMask.convertTo(redMask, CV_8U);                        // 0/255

    // ノイズ削減（任意）
    if (kMorphKernel > 0) {
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(kMorphKernel, kMorphKernel));
        cv::morphologyEx(redMask, redMask, cv::MORPH_OPEN, kernel);
        cv::morphologyEx(redMask, redMask, cv::MORPH_CLOSE, kernel);
    }

    // 連結成分で最大赤領域を取得
    cv::Mat labels, stats, centroids;
    int num = cv::connectedComponentsWithStats(redMask, labels, stats, centroids, 8, CV_32S);
    if (num <= 1) return out; // 背景のみ

    int maxLabel = -1, maxArea = 0;
    for (int i = 1; i < num; ++i) {
        int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area > maxArea) { maxArea = area; maxLabel = i; }
    }
    if (maxLabel < 0) return out;

    const double imgArea = double(image.width()) * image.height();
    const float  areaRatio = float(maxArea / imgArea);
    if (areaRatio < minAreaRatio) return out;

    DetectedObject obj;
    const int left   = stats.at<int>(maxLabel, cv::CC_STAT_LEFT);
    const int top    = stats.at<int>(maxLabel, cv::CC_STAT_TOP);
    const int width  = stats.at<int>(maxLabel, cv::CC_STAT_WIDTH);
    const int height = stats.at<int>(maxLabel, cv::CC_STAT_HEIGHT);
    obj.x1 = left; obj.y1 = top; obj.x2 = left + width; obj.y2 = top + height;
    obj.index = maxLabel;
    obj.classifySize = 1;
    obj.name = "red_dominant";
    obj.score = areaRatio; // 「R成分がどれだけあるか」= 赤マスク面積比

    out.push_back(obj);
    return out;
}
