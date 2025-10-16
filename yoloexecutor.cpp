#include "yoloexecutor.h"

#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

YoloExecutor::YoloExecutor(QObject* parent){}

// ---------------------- internal: files/labels ------------------

QString YoloExecutor::findModelsBaseDir_()
{
    // %APPDATA%/Bendemo/models
    {
        const QString p = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/models/";
        if (QDir(p).exists()) return p;
    }

    // .../bin/models
    {
        const QString p = QCoreApplication::applicationDirPath() + "/models/";
        if (QDir(p).exists()) return p;
    }

    // Directly under the repository /models
    {
        QDir dir(QCoreApplication::applicationDirPath());
        for (int i = 0; i < 8; ++i) {
            if (dir.exists("CMakeLists.txt") || dir.exists(".git")) {
                const QString p = dir.absoluteFilePath("models/");
                if (QDir(p).exists()) return p;
            }
            if (!dir.cdUp()) break;
        }
    }

    return {};
}

bool YoloExecutor::checkFilesAndLabel_(QString* shownName)
{
    const std::string base = findModelsBaseDir_().toStdString();
    const std::string modelPath = base + MODEL_NAME;
    const std::string yamlPath  = base + CLASSIFY_YAML_PATH;

    const bool hasModel = fs::exists(fs::path(modelPath));
    const bool hasYaml  = fs::exists(fs::path(yamlPath));

    if (!hasModel || !hasYaml)
    {
        qDebug() << "[YoloExecutor] files missing. model=" << hasModel
                 << " yaml=" << hasYaml
                 << " base=" << QString::fromStdString(base);
        return false;
    }
    if (shownName) *shownName = QString::fromStdString(MODEL_NAME);
    return true;
}

// ----------------------------- Loader -----------------------------

bool YoloExecutor::Load(bool useCUDA)
{
    canUseCUDA_ = useCUDA;

    const auto modelPath = findModelsBaseDir_().toStdString() + MODEL_NAME;

    qDebug() << "[YoloExecutor] Model Path : " << modelPath << " Exist : " << fs::exists(fs::path(modelPath));

    const torch::Device device = useCUDA ? torch::kCUDA : torch::kCPU;

    qDebug() << "[YoloExecutor] Device : " << device.str();

    try
    {
        if(useCUDA)
        {
            model_ = std::make_unique<torch::jit::Module>(torch::jit::load(findModelsBaseDir_().toStdString() + MODEL_NAME, torch::kCUDA));
        }
        else
        {
            model_ = std::make_unique<torch::jit::Module>(torch::jit::load(findModelsBaseDir_().toStdString() + MODEL_NAME, torch::kCPU));
        }

        model_->eval();

        // Load Classify Names from the yaml file
        classifyNames_.clear();
        YAML::Node labels = YAML::LoadFile(findModelsBaseDir_().toStdString() + CLASSIFY_YAML_PATH);
        for (auto it = labels["names"].begin(); it != labels["names"].end(); ++it)
        {
            classifyNames_.append(it->second.as<std::string>());
        }

        qDebug() << "[YoloExecutor] Loading finished successfully! [MLExecutor]";

        return true;
    }
    catch (const c10::Error& e)
    {
        qDebug() << "[YoloExecutor][ERROR]" << e.msg();
        return false;
    }
}

// --------------------------- Detect (sync) ----------------------

QVector<Detector::DetectedObject> YoloExecutor::Detect(const std::shared_ptr<QImage> image)
{
    detectedObjects_.clear();

    if (!isDetectionPermitted_)
    {
        return detectedObjects_;
    }

    if (!image || image->isNull())
    {
        qDebug() << "[YoloExecutor][ERROR] Invalid image (The image is null)";
        return detectedObjects_;
    }

    if (!model_)
    {
        qDebug() << "[YoloExecutor][ERROR] The model is null";
        return detectedObjects_;
    }

    torch::Tensor imgTensor = QImageToTensor(image);

    // -------------------------------------------------------------------------------
    // Resizing images
    // Resize without changing the aspect ratio to avoid affecting detection.
    // -------------------------------------------------------------------------------

    int targetWidth = INPUT_EDGE_SIZE;
    int targetHeight = static_cast<int>(image->size().height() * INPUT_EDGE_SIZE / image->size().width());

    paddingSize_.setWidth(0);
    paddingSize_.setHeight((targetWidth - targetHeight) / 2);

    reductionRatio_.setX((float)targetWidth / image->size().width());
    reductionRatio_.setY((float)targetHeight / image->size().height());

    imgTensor = ResizeImage(imgTensor, targetHeight, targetWidth);

    imgTensor = PadImage(imgTensor);

    // -------------------------------------------------------------------------------
    // Detection
    // -------------------------------------------------------------------------------

    c10::IValue output = model_->forward({imgTensor});

    if(canUseCUDA_)
    {
        detections_ = output.toTensor().cuda();
    }
    else
    {
        detections_ = output.toTensor().cpu();
    }

    StoreDetectedObjects();
    return detectedObjects_;

}

