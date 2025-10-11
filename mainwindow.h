#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "CameraDisplayer.h"
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

signals:
    void channelChanged(int position, double value);

private:
    Ui::MainWindow *ui;
    SerialInterface* serialInterface{nullptr};
    CameraDisplayer* cameraDisplayer_ = nullptr;

    IntegratedValueController* outerTubeVController{nullptr};
    IntegratedValueController* outerTubeHController{nullptr};
};
#endif // MAINWINDOW_H
