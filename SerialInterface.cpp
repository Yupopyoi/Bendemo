#include "SerialInterface.h"

#include <QtGlobal>
#include <QMetaObject>

namespace {
// guard to avoid huge buffers
constexpr int kMaxReasonableLen = 1'000'000;
}

SerialInterface::SerialInterface(int tx_payload_len,
                                 int rx_payload_len,
                                 QObject* parent)
    : QObject(parent),
    tx_len_(tx_payload_len),
    rx_len_(rx_payload_len),
    tx_message_(tx_payload_len, '\x00'),
    latest_rx_payload_(rx_payload_len, '\x00')
{
    Q_ASSERT(tx_len_ > 0 && rx_len_ > 0);
    connect(&serial_, &QSerialPort::readyRead, this, &SerialInterface::onReadyRead);
    connect(&serial_, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError e){
        if (e != QSerialPort::NoError) {
            emit errorOccurred(QString("[Serial] Error: %1").arg(serial_.errorString()));
        }
    });

    flushTimer_.setInterval(1000);
    connect(&flushTimer_, &QTimer::timeout, this, [this](){
        if (logStream_.device()) logStream_.flush();
    });
}

SerialInterface::~SerialInterface()
{
    if(isRecording_)
    {
        flushTimer_.stop();
        if (logStream_.device()) logStream_.flush();
        logFile_.close();
    }

    saveLatestTxCsv_();

    close();    
}

QString SerialInterface::port()
{
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();

    for (const QSerialPortInfo &info : std::as_const(ports))
    {
        QString desc = info.description().toLower();
        QString manu = info.manufacturer().toLower();

        if (desc.contains("arduino") ||
            manu.contains("arduino") ||
            desc.contains("ch340") ||
            desc.contains("usb serial") ||
            manu.contains("wch") ||
            manu.contains("silicon labs"))
        {
            return info.portName();
        }
    }

    if (!ports.isEmpty()) {
        return ports.first().portName();
    }

    return QString();
}


bool SerialInterface::open(const QString& port_name, int baud_rate)
{
    if (port_name == "")
    {
        qDebug() << "[Serial] Open failed: Invalid port name";
        isOpened_ = false;
        return false;
    }

    if (isOpen()) return true;
    configurePort(port_name, baud_rate);
    if (!serial_.open(QIODevice::ReadWrite)) {
        emit errorOccurred(QString("[Serial] Open failed: %1").arg(serial_.errorString()));
        isOpened_ = false;
        return false;
    }

    isOpened_ = true;
    return true;
}

void SerialInterface::close()
{
    if (serial_.isOpen()) {
        serial_.close();
    }
    rx_accumulator_.clear();
    isOpened_ = false;
}

bool SerialInterface::isOpen() const
{
    return isOpened_;
}

void SerialInterface::configurePort(const QString& port_name, int baud_rate)
{
    serial_.setPortName(port_name);
    serial_.setBaudRate(baud_rate);
    serial_.setDataBits(QSerialPort::Data8);
    serial_.setParity(QSerialPort::NoParity);
    serial_.setStopBits(QSerialPort::OneStop);
    serial_.setFlowControl(QSerialPort::NoFlowControl);
}

bool SerialInterface::SetMessage(int position, const QByteArray& chunk)
{
    if (position < 0 || chunk.size() < 0) {
        emit errorOccurred("[Serial] SetMessage: negative position/size.");
        return false;
    }

    if (position + chunk.size() > tx_len_) {
        emit errorOccurred(QString("[Serial] SetMessage: out of range (pos=%1, size=%2, tx_len=%3)")
                               .arg(position).arg(chunk.size()).arg(tx_len_));
        return false;
    }

    if (chunk.isEmpty()) return true; // nothing to do
    std::copy(chunk.begin(), chunk.end(), tx_message_.begin() + position);
    return true;
}

