#ifndef SERIALINTERFACE_H
#define SERIALINTERFACE_H

#pragma once

#include <QByteArray>
#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>
#include <QTimer>

/**
 * @brief Qt-based serial interface with COBS framing for fixed-length TX/RX (Transmitter/Receiver) payloads.
 *
 * Usage:
 *   SerialInterface serialInterface(tx_len, rx_len);
 *   serialInterface.open("COM3", 115200);
 *   serialInterface.SetMessage(0, QByteArray::fromHex("01020304"));
 *   serialInterface.Send();
 *   auto latest = serialInterface.readLatest(); // fixed length rx_len
 *
 * Signals:
 *   dataReceived(QByteArray) emitted when a full valid frame is decoded.
 *   errorOccurred(QString) on errors (range, framing, port errors).
 */

class SerialInterface : public QObject
{
    Q_OBJECT
public:
    explicit SerialInterface(int tx_payload_len,
                             int rx_payload_len,
                             QObject* parent = nullptr);
    ~SerialInterface();

    // This function returns the first port.
    QString port();

    bool open(const QString& port_name, int baud_rate);
    void close();
    bool isOpen() const;

    // Transmitter
    bool SetMessage(int position, const QByteArray& chunk);
    bool Send();

    // Receiver
    QByteArray read() const;

signals:
    void dataReceived(const QByteArray& payload); // size == rx_len_
    void errorOccurred(const QString& message);

public slots:
    void changeRecordState();

private slots:
    void onReadyRead();

private:
    // --- COBS helpers ---
    // Returns encoded frame WITHOUT trailing 0x00; caller appends 0x00.
    static QByteArray cobsEncode(const QByteArray& input);
    // Decodes one COBS-encoded frame (without trailing 0x00). Returns empty on error.
    static QByteArray cobsDecode(const QByteArray& encoded);

    static int minCobsEncodedLength(int raw_len);
    static int maxCobsEncodedLength(int raw_len);

    void configurePort(const QString& port_name, int baud_rate);

    // framing
    void processIncoming(); // parse rx_accumulator_ for 0x00-terminated frames

    void saveLatestTxCsv_();

private:
    QSerialPort serial_;
    const int tx_len_;
    const int rx_len_;

    QByteArray tx_message_;        // fixed length = tx_len_
    QByteArray latest_rx_payload_; // fixed length = rx_len_

    QByteArray rx_accumulator_;    // collects bytes until 0x00 (frame delimiter)

    // Logging
    bool        isRecording_{false};
    QFile       logFile_;
    QTextStream logStream_;
    QTimer      flushTimer_;
    QDate       currentDate_;
    QString     currentStamp_;
};

#endif // SERIALINTERFACE_H