// ------------------------ Pre/Post process helpers ----------------------

torch::Tensor YoloExecutor::QImageToTensor(const std::shared_ptr<QImage> image)
{
    QImage img = image->convertToFormat(QImage::Format_RGB888);
    int width = img.width();
    int height = img.height();

    std::vector<uint8_t> img_data(height * width * 3);

    if(canUseCUDA_)
    {
        // ToDo : Use cudaMemcpyAsync
        memcpy(img_data.data(), img.bits(), img_data.size());
    }
    else
    {
        memcpy(img_data.data(), img.bits(), img_data.size());
    }

    auto img_tensor = torch::from_blob(img_data.data(), {height, width, 3}, torch::kUInt8);
    img_tensor = img_tensor.permute({2, 0, 1});  // HWC to CHW
    img_tensor = img_tensor.toType(torch::kFloat).div(255.0);
    img_tensor = img_tensor.unsqueeze(0);  // Add batch dimension

    if(canUseCUDA_)
    {
        return img_tensor.to(torch::kCUDA);
    }
    else
    {
        return img_tensor.to(torch::kCPU);
    }
}

torch::Tensor YoloExecutor::ResizeImage(const torch::Tensor& image, const int targetHeight, const int targetWidth)
{
    auto resized_image = torch::nn::functional::interpolate
    (
        image,
        torch::nn::functional::InterpolateFuncOptions()
            .size(std::vector<int64_t>({targetHeight, targetWidth}))
            .mode(torch::kBilinear)
            .align_corners(false)
    );

    if(canUseCUDA_)
    {
        return resized_image.to(torch::kCUDA);
    }
    else
    {
        return resized_image.to(torch::kCPU);
    }
}

torch::Tensor YoloExecutor::PadImage(const torch::Tensor& image)
{
    int pad_x = (INPUT_EDGE_SIZE - image.size(3)) / 2;
    int pad_y = (INPUT_EDGE_SIZE - image.size(2)) / 2;
    int pad_left = pad_x;
    int pad_right = INPUT_EDGE_SIZE - image.size(3) - pad_left;
    int pad_top = pad_y;
    int pad_bottom = INPUT_EDGE_SIZE - image.size(2) - pad_top;

    return torch::nn::functional::pad(
               image.unsqueeze(0), // Add batch dimension
               torch::nn::functional::PadFuncOptions({pad_left, pad_right, pad_top, pad_bottom}).mode(torch::kConstant).value(0)).squeeze(0); // Remove batch dimension
}

void YoloExecutor::StoreDetectedObjects()
{
    detectedObjects_.clear();

    for (int i = 0; i < detections_.size(1); i++)
    {
        auto score = detections_[0][i][4].item<float>();

        if (score > SCORE_THRESHOLD)
        {
            DetectedObject detectedObject;

            auto x1 = detections_[0][i][0].item<float>();
            auto y1 = detections_[0][i][1].item<float>();
            auto x2 = detections_[0][i][2].item<float>();
            auto y2 = detections_[0][i][3].item<float>();
            auto index = detections_[0][i][5].item<int>();

            detectedObject.x1 = (x1 - paddingSize_.width())  / reductionRatio_.x();
            detectedObject.y1 = (y1 - paddingSize_.height()) / reductionRatio_.y();
            detectedObject.x2 = (x2 - paddingSize_.width())  / reductionRatio_.x();
            detectedObject.y2 = (y2 - paddingSize_.height()) / reductionRatio_.y();
            detectedObject.score = score;
            detectedObject.index = index;
            detectedObject.classifySize = classifyNames_.size();
            detectedObject.name = classifyNames_[detections_[0][i][5].item<int>()];

            detectedObjects_.append(detectedObject);
        }
    }

    std::sort(detectedObjects_.begin(), detectedObjects_.end(),
              [](const Detector::DetectedObject& a, const Detector::DetectedObject& b) {
                  return a.score > b.score;
              });
}
