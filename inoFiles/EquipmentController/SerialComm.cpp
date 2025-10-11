#include "SerialComm.h"

SerialComm::SerialComm(HardwareSerial &serial, unsigned long baudRate) : serial(serial), baudRate(baudRate) {}

void SerialComm::begin()
{
    serial.begin(baudRate);
}

size_t SerialComm::send(const uint8_t *data, size_t length)
{
    // エンコード
    uint8_t encodedBuffer[length + 2]; // COBSは2バイト増加
    encode(data, length, encodedBuffer);

    // エンコードしたデータを送信
    return serial.write(encodedBuffer, length + 2);
}

size_t SerialComm::receive(uint8_t *buffer, size_t length)
{
    static uint8_t receiveBuffer[64]; // 受信データの一時保存用
    static size_t bufferIndex = 0;   // 現在のバッファ位置

    // シリアルからデータを読み込む
    while (serial.available())
    {
        uint8_t byte = serial.read();

        if (byte == 0x00)
        {
            // COBSパケットの終端に到達
            // デコードしたデータを返す
            decode(receiveBuffer, bufferIndex, buffer);

            // バッファをリセット
            bufferIndex = 0;

            // デコード結果の長さを返す
            return bufferIndex;
        }
        else
        {
            // 受信データを一時バッファに格納
            if (bufferIndex < sizeof(receiveBuffer))
            {
                receiveBuffer[bufferIndex++] = byte;
            }
        }
    }

    // データがまだ完全に受信されていない場合は0を返す
    return 0;
}

// 受信可能か確認
bool SerialComm::isAvailable() {
    return serial.available() > 0;
}

void SerialComm::encode(const uint8_t *buffer, size_t size, uint8_t *encodedBuffer)
{
    size_t readIndex = 0;
    size_t writeIndex = 1;
    size_t codeIndex = 0;
    uint8_t code = 1;

    while (readIndex < size)
    {
        if (buffer[readIndex] == 0)
        {
            encodedBuffer[codeIndex] = code;
            code = 1;
            codeIndex = writeIndex++;
            readIndex++;
        }
        else
        {
            encodedBuffer[writeIndex++] = buffer[readIndex++];
            code++;

            if (code == 0xFF)
            {
                encodedBuffer[codeIndex] = code;
                code = 1;
                codeIndex = writeIndex++;
            }
        }
    }

    encodedBuffer[codeIndex] = code;
    encodedBuffer[size + 1] = 0x00; // 終端文字を追加
}

void SerialComm::decode(const uint8_t *encodedBuffer, size_t size, uint8_t *decodedBuffer)
{
    if (size == 0) return;

    int zeroIndex = encodedBuffer[0] - 1;
    for (int i = 0; i < (int)size - 2; i++)
    {
        if (i == zeroIndex)
        {
            decodedBuffer[zeroIndex] = 0;
            zeroIndex += encodedBuffer[i + 1];
        }
        else
        {
            decodedBuffer[i] = encodedBuffer[i + 1];
        }
    }
}
