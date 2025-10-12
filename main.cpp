#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QTimer>

#include "darknessdetector.h"
#include "SerialInterface.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow mainWindow;
    mainWindow.show();

    // =========================================== Serial Communication ===========================================

    SerialInterface serialInterface(30, 22);

    QObject::connect(&serialInterface, &SerialInterface::dataReceived, [&](const QByteArray &data){
        uint8_t val0 = static_cast<uint8_t>(data[0]);
        uint8_t val1 = static_cast<uint8_t>(data[1]);
        uint8_t val2 = static_cast<uint8_t>(data[2]);
        //qDebug() << "Received :" << val0 << " , " << val1  << " , " << val2;
    });

    QObject::connect(&serialInterface, &SerialInterface::errorOccurred, [](const QString &msg){
        qWarning() << msg;
    });

    if (serialInterface.open(serialInterface.port(), 115200))
    {
        mainWindow.setSerialInterface(&serialInterface);
    }
    else
    {
        qCritical() << "Failed to open port.";
    }

    mainWindow.setSerialInterface(&serialInterface);

    // =========================================== Darkness Detector ===========================================

    auto darknessDetector = new DarknessDetector(&mainWindow);

    darknessDetector->setMinAreaRatio(0.02f);
    darknessDetector->setBlackThreshold(40);
    darknessDetector->setWhiteMask(5, 3);

    darknessDetector->start();

    QObject::connect(darknessDetector, &DarknessDetector::detectionReady, &mainWindow,
                    [&](QVector<Detector::DetectedObject> results, QImage src, float sx, float sy)
                    {
                       if (results.isEmpty()) return;
                       mainWindow.DrawDetectedBox(results);
                    });

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&](){
        darknessDetector->stop();
    });

    // =========================================== Update ===========================================

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
