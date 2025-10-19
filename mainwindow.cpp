#include "mainwindow.h"
#include "./ui_mainwindow.h"

#ifdef Q_OS_WIN
#include <Windows.h>

static void ensureNumLockOn()
{
    if ((GetKeyState(VK_NUMLOCK) & 0x1) == 0) {
        INPUT inputs[2] = {};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_NUMLOCK;

        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = VK_NUMLOCK;
        inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

        SendInput(2, inputs, sizeof(INPUT));
    }
}
#endif

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    #ifdef Q_OS_WIN
        ensureNumLockOn();
    #endif

    ui->setupUi(this);
    QByteArray latestSentSerialData = ReadLatestSentSerialData();

    // Outer Tube Vertical Movement Integrated Controller
    outerTubeVController = new IntegratedValueController(this, ui->verticalSliderOuter, ui->doubleSpinBoxVO, ui->resetButtonOV, 0.5);

    outerTubeVController->setRange(110, 160);
    outerTubeVController->setDecimals(1);
    double latestValueV = doubleFromBytes(latestSentSerialData, 0);
    if (latestValueV == std::numeric_limits<double>::quiet_NaN()) latestValueV = 135.0;
    else if (latestValueV == 0) latestValueV = 135.0;
    outerTubeVController->setValue(latestValueV);

    // Outer Tube Horizontal Movement Integrated Controller
    outerTubeHController = new IntegratedValueController(this, ui->horizontalSliderOuter, ui->doubleSpinBoxHO, ui->resetButtonOH, 0.5);

    outerTubeHController->setRange(110, 160);
    outerTubeHController->setDecimals(1);
    double latestValueH = doubleFromBytes(latestSentSerialData, 2);
    if (latestValueH == std::numeric_limits<double>::quiet_NaN()) latestValueH = 135.0;
    else if (latestValueH == 0) latestValueH = 135.0;
    outerTubeHController->setValue(latestValueH);

    connect(outerTubeVController, &IntegratedValueController::valueChanged, this, [&](double v)
    {
        // [HACK] If the connection to Arduino is not established, some parts of the SerialInterface may not function correctly.
        // Here, I am using the log text to verify that it is not connected.
        if(ui->arduinoLogLabel->text().contains(QStringLiteral("COM")) == false) return;
        serialInterface->SetMessage(0, outerTubeVController->valueAsBytes());
        serialInterface->Send();
    });

    connect(outerTubeHController, &IntegratedValueController::valueChanged, this, [&](double v)
    {
        if(ui->arduinoLogLabel->text().contains(QStringLiteral("COM")) == false) return;
        serialInterface->SetMessage(2, outerTubeHController->valueAsBytes());
        serialInterface->Send();
    });

    // Camera
    QVector<QLabel*> labels;
    labels << ui->labelResolution << ui->labelAspect;

    cameraDisplayer_ = new CameraDisplayer(
        /*graphicsView*/   ui->graphicsView,
        /*deviceComboBox*/ ui->cameraComboBox,
        /*labels*/         labels,
        /*captureButton*/  ui->cuptureButton,
        /*flipCheckBox*/   ui->flipCheckBox,
        /*parent*/         this
    );
    QTimer::singleShot(0, this, [this]{
        emit cameraReady(cameraDisplayer_);
    });
    qDebug() << "New CameraDisplayer";

    // BBox Renderer
    bboxRenderer_ = new BBoxRenderer(ui->graphicsView, ui->dbboxDispCheckBox, this);

    // Key Input
    auto* shot_numpad8 = new QShortcut(QKeySequence(QKeyCombination(Qt::KeypadModifier, Qt::Key_8)), this);
    connect(shot_numpad8, &QShortcut::activated, this, [&](){
        outerTubeVController->updateValue(true);
    });

    auto* shot_numpad2 = new QShortcut(QKeySequence(QKeyCombination(Qt::KeypadModifier, Qt::Key_2)), this);
    connect(shot_numpad2, &QShortcut::activated, this, [&](){
        outerTubeVController->updateValue(false);
    });

    auto* shot_numpad6 = new QShortcut(QKeySequence(QKeyCombination(Qt::KeypadModifier, Qt::Key_6)), this);
    connect(shot_numpad6, &QShortcut::activated, this, [&](){
        outerTubeHController->updateValue(true);
    });

    auto* shot_numpad4 = new QShortcut(QKeySequence(QKeyCombination(Qt::KeypadModifier, Qt::Key_4)), this);
    connect(shot_numpad4, &QShortcut::activated, this, [&](){
        outerTubeHController->updateValue(false);
    });

    // =========================================== Initialization ===========================================

    QTimer::singleShot(0, this, [this](){
        if (!serialInterface) return;
        serialInterface->SetMessage(0, outerTubeVController->valueAsBytes());
        serialInterface->SetMessage(2, outerTubeHController->valueAsBytes());

        // Send the message twice since it's easy to fail the first time.
        serialInterface->Send();
        serialInterface->Send();
    });

    // =========================================== Connections ===========================================

    connect(ui->recordButton, &QPushButton::clicked, this, [&](){
        serialInterface->changeRecordState();

        QString currentText = ui->recordButton->text();
        if (currentText == "Record")
        {
            ui->recordButton->setText("Stop");
        }
        else /* currentText == "Stop" */
        {
            ui->recordButton->setText("Record");
        }
    });

    connect(ui->applyButton, &QPushButton::clicked, this, [&](){

        QString currentText = ui->applyButton->text();
        if (currentText == "Start Applying")
        {
            ui->applyButton->setText("Stop Applying");
            canApply_ = true;
        }
        else /* currentText == "Stop Applying" */
        {
            ui->applyButton->setText("Start Applying");
            canApply_ = false;
        }
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::DrawDetectedBox(QVector<Detector::DetectedObject> objects)
{
    const QSize camRes = cameraDisplayer_->OriginalResolution();
    bboxRenderer_->UpdateBoundingBoxes(objects, camRes);
}

void MainWindow::setArduinoLogLabel(QByteArray log, QString portName, int baudrate)
{
    QString logText = "";
    if(log.size() == 0)
    {
        logText = "No Byte Data Received!";
    }
    else
    {
        int counter = 0;
        foreach (byte l, log)
        {
            logText += QString::number(l);
            if (++counter >= 13)
            {
                break;
            }
            logText += " , ";
        }
    }

    QString text = "Port : " + portName + ", BaudRate : " + QString::number(baudrate) + "\n" + logText;
    ui->arduinoLogLabel->setText(text);
}

void MainWindow::setDifferenceLabel(double xDiff, double yDiff)
{
    if(std::isnan(xDiff) || std::isnan(yDiff))
    {
        ui->labelDiff->setText("Difference from the center : ---.- , ---.-");
        return;
    }
    QString text = "Difference from the center x : " + QString::number(xDiff, 'f', 1) + " , y :  " + QString::number(yDiff, 'f', 1);
    ui->labelDiff->setText(text);
}

void MainWindow::setControllLabel(double x, double y)
{
    if(std::isnan(x) || std::isnan(y))
    {
        ui->labelControll->setText("Controll : ---.- , ---.-");
        return;
    }
    QString text = "Controll : " + QString::number(x, 'f', 1) + " , " + QString::number(y, 'f', 1);
    ui->labelControll->setText(text);
}

void MainWindow::setDetectorComboBox(QString yoloModelName, int defaultIndex)
{
    ui->detectorComboBox->blockSignals(true);
    ui->detectorComboBox->clear();
    ui->detectorComboBox->addItem("OpenCV");
    ui->detectorComboBox->addItem(yoloModelName);
    ui->detectorComboBox->blockSignals(false);

    ui->detectorComboBox->setCurrentIndex(defaultIndex);
}

void MainWindow::addMotorValue(int motorIndex, double value)
{
    switch(motorIndex)
    {
    case 0: /* Outer Tube (Vertical) */
        outerTubeVController->addValue(value);
        break;
    case 1: /* Outer Tube (Horizontal) */
        outerTubeHController->addValue(value);
        break;
    }
}

QString MainWindow::DetectorName()
{
    return ui->detectorComboBox->currentText();
}

// ===================================== Private Methods =====================================

QByteArray MainWindow::ReadLatestSentSerialData()
{
    auto latestCsvPath = []() -> QString {
        QDir base(QCoreApplication::applicationDirPath());
        const QString dn = base.dirName().toLower();
        if (dn == "release" || dn == "debug") base.cdUp();
        base.mkpath("SerialLogs");
        return base.filePath("SerialLogs/LatestSentSerial.csv");
    };

    auto loadLatestBytes = [](const QString& path) -> QByteArray {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
        QTextStream s(&f);
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
        s.setEncoding(QStringConverter::Utf8);
#endif
        if (s.atEnd()) return {};
        s.readLine();
        if (s.atEnd()) return {};
        const QString line = s.readLine().trimmed();
        if (line.isEmpty()) return {};
        const QStringList cols = line.split(',', Qt::KeepEmptyParts);
        if (cols.size() < 2) return {};

        QByteArray bytes; bytes.reserve(cols.size()-1);
        for (int i = 1; i < cols.size(); ++i) {
            bool ok=false; int v = cols[i].toInt(&ok, 10);
            if (!ok || v < 0 || v > 255) return {};
            bytes.append(static_cast<char>(static_cast<unsigned char>(v)));
        }
        return bytes;
    };

    const QString path = latestCsvPath();

    return loadLatestBytes(path);
}
