#pragma once
#ifndef YOLOEXECUTOR_H
#define YOLOEXECUTOR_H

#include <memory>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QImage>
#include <QLabel>
#include <QObject>
#include <QPainter>
#include <QPointF>
#include <QSize>
#include <QStandardPaths>
#include <QString>
#include <QVector>

#include <torch/script.h>
#include <torch/torch.h>
#include <yaml-cpp/yaml.h>

#include <ATen/cuda/CUDAContext.h>
#include <c10/cuda/CUDACachingAllocator.h>

#include "darknessdetector.h"

#ifndef MODEL_NAME
#define MODEL_NAME         std::string("yolov10b.torchscript")
#endif
#ifndef CLASSIFY_YAML_PATH
#define CLASSIFY_YAML_PATH std::string("yolov10.yaml")
#endif
#ifndef INPUT_EDGE_SIZE
#define INPUT_EDGE_SIZE    640
#endif
#ifndef SCORE_THRESHOLD
#define SCORE_THRESHOLD    0.10f
#endif

class YoloExecutor : public QObject, public Detector
{
    Q_OBJECT
public:
    explicit YoloExecutor(QObject* parent = nullptr);
    ~YoloExecutor() override = default;

    bool Load(bool useCUDA);

    QVector<DetectedObject> Detect(const std::shared_ptr<QImage> image);

    void PermitDetection(bool on) { isDetectionPermitted_ = on; }

    QString ModelName() {return QString::fromStdString(MODEL_NAME);}

signals:
    void errorOccurred(const QString& message);

private:
    // File Loaders
    QString findModelsBaseDir_();
    bool checkFilesAndLabel_(QString* shownName);

    // Preprocess
    torch::Tensor QImageToTensor(const std::shared_ptr<QImage> image);
    torch::Tensor ResizeImage(const torch::Tensor& image, int targetH, int targetW);
    torch::Tensor PadImage(const torch::Tensor& image);

    // Postprocess
    void StoreDetectedObjects();

private:
    // Torch
    std::unique_ptr<torch::jit::Module> model_{nullptr};
    int modelVersion_;

    bool canUseCUDA_{false};
    bool isDetectionPermitted_{true};

    bool onlyHorse_{true};

    QPointF reductionRatio_{1.f, 1.f};
    QSize   paddingSize_{0, 0};

    // Results (Buffer)
    torch::Tensor detections_; // [1,N,6]
    QVector<DetectedObject> detectedObjects_;

    QVector<std::string> classifyNames_;
};

#endif // YOLOEXECUTOR_H