bool SerialInterface::Send()
{
    if (!isOpen()) {
        emit errorOccurred("[Serial] Send: port not open.");
        return false;
    }

    // COBS encode + trailing 0x00 delimiter
    const QByteArray encoded = cobsEncode(tx_message_);
    if (encoded.isEmpty()) {
        emit errorOccurred("[Serial] Send: COBS encoding failed.");
        return false;
    }

    QByteArray frame = encoded;
    frame.push_back('\x00'); // delimiter

    const qint64 written = serial_.write(frame);
    if (written != frame.size()) {
        emit errorOccurred(QString("[Serial] Send: write returned %1 / %2")
                               .arg(written).arg(frame.size()));
        return false;
    }
    return true;
}

QByteArray SerialInterface::read() const
{
    return latest_rx_payload_;
}

// ======================= Slots =======================

void SerialInterface::changeRecordState()
{
    isRecording_ = !isRecording_;

    if (isRecording_)
    {
        // 保存先 = 実行ファイルのあるディレクトリ（/release or /debug の時は1つ上に寄せる）
        QDir base(QCoreApplication::applicationDirPath());
        const QString dn = base.dirName().toLower();
        if (dn == "release" || dn == "debug") base.cdUp();   // ← そのまま直下に置きたいならこの行を消す

        base.mkpath("SerialLogs");
        const QString dir = base.filePath("SerialLogs");

        currentDate_  = QDate::currentDate();
        currentStamp_ = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
        const QString path = QDir(dir).filePath(currentStamp_ + ".csv");

        const bool existed = QFile::exists(path);
        logFile_.setFileName(path);
        if (!logFile_.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            emit errorOccurred(QString("[Serial] CSV open failed: %1").arg(path));
            isRecording_ = false;
            return;
        }

        logStream_.setDevice(&logFile_);
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
        logStream_.setEncoding(QStringConverter::Utf8);
#endif
        if (!existed) {
            logStream_ << "timestamp";
            for (int i = 0; i < rx_len_; ++i) logStream_ << ",b" << i;
            logStream_ << '\n';
        }
        flushTimer_.start();
        qDebug() << "[Serial] Recording ON ->" << path;
    } else {
        flushTimer_.stop();
        if (logStream_.device()) logStream_.flush();
        logFile_.close();
        qDebug() << "[Serial] Recording OFF";
    }
}

void SerialInterface::onReadyRead()
{
    const QByteArray chunk = serial_.readAll();
    if (chunk.isEmpty()) return;

    // guard against pathological growth
    if (rx_accumulator_.size() + chunk.size() > kMaxReasonableLen) {
        rx_accumulator_.clear();
        emit errorOccurred("[Serial] RX accumulator overflow — cleared.");
        return;
    }

    rx_accumulator_.append(chunk);
    processIncoming();
}

void SerialInterface::processIncoming()
{
    // Frames are 0x00-terminated. There may be multiple complete frames.
    int delim_index;
    while ((delim_index = rx_accumulator_.indexOf('\x00')) >= 0) {
        const QByteArray one_frame_no_terminator = rx_accumulator_.left(delim_index);
        rx_accumulator_.remove(0, delim_index + 1); // consume frame + delimiter

        // Basic sanity on encoded length (without 0x00)
        const int min_enc = minCobsEncodedLength(rx_len_);
        const int max_enc = maxCobsEncodedLength(rx_len_);
        if (one_frame_no_terminator.size() < min_enc ||
            one_frame_no_terminator.size() > max_enc) {
            emit errorOccurred(QString("[Serial] Bad frame size: %1 (expected %2..%3)")
                                   .arg(one_frame_no_terminator.size()).arg(min_enc).arg(max_enc));
            continue;
        }

        const QByteArray decoded = cobsDecode(one_frame_no_terminator);
        if (decoded.size() != rx_len_) {
            emit errorOccurred(QString("[Serial] COBS decode failed or wrong length: %1 (expected %2)")
                                   .arg(decoded.size()).arg(rx_len_));
            continue;
        }

        latest_rx_payload_ = decoded;

        if (isRecording_ && logStream_.device())
        {
            const QString ts =
                QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
            logStream_ << ts;
            for (int i = 0; i < latest_rx_payload_.size(); ++i) {
                const auto v = static_cast<unsigned char>(latest_rx_payload_.at(i));
                logStream_ << ',' << v;
            }
            logStream_ << '\n';
        }

        emit dataReceived(latest_rx_payload_);
    }
}

