#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <MPU6050.h>
#include <math.h>

/**
 * ImuComplementary
 * - Electronic Cats の MPU6050 ライブラリで MPU-6500/6050 を読み、
 *   ソフト補正(平均バイアス引き) + 相補フィルタで Roll/Pitch/Yaw を算出する薄いラッパ
 */
class ImuComplementary {
public:
    explicit ImuComplementary(uint8_t i2c_addr = 0x68, float alpha = 0.98f);

    // 初期化：I2C開始、MPU初期化、DLPF/Rate設定
    // i2c_clock=100kHz から始め、安定後に 400kHz へ上げるのが無難
    bool begin(uint32_t i2c_clock_hz = 100000, int dlpf_mode = 3, int rate_div = 9);

    // 静置時の平均でソフトバイアス（counts）を推定
    void calibrateSoftware(int samples = 2000, int usDelay = 1000);

    // 既知のバイアスを「counts」で直接設定（加速度:LSB、ジャイロ:LSB）
    void setManualBiasCounts(long ax_c, long ay_c, long az_c,
                             long gx_c, long gy_c, long gz_c);

    // 既知のバイアスを「物理量」で設定（加速度:g、ジャイロ:dps）
    void setManualBiasPhysical(float ax_g, float ay_g, float az_g,
                               float gx_dps, float gy_dps, float gz_dps);

    // 1サイクル更新（読み取り→補正→相補フィルタ）。成功時 true
    bool update();

    // 角度取得（deg）
    float roll()  const { return roll_deg_;  }
    float pitch() const { return pitch_deg_; }
    float yaw()   const { return yaw_deg_;   }

    void getRPYBytes(uint8_t out6[6]) const;

    // ヨーのみゼロリセット
    void zeroYaw() { yaw_deg_ = 0.0f; }

    // 現在の生値（counts）を取得
    void rawAccel(int16_t& ax, int16_t& ay, int16_t& az) const { ax = ax_; ay = ay_; az = az_; }
    void rawGyro (int16_t& gx, int16_t& gy, int16_t& gz) const { gx = gx_; gy = gy_; gz = gz_; }

    // 現在の補正後物理量（g, dps）を取得
    void accelG(float& xg, float& yg, float& zg) const { xg = ax_g_; yg = ay_g_; zg = az_g_; }
    void gyroDps(float& xd, float& yd, float& zd) const { xd = gx_dps_; yd = gy_dps_; zd = gz_dps_; }

    // 相補フィルタ係数を変更（0.95〜0.99目安）
    void setAlpha(float a) { alpha_ = constrain(a, 0.0f, 1.0f); }

    // レンジ変更（必要なときのみ）
    void setAccelRange(uint8_t fs) { mpu_.setFullScaleAccelRange(fs); }
    void setGyroRange (uint8_t fs) { mpu_.setFullScaleGyroRange(fs);  }

private:
    float accelCountsPerG_() const;
    float gyroCountsPerDPS_() const;

private:
    MPU6050 mpu_;
    uint8_t addr_;

    // 生値（counts）
    int16_t ax_ = 0, ay_ = 0, az_ = 0, gx_ = 0, gy_ = 0, gz_ = 0;

    // 補正後（物理量）
    float ax_g_ = 0, ay_g_ = 0, az_g_ = 0;
    float gx_dps_ = 0, gy_dps_ = 0, gz_dps_ = 0;

    // ソフトバイアス（counts）
    long ax_bias_ = 0, ay_bias_ = 0, az_bias_ = 0;
    long gx_bias_ = 0, gy_bias_ = 0, gz_bias_ = 0;

    // 姿勢状態（deg）
    float roll_deg_ = 0.0f, pitch_deg_ = 0.0f, yaw_deg_ = 0.0f;

    // 相補フィルタ係数
    float alpha_;

    // Δt
    uint32_t last_us_ = 0;

    // 更新が初回かどうか
    bool first_update_ = true;

    static float wrap180f_(float a);
    static void packAngleDeg100_(float deg, uint8_t out2[2]); // 角度→2バイト
};
