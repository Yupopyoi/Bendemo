#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Outer Tube Vertical Movement Integrated Controller
    outerTubeVController = new IntegratedValueController(this, ui->verticalSliderOuter, ui->doubleSpinBoxVO, 0.5);

    outerTubeVController->setRange(0, 270);
    outerTubeVController->setDecimals(1);
    outerTubeVController->setValue(135.0);

    // Outer Tube Horizontal Movement Integrated Controller
    outerTubeHController = new IntegratedValueController(this, ui->horizontalSliderOuter, ui->doubleSpinBoxHO, 0.5);

    outerTubeHController->setRange(0, 270);
    outerTubeHController->setDecimals(1);
    outerTubeHController->setValue(135.0);

    connect(outerTubeVController, &IntegratedValueController::valueChanged, this, [&](double v){
        serialInterface->SetMessage(0, outerTubeVController->valueAsBytes());
        serialInterface->Send();
    });

    connect(outerTubeHController, &IntegratedValueController::valueChanged, this, [&](double v){
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

    // BBox Renderer
    bboxRenderer_ = new BBoxRenderer(ui->graphicsView, ui->dbboxDispCheckBox, this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::DrawDetectedBox(QVector<Detector::DetectedObject> objects)
{
    // CameraDisplayer が持つ元フレーム解像度（例：first photoResolutions 等）を渡す
    const QSize camRes = /* 例: cameraDisplayer_->OriginalResolution() */ QSize(600,600);
    bboxRenderer_->UpdateBoundingBoxes(objects, camRes);
}
