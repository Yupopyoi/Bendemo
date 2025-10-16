#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QShortcut>
#include <QTimer>

#include "bbox_renderer.h"
#include "CameraDisplayer.h"
#include "DarknessDetector.h"
#include "IntegratedValueController.h"
#include "SerialInterface.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void setSerialInterface(SerialInterface* ptr){serialInterface = ptr;}

    QImage LatestCameraImage(){return cameraDisplayer_->LatestImage();}
    int CanvasSize(){return cameraDisplayer_->CanvasSize();}
    void DrawDetectedBox(QVector<Detector::DetectedObject> obj);

    // Set Label and ComboBox
    void setArduinoLogLabel(QByteArray log, QString portName, int baudrate = 115200);
    void setDifferenceLabel(double xDiff, double yDiff);
    void setControllLabel(double xDiff, double yDiff);
    void setDetectorComboBox(QString yoloModelName, int defaultIndex = 0);

    // Controll Equipment
    bool canApply() noexcept {return canApply_;}
    void addMotorValue(int motorIndex, double value);

    QString DetectorName();

signals:
    void channelChanged(int position, double value);
    void cameraReady(CameraDisplayer* cam);

private:
    Ui::MainWindow *ui;
    SerialInterface* serialInterface{nullptr};
    CameraDisplayer* cameraDisplayer_{nullptr};
    BBoxRenderer* bboxRenderer_{nullptr};

    IntegratedValueController* outerTubeVController{nullptr};
    IntegratedValueController* outerTubeHController{nullptr};

    bool canApply_{false};

    QByteArray ReadLatestSentSerialData();
    inline double doubleFromBytes(const QByteArray& bytes, int idx)
    {
        if (idx < 0 || idx + 1 >= bytes.size()) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        const int hi = static_cast<unsigned char>(bytes[idx]);
        const int lo = static_cast<unsigned char>(bytes[idx + 1]);
        return (hi * 256 + lo) / 10.0;
    }
};
#endif // MAINWINDOW_H
