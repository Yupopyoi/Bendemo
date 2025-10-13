#pragma once
#include <Arduino.h>
#include <Wire.h>

/**
 * Mpu6x00: MPU6050/6500 minimal driver with complementary filter.
 *  - begin(): init & wake up
 *  - calibrate(): estimate gyro/accel offsets (board at rest)
 *  - update(): read sensors, compute filtered roll/pitch
 *  - getters: roll(), pitch(), raw & dps/g values
 */
class Mpu6x00 {
public:
    explicit Mpu6x00(uint8_t i2cAddr = 0x68) : addr_(i2cAddr) {}

    bool begin(TwoWire* wire = &Wire, uint32_t i2cClock = 400000);
    void setAlpha(float a) { alpha_ = constrain(a, 0.0f, 1.0f); } // 0.95–0.98 推奨
    void setAccelScale(int gSel);     // 2/4/8/16 (g)
    void setGyroScale(int dpsSel);    // 250/500/1000/2000 (deg/s)
    void calibrate(uint16_t samples = 500, uint16_t delayMs = 2);

    // 一回の測定・更新（成功なら true）
    bool update();

    // 取得系
    float roll()  const { return rollDeg_; }   // deg
    float pitch() const { return pitchDeg_; }  // deg

    // センサ生・換算値（直近）
    void rawAccel(int16_t& ax, int16_t& ay, int16_t& az) const { ax = ax_; ay = ay_; az = az_; }
    void rawGyro(int16_t& gx, int16_t& gy, int16_t& gz) const { gx = gx_; gy = gy_; gz = gz_; }
    void accelG(float& xg, float& yg, float& zg) const { xg = ax_g_; yg = ay_g_; zg = az_g_; }
    void gyroDps(float& x, float& y, float& z)  const { x = gx_dps_; y = gy_dps_; z = gz_dps_; }

    // オフセット設定/取得
    void setGyroOffset(float x, float y, float z) { gox_ = x; goy_ = y; goz_ = z; }
    void setAccelOffset(float x, float y) { aox_ = x; aoy_ = y; }
    void getGyroOffset(float& x, float& y, float& z) const { x = gox_; y = goy_; z = goz_; }
    void getAccelOffset(float& x, float& y)         const { x = aox_; y = aoy_; }

private:
    // レジスタ
    static constexpr uint8_t REG_PWR_MGMT_1 = 0x6B;
    static constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
    static constexpr uint8_t REG_GYRO_XOUT_H = 0x43;
    static constexpr uint8_t REG_CONFIG = 0x1A;
    static constexpr uint8_t REG_GYRO_CONFIG = 0x1B;
    static constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
    static constexpr uint8_t REG_WHO_AM_I = 0x75;

    bool writeByte(uint8_t reg, uint8_t data);
    bool readBytes(uint8_t reg, uint8_t* buf, size_t len);

    void updateScales_();   // fs設定→スケール係数更新
    void accelAngles_(float& pitchDeg, float& rollDeg) const; // 加速度のみ角度

private:
    TwoWire* wire_ = &Wire;
    uint8_t  addr_ = 0x68;

    // スケール（LSB→物理量）
    float accelLSBperG_ = 16384.0f;   // ±2g
    float gyroLSBperDPS_ = 131.0f;    // ±250 dps

    // オフセット
    float gox_ = 0, goy_ = 0, goz_ = 0;     // gyro dps offset
    float aox_ = 0, aoy_ = 0;             // accel angle offset (deg) for pitch/roll

    // 生値
    int16_t ax_ = 0, ay_ = 0, az_ = 0, gx_ = 0, gy_ = 0, gz_ = 0;
    // 換算値
    float ax_g_ = 0, ay_g_ = 0, az_g_ = 0;
    float gx_dps_ = 0, gy_dps_ = 0, gz_dps_ = 0;

    // フィルタ状態
    float pitchDeg_ = 0, rollDeg_ = 0;
    float alpha_ = 0.95f;

    // 時間管理
    uint32_t lastUs_ = 0;
};
