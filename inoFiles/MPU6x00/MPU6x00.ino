#include "mpu6x00.h"

Mpu6x00 imu(0x68);

void setup() {
  Serial.begin(115200);
  delay(100);
  if (!imu.begin(&Wire, 400000)) {
    Serial.println("MPU init failed");
    while(1);
  }

  // 必要ならスケール変更
  // imu.setAccelScale(2);   // 2/4/8/16 g
  // imu.setGyroScale(250);  // 250/500/1000/2000 dps
  imu.setAlpha(0.95f);

  Serial.println("Calibrating... keep the board still.");
  imu.calibrate(300, 5);
  Serial.println("Done.");
}

void loop() {
  if (imu.update()) {
    Serial.print("Pitch(deg): ");
    Serial.print(imu.pitch(), 2);
    Serial.print("  Roll(deg): ");
    Serial.println(imu.roll(), 2);
  }
  delay(10);
}
