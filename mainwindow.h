#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

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
    void DrawDetectedBox(QVector<Detector::DetectedObject> obj);

signals:
    void channelChanged(int position, double value);

private:
    Ui::MainWindow *ui;
    SerialInterface* serialInterface{nullptr};
    CameraDisplayer* cameraDisplayer_{nullptr};
    BBoxRenderer* bboxRenderer_{nullptr};

    IntegratedValueController* outerTubeVController{nullptr};
    IntegratedValueController* outerTubeHController{nullptr};
};
#endif // MAINWINDOW_H
