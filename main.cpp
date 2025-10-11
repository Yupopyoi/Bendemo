#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QTimer>

#include "SerialInterface.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    // 送受信データ長は1バイト固定
    SerialInterface serialInterface(30, 22);

    QObject::connect(&serialInterface, &SerialInterface::dataReceived, [&](const QByteArray &data){
        uint8_t val0 = static_cast<uint8_t>(data[0]);
        uint8_t val1 = static_cast<uint8_t>(data[1]);
        uint8_t val2 = static_cast<uint8_t>(data[2]);
        qDebug() << "Received :" << val0 << " , " << val1  << " , " << val2;
    });

    QObject::connect(&serialInterface, &SerialInterface::errorOccurred, [](const QString &msg){
        qWarning() << msg;
    });

    if (serialInterface.open(serialInterface.port(), 115200))
    {
        w.setSerialInterface(&serialInterface);
    }
    else
    {
        qCritical() << "Failed to open port.";
    }

    w.setSerialInterface(&serialInterface);

    return a.exec();
}
