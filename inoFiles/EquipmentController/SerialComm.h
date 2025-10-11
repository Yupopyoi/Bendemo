#ifndef SERIALCOMM_H
#define SERIALCOMM_H

#include <Arduino.h>

class SerialComm {
    public:
        SerialComm(HardwareSerial &serial, unsigned long baudRate = 115200);

        void begin();

        size_t send(const uint8_t *data, size_t length);

        size_t receive(uint8_t *buffer, size_t length);

        bool isAvailable();

    private:
        HardwareSerial &serial;
        unsigned long baudRate;

        // COBSエンコード
        void encode(const uint8_t *buffer, size_t size, uint8_t *encodedBuffer);

        // COBSデコード
        void decode(const uint8_t *encodedBuffer, size_t size, uint8_t *decodedBuffer);
};

#endif // SERIALCOMM_H
