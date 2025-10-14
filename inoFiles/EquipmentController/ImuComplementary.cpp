#include "ImuComplementary.h"

ImuComplementary::ImuComplementary(uint8_t i2c_addr, float alpha)
: mpu_(i2c_addr), addr_(i2c_addr), alpha_(alpha) {}

bool ImuComplementary::begin(uint32_t i2c_clock_hz, int dlpf_mode, int rate_div) {
    Wire.begin();
    Wire.setClock(i2c_clock_hz);
    delay(300);

    mpu_.initialize();
    mpu_.setSleepEnabled(false);

    // DLPF/Rate 設定（安定性重視の初期値）
    mpu_.setDLPFMode(dlpf_mode); // 3 ≈ ~44Hz
    mpu_.setRate(rate_div);      // 9 → ~100Hz（DLPF時は 1kHz/(1+rate)）

    // 最初の時刻セット
    last_us_ = micros();
    first_update_ = true;

    return true; // WHO_AM_I=0x70(6500)でもOKにするため、ここでは false を返さない
}

void ImuComplementary::calibrateSoftware(int samples, int usDelay) {
    long sax=0, say=0, saz=0, sgx=0, sgy=0, sgz=0;

    // 捨てサンプルで温度・I2C準備の揺らぎを減らす
    for (int i=0; i<100; ++i) {
        mpu_.getMotion6(&ax_, &ay_, &az_, &gx_, &gy_, &gz_);
        delayMicroseconds(usDelay);
    }

    for (int i=0; i<samples; ++i) {
        mpu_.getMotion6(&ax_, &ay_, &az_, &gx_, &gy_, &gz_);
        sax += ax_; say += ay_; saz += az_;
        sgx += gx_; sgy += gy_; sgz += gz_;
        delayMicroseconds(usDelay);
    }

    long ax_avg = sax / samples;
    long ay_avg = say / samples;
    long az_avg = saz / samples;
    long gx_avg = sgx / samples;
    long gy_avg = sgy / samples;
    long gz_avg = sgz / samples;

    const long AZ_TARGET = (long)accelCountsPerG_(); // 静置Zは +1g を目標
    ax_bias_ = ax_avg;
    ay_bias_ = ay_avg;
    az_bias_ = az_avg - AZ_TARGET;
    gx_bias_ = gx_avg;
    gy_bias_ = gy_avg;
    gz_bias_ = gz_avg;
}

void ImuComplementary::setManualBiasCounts(long ax_c, long ay_c, long az_c,
                                           long gx_c, long gy_c, long gz_c) {
    ax_bias_ = ax_c; ay_bias_ = ay_c; az_bias_ = az_c;
    gx_bias_ = gx_c; gy_bias_ = gy_c; gz_bias_ = gz_c;
}

void ImuComplementary::setManualBiasPhysical(float ax_g, float ay_g, float az_g,
                                             float gx_dps, float gy_dps, float gz_dps) {
    // 物理量 → counts へ変換
    const float aLSB = accelCountsPerG_();
    const float gLSB = gyroCountsPerDPS_();
    ax_bias_ = lroundf(ax_g * aLSB);
    ay_bias_ = lroundf(ay_g * aLSB);
    // Z は +1g を目標にするので (平均値 - 1g) をバイアスにするのが一般的だが、
    // ここでは「与えられた物理バイアスをそのまま引く」仕様にする：
    az_bias_ = lroundf(az_g * aLSB);
    gx_bias_ = lroundf(gx_dps * gLSB);
    gy_bias_ = lroundf(gy_dps * gLSB);
    gz_bias_ = lroundf(gz_dps * gLSB);
}

