// ---------------------------------------- シリアル通信 ----------------------------------------

#include "SerialComm.h"

// シリアル通信用インスタンス
SerialComm serialComm(Serial, 115200);

// データバッファのサイズ
const size_t INPUT_BUFFER_SIZE{ 30 };
uint8_t readDataBuffer_[INPUT_BUFFER_SIZE];

const size_t OUTPUT_BUFFER_SIZE{ 22 };
uint8_t writeDataBuffer_[OUTPUT_BUFFER_SIZE];

// ----------------------------------------  ピン割当て ----------------------------------------




// ----------------------------------------  サーボモーター ----------------------------------------

#include <Servo.h>
#include "ServoArrayController.h"

static const int kMotors = 6;
static const int SERVO_PINS[kMotors] = {5, 6, 0, 0, 0, 0};

ServoArrayController servos(kMotors);

// ---------------------------------------- 接続確認カウンター ----------------------------------------

uint8_t counter_{ 0 };
uint8_t subcounter_{ 0 };

// ---------------------------------------- IMU ----------------------------------------

#include "mpu6x00.h"

Mpu6x00 imu(0x68);

//  ------------------------------------------------------------------------------------------------

void setup()
{
  setupMotors();
  serialComm.begin();
}


void setupMotors()
{
  servos.SetDeadbandDeg(0.5f);
  servos.SetSlewRateDegPerSec(300.0f);
  servos.AttachAllPin(SERVO_PINS);

  for(int motorIndex = 0; motorIndex < kMotors; motorIndex++)
  {
    servos.SetMaxMin(motorIndex, 0, 270);
    servos.SetPulseRangeUs(motorIndex, 500, 2500);

    // Default Value
    servos.RotateWithAngleValue(motorIndex, 135.0f);
  }
}

void loop() 
{
  // 受信データがある場合
  if (serialComm.isAvailable()) {
    // データを受信してバッファに格納
    size_t recievedLength = serialComm.receive(readDataBuffer_, INPUT_BUFFER_SIZE);
  }
  
  OperateMotors();

  SerialWrite();
}

void SerialWrite() 
{
  // 値の割当て
  writeDataBuffer_[0] = (counter_ % 256);
  writeDataBuffer_[1] = readDataBuffer_[0];
  writeDataBuffer_[2] = readDataBuffer_[1];
  writeDataBuffer_[3] = readDataBuffer_[2];
  writeDataBuffer_[4] = readDataBuffer_[3];
  writeDataBuffer_[5] = readDataBuffer_[4];
  writeDataBuffer_[6] = readDataBuffer_[5];
  writeDataBuffer_[7] = readDataBuffer_[6];
  writeDataBuffer_[8] = readDataBuffer_[7];
  writeDataBuffer_[9] = readDataBuffer_[8];
  writeDataBuffer_[10] = readDataBuffer_[9];
  writeDataBuffer_[11] = 0x00;
  writeDataBuffer_[12] = 0x00;
  writeDataBuffer_[13] = 0x00;
  writeDataBuffer_[14] = 0x00;
  writeDataBuffer_[15] = 0x00;
  writeDataBuffer_[16] = 0x00;
  writeDataBuffer_[17] = 0x00;
  writeDataBuffer_[18] = 0x00;
  writeDataBuffer_[19] = 0x00;
  writeDataBuffer_[20] = 0x00;
  writeDataBuffer_[21] = 0x00;
  
  if(subcounter_++ == 255) counter_++;

  // データの送信
  serialComm.send(writeDataBuffer_, OUTPUT_BUFFER_SIZE);
}

void OperateMotors()
{
  // Servo : Target Set & Update
    for(int motorIndex = 0; motorIndex < kMotors; motorIndex++)
    {
        // Translation of angular data into int , because each of them is represented by 2 bytes.
        float target = (float)(readDataBuffer_[motorIndex * 2] << 8 | readDataBuffer_[motorIndex * 2 + 1]) / 10.0f;
        servos.RotateWithAngleValue(motorIndex, target);
    }
}
