#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QTimer>

#include "autobending.h"
#include "darknessdetector.h"
#include "SerialInterface.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow mainWindow;
    mainWindow.show();

    // =========================================== Serial Communication ===========================================

    SerialInterface serialInterface(30, 22);

    QObject::connect(&serialInterface, &SerialInterface::errorOccurred, [](const QString &msg){
        qWarning() << msg;
    });

    const int Baudrate = 115200;
    const QString PortName = serialInterface.port();
    if (serialInterface.open(PortName, Baudrate))
    {
        mainWindow.setSerialInterface(&serialInterface);
    }
    else
    {
        qCritical() << "Failed to open port.";
    }

    mainWindow.setArduinoLogLabel(QByteArray(), PortName, Baudrate);

    QObject::connect(&serialInterface, &SerialInterface::dataReceived, [&](const QByteArray &data)
                     {
                        mainWindow.setArduinoLogLabel(data, PortName, Baudrate);
                     });

    // =========================================== Darkness Detector & Auto Bender ===========================================

    auto darknessDetector = new DarknessDetector(&mainWindow);

    darknessDetector->setMinAreaRatio(0.02f);
    darknessDetector->setBlackThreshold(40);
    darknessDetector->setWhiteMask(5, 3);

    darknessDetector->start();

    AutoBending autoBend;
    autoBend.setGains(1.0, 0.00, 0.01);
    autoBend.setDeadband(2.0);
    autoBend.setOutputSaturation(2.0);
    autoBend.setDerivativeCutoffHz(5.0);
    autoBend.setGeometry(25.0, 25.0);

    double addX_ = 0.0, addY_ = 0.0;
    QObject::connect(darknessDetector, &DarknessDetector::detectionReady, &mainWindow,
                    [&](QVector<Detector::DetectedObject> results, QImage src, float sx, float sy)
                    {
                        mainWindow.DrawDetectedBox(results);

                        if (results.isEmpty())
                        {
                            mainWindow.setDifferenceLabel(std::nan(""), std::nan(""));
                            mainWindow.setControllLabel(std::nan(""), std::nan(""));
                            return;
                        }

                        // Difference from the center
                        double reduceRatioX = (double)mainWindow.CanvasSize() / (src.size().width());
                        double reduceRatioY = (double)mainWindow.CanvasSize() / (src.size().height());
                        double centerPositionX = (results[0].x1 + results[0].x2) * 0.5 * reduceRatioX;
                        double centerPositionY = (results[0].y1 + results[0].y2) * 0.5 * reduceRatioY;

                        double differenceX = centerPositionX - (double)mainWindow.CanvasSize() * 0.5;
                        double differenceY = (double)mainWindow.CanvasSize() * 0.5 - centerPositionY;

                        mainWindow.setDifferenceLabel(differenceX, differenceY);

                        double dX = 0.0, dY = 0.0;
                        if (autoBend.step(differenceX, differenceY, dX, dY))
                        {
                            mainWindow.setControllLabel(dX, dY);
                            addX_ = dX;
                            addY_ = dY;
                        }
                    });

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&](){
        darknessDetector->stop();
    });

    QTimer motorUpdateTimer;
    QObject::connect(&motorUpdateTimer, &QTimer::timeout,
                     [&mainWindow, &addX_, &addY_]()
                     {
                        if (!mainWindow.canApply()) return;

                        if (std::abs(addX_) > 0.0) mainWindow.addMotorValue(0, addX_);
                        if (std::abs(addY_) > 0.0) mainWindow.addMotorValue(1, addY_);
                     });
    motorUpdateTimer.start(1000);

    QTimer updateTimer;
    QObject::connect(&updateTimer, &QTimer::timeout,
                     [&mainWindow, &darknessDetector]()
                     {
                         QImage latestImage = mainWindow.LatestCameraImage();

                        if (!latestImage.isNull()) {
                            const float scaleX = 1.0f;
                            const float scaleY = 1.0f;
                            darknessDetector->submitFrame(latestImage, scaleX, scaleY);
                        }
                     });
    updateTimer.start(50);

    return app.exec();
}