bool ImuComplementary::update() {
    // センサ読み取り
    mpu_.getMotion6(&ax_, &ay_, &az_, &gx_, &gy_, &gz_);

    // 初回は時刻合わせだけして抜ける
    uint32_t now = micros();
    if (first_update_) {
        last_us_ = now;
        first_update_ = false;
        return false;
    }

    // Δt[s]
    float dt = (now - last_us_) * 1e-6f;
    last_us_ = now;
    if (dt <= 0.0f || dt > 0.5f) dt = 0.01f; // 非常時ガード

    // ソフト補正（counts → 物理量）
    const float aLSB = accelCountsPerG_();
    const float gLSB = gyroCountsPerDPS_();

    float axc = (float)((long)ax_ - ax_bias_);
    float ayc = (float)((long)ay_ - ay_bias_);
    float azc = (float)((long)az_ - az_bias_);
    float gxc = (float)((long)gx_ - gx_bias_);
    float gyc = (float)((long)gy_ - gy_bias_);
    float gzc = (float)((long)gz_ - gz_bias_);

    ax_g_   = axc / aLSB;
    ay_g_   = ayc / aLSB;
    az_g_   = azc / aLSB;
    gx_dps_ = gxc / gLSB;
    gy_dps_ = gyc / gLSB;
    gz_dps_ = gzc / gLSB;

    // 加速度からの姿勢（deg）
    float pitch_acc = atan2f(-ax_g_, sqrtf(ay_g_ * ay_g_ + az_g_ * az_g_)) * 180.0f / PI;
    float roll_acc  = atan2f( ay_g_, az_g_) * 180.0f / PI;

    // 相補フィルタ
    pitch_deg_ = alpha_ * (pitch_deg_ + gy_dps_ * dt) + (1.0f - alpha_) * pitch_acc;
    roll_deg_  = alpha_ * (roll_deg_  + gx_dps_ * dt) + (1.0f - alpha_) * roll_acc;

    // ヨーはジャイロ積分のみ（磁気センサなし → ドリフトあり）
    yaw_deg_ += gz_dps_ * dt;
    // 正規化 [-180,180)
    while (yaw_deg_ >= 180.0f) yaw_deg_ -= 360.0f;
    while (yaw_deg_ <  -180.0f) yaw_deg_ += 360.0f;

    return true;
}

float ImuComplementary::accelCountsPerG_() const {
    switch (mpu_.getFullScaleAccelRange()) {
        case 0: return 16384.0f; // ±2g
        case 1: return 8192.0f;  // ±4g
        case 2: return 4096.0f;  // ±8g
        case 3: return 2048.0f;  // ±16g
        default: return 16384.0f;
    }
}
float ImuComplementary::gyroCountsPerDPS_() const {
    switch (mpu_.getFullScaleGyroRange()) {
        case 0: return 131.0f; // ±250 dps
        case 1: return 65.5f;  // ±500 dps
        case 2: return 32.8f;  // ±1000 dps
        case 3: return 16.4f;  // ±2000 dps
        default: return 131.0f;
    }
}

float ImuComplementary::wrap180f_(float a) {
    while (a >= 180.0f) a -= 360.0f;
    while (a <  -180.0f) a += 360.0f;
    return a;
}

void ImuComplementary::packAngleDeg100_(float deg, uint8_t out2[2]) {
    deg = wrap180f_(deg);
    long v = lroundf(deg * 100.0f);  // 小数2桁
    if (v < -18000) v = -18000;
    if (v >  18000) v =  18000;
    int16_t s = (int16_t)v;
    // ビッグエンディアン（MSB→LSB）
    out2[0] = (uint8_t)((s >> 8) & 0xFF);
    out2[1] = (uint8_t)(s & 0xFF);

    // ※ リトルエンディアンで送りたいなら、上の2行を入れ替えてください:
    // out2[0] = (uint8_t)(s & 0xFF);
    // out2[1] = (uint8_t)((s >> 8) & 0xFF);
}

void ImuComplementary::getRPYBytes(uint8_t out6[6]) const {
    uint8_t tmp[2];
    packAngleDeg100_(roll_deg_,  tmp); out6[0] = tmp[0]; out6[1] = tmp[1];
    packAngleDeg100_(pitch_deg_, tmp); out6[2] = tmp[0]; out6[3] = tmp[1];
    packAngleDeg100_(yaw_deg_,   tmp); out6[4] = tmp[0]; out6[5] = tmp[1];
}