void SerialInterface::saveLatestTxCsv_()
{
    QDir base(QCoreApplication::applicationDirPath());
    const QString dn = base.dirName().toLower();

    if (dn == "release" || dn == "debug") base.cdUp();
    base.mkpath("SerialLogs");
    const QString path = base.filePath("SerialLogs/LatestSentSerial.csv");

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        emit errorOccurred(QString("[Serial] Save latest TX failed: %1").arg(f.errorString()));
        return;
    }

    QTextStream s(&f);
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    s.setEncoding(QStringConverter::Utf8);
#endif

    s << "timestamp";
    for (int i = 0; i < tx_len_; ++i) s << ",b" << i;
    s << '\n';

    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    s << ts;
    for (int i = 0; i < tx_message_.size(); ++i) {
        const auto v = static_cast<unsigned char>(tx_message_.at(i));
        s << ',' << v;
    }
    s << '\n';
}

// ======================= COBS (Consistent Overhead Byte Stuffing) =======================

// Encode algorithm: emit "code" byte followed by up to 254 non-zero bytes, repeat; no zero in payload.
// We return encoded sequence WITHOUT the trailing 0x00 delimiter.
QByteArray SerialInterface::cobsEncode(const QByteArray& input)
{
    if (input.isEmpty()) return QByteArray(); // we assume fixed length > 0
    QByteArray out;
    out.reserve(input.size() + input.size() / 254 + 1);

    int code_index = 0;
    quint8 code = 1;
    out.append('\x00'); // placeholder for first code

    for (unsigned char uc : input) {
        if (uc == 0x00) {
            out[code_index] = static_cast<char>(code);
            code_index = out.size();
            out.append('\x00'); // placeholder for next code
            code = 1;
        } else {
            out.append(static_cast<char>(uc));
            ++code;
            if (code == 0xFF) {
                out[code_index] = static_cast<char>(code);
                code_index = out.size();
                out.append('\x00'); // placeholder for next code
                code = 1;
            }
        }
    }
    out[code_index] = static_cast<char>(code);
    return out;
}

// Decode algorithm: walk code blocks; code=n means (n-1) following non-zero bytes; if code=0 → error.
// Returns empty on malformed input.
QByteArray SerialInterface::cobsDecode(const QByteArray& encoded)
{
    QByteArray out;
    out.reserve(encoded.size()); // approximate

    int i = 0;
    const int n = encoded.size();

    while (i < n) {
        const quint8 code = static_cast<quint8>(encoded.at(i));
        if (code == 0) {
            return QByteArray(); // invalid
        }
        ++i;
        const int copy_len = code - 1;

        if (i + copy_len > n) {
            return QByteArray(); // truncated
        }
        out.append(encoded.constData() + i, copy_len);
        i += copy_len;

        // if code < 0xFF and not at the very end, we insert a zero
        if (code < 0xFF && i < n) {
            out.append('\x00');
        }
    }
    return out;
}

// For a raw payload of length L:
// - Min encoded length occurs when there are no zeros and all code blocks are full (except last).
//   Every 254 bytes forces a code byte. Rough bound: ceil(L/254) code bytes + L data bytes.
//   We return a simple safe bound here.
int SerialInterface::minCobsEncodedLength(int raw_len)
{
    if (raw_len <= 0) return 0;
    const int code_bytes = (raw_len + 253) / 254; // ceil
    return raw_len + code_bytes;
}

// Max encoded length occurs when every byte is zero -> each code byte covers 0 data bytes,
// but COBS still emits a code byte per zero (value 0x01). So encoded length == raw_len + 1.
int SerialInterface::maxCobsEncodedLength(int raw_len)
{
    if (raw_len <= 0) return 0;
    return raw_len + 1;
}
