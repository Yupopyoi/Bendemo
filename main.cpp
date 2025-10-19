#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QTimer>

#include <c10/macros/Macros.h>

#include "autobending.h"
#include "darknessdetector.h"
#include "SerialInterface.h"
#include "yoloexecutor.h"

int main(int argc, char *argv[])
{
    qputenv("CUDA_LAUNCH_BLOCKING", "1");
    qputenv("TORCH_SHOW_CPP_STACKTRACES", "1");

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
        qCritical() << "[Main] Failed to open port.";
    }

    mainWindow.setArduinoLogLabel(QByteArray(), PortName, Baudrate);

    QObject::connect(&serialInterface, &SerialInterface::dataReceived, [&](const QByteArray &data)
                     {
                        mainWindow.setArduinoLogLabel(data, PortName, Baudrate);
                     });

    // Send continuously at regular intervals.
    // This allows the Arduino to confirm that communication with the Qt application has been established.
    // If communication cannot be confirmed, the Arduino will physically disconnect the power circuit connected to the motor.
    QTimer continuousSendTimer;
    QObject::connect(&continuousSendTimer, &QTimer::timeout,
                     [&serialInterface]()
                     {
                        serialInterface.Send();
                     });
    continuousSendTimer.start(500);

    // ===========================================    Auto Bender    ===========================================

    AutoBending autoBend;
    autoBend.setGains(1.0, 0.00, 0.01);
    autoBend.setDeadband(50.0);
    autoBend.setOutputSaturation(2.0);
    autoBend.setDerivativeCutoffHz(5.0);
    autoBend.setGeometry(25.0, 25.0);

    double addX_ = 0.0, addY_ = 0.0;
    QTimer motorUpdateTimer;
    QObject::connect(&motorUpdateTimer, &QTimer::timeout,
                     [&mainWindow, &addX_, &addY_]()
                     {
                         if (!mainWindow.canApply()) return;

                         if (std::abs(addX_) > 0.0) mainWindow.addMotorValue(1 /* Horizontal */, addX_);
                         if (std::abs(addY_) > 0.0) mainWindow.addMotorValue(0 /*  Vertical  */, addY_);
                     });
    motorUpdateTimer.start(100);


    // =========================================== Center Difference Calculator ===========================================

    struct CenterDifferenceCalculator
    {
        double operator()(const QVector<Detector::DetectedObject>& results,
                          const QImage& src,
                          int detectedIndex,
                          int canvasSize,
                          double& differenceX,
                          double& differenceY) const
        {
            if (results.size() == 0) return 0.0;

            const double reduceRatioX = static_cast<double>(canvasSize) / src.width();
            const double reduceRatioY = static_cast<double>(canvasSize) / src.height();

            const double centerPositionX = (results[detectedIndex].x1 + results[detectedIndex].x2) * 0.5 * reduceRatioX;
            const double centerPositionY = (results[detectedIndex].y1 + results[detectedIndex].y2) * 0.5 * reduceRatioY;

            differenceX = centerPositionX - static_cast<double>(canvasSize) * 0.5;
            differenceY = static_cast<double>(canvasSize) * 0.5 - centerPositionY;

            return std::hypot(differenceX, differenceY);
        }
    };

    CenterDifferenceCalculator calculator;

    // =========================================== Darkness Detector ===========================================

    auto darknessDetector = new DarknessDetector(nullptr);

    darknessDetector->setMinAreaRatio(0.02f);
    darknessDetector->setBlackThreshold(40);
    darknessDetector->setWhiteMask(5, 3);

    darknessDetector->start();

    // Image Acquisition & Detection
    QTimer ddUpdateTimer;
    QObject::connect(&ddUpdateTimer, &QTimer::timeout,
                    [&mainWindow, darknessDetector]()
                    {
                        if(mainWindow.DetectorName().contains("OpenCV") == false) return;

                        const QImage latest = mainWindow.LatestCameraImage();
                        if (latest.isNull()) return;

                        // submitFrame() contains the detect function
                        darknessDetector->submitFrame(latest);
                    });
    ddUpdateTimer.start(50);

    QObject::connect(darknessDetector, &DarknessDetector::detectionReady, &mainWindow,
                    [&](QVector<Detector::DetectedObject> results, QImage src, float sx, float sy)
                    {
                        // Output of bounding boxes
                         mainWindow.DrawDetectedBox(results);

                        if (results.isEmpty())
                        {
                            mainWindow.setDifferenceLabel(std::nan(""), std::nan(""));
                            mainWindow.setControllLabel(std::nan(""), std::nan(""));
                            return;
                        }

                        // Calculate the difference in image center coordinates
                        double differenceX, differenceY;
                        calculator(results, src, 0, mainWindow.CanvasSize(), differenceX, differenceY);

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


    // =========================================== YOLO Detector ===========================================
    const bool useCUDA = true;
    auto yolo = std::make_unique<YoloExecutor>();

    if (!yolo->Load(useCUDA)) {
        qCritical() << "[Main] YOLO Load failed";
    }

    mainWindow.setDetectorComboBox(yolo->ModelName(), 1);

    static bool busy = false;
    static QElapsedTimer tick; tick.start();

    QObject::connect(&mainWindow, &MainWindow::cameraReady,
                    &mainWindow, [&](CameraDisplayer* cam){
                        QTimer updateTimer;

                        static bool busy = false;
                        static QElapsedTimer tick; tick.start();

                        QObject::connect(cam, &CameraDisplayer::frameReady,
                                        &mainWindow,
                                        [&](const QImage& img)
                                        {
                                            if(mainWindow.DetectorName().contains("yolo") == false) return;

                                            if (busy) return;
                                            if (tick.isValid() && tick.elapsed() < 100)
                                                return;
                                            tick.restart();

                                            busy = true;

                                            auto qimgPtr = std::make_shared<QImage>(img);
                                            auto results = yolo->Detect(qimgPtr);

                                            mainWindow.DrawDetectedBox(results);
                                            busy = false;

                                            // Calculate the difference in image center coordinates
                                            double differenceX, differenceY;
                                            calculator(results, img, 0, mainWindow.CanvasSize(), differenceX, differenceY);

                                            mainWindow.setDifferenceLabel(differenceX, differenceY);

                                            double dX = 0.0, dY = 0.0;
                                            if (autoBend.step(differenceX, differenceY, dX, dY))
                                            {
                                                mainWindow.setControllLabel(dX, dY);
                                                addX_ = dX;
                                                addY_ = dY;
                                            }
                                        },
                                        Qt::QueuedConnection);
                    });

    return app.exec();
}
