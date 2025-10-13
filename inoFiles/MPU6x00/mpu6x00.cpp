#include "mpu6x00.h"

// ---- low-level I2C ----
bool Mpu6x00::writeByte(uint8_t reg, uint8_t data) {
    wire_->beginTransmission(addr_);
    wire_->write(reg);
    wire_->write(data);
    return (wire_->endTransmission() == 0);
}

bool Mpu6x00::readBytes(uint8_t reg, uint8_t* buf, size_t len) {
    wire_->beginTransmission(addr_);
    wire_->write(reg);
    if (wire_->endTransmission(false) != 0) return false;
    if (wire_->requestFrom((int)addr_, (int)len) != (int)len) return false;
    for (size_t i = 0; i < len; ++i) buf[i] = wire_->read();
    return true;
}

// ---- public ----
bool Mpu6x00::begin(TwoWire* wire, uint32_t i2cClock) {
    wire_ = wire ? wire : &Wire;
    wire_->begin();
    wire_->setClock(i2cClock);

    // wake up
    if (!writeByte(REG_PWR_MGMT_1, 0x00)) return false; // clear sleep
    delay(100);

    // ��{�ݒ�iDLPF=42Hz�����j
    writeByte(REG_CONFIG, 0x03);

    // ����X�P�[��
    setAccelScale(2);
    setGyroScale(250);

    // WhoAmI �̊ȈՃ`�F�b�N�i�C�Ӂj
    uint8_t id = 0;
    readBytes(REG_WHO_AM_I, &id, 1);
    // MPU6050=0x68, MPU6500=0x70/0x71�n���i�����`�F�b�N�͏ȗ��j

    lastUs_ = micros();
    return true;
}

void Mpu6x00::setAccelScale(int gSel) {
    uint8_t val = 0;
    switch (gSel) {
    case 2:  val = 0 << 3; accelLSBperG_ = 16384.0f; break;
    case 4:  val = 1 << 3; accelLSBperG_ = 8192.0f;  break;
    case 8:  val = 2 << 3; accelLSBperG_ = 4096.0f;  break;
    case 16: val = 3 << 3; accelLSBperG_ = 2048.0f;  break;
    default: val = 0 << 3; accelLSBperG_ = 16384.0f; break;
    }
    writeByte(REG_ACCEL_CONFIG, val);
}

void Mpu6x00::setGyroScale(int dpsSel) {
    uint8_t val = 0;
    switch (dpsSel) {
    case 250:  val = 0 << 3; gyroLSBperDPS_ = 131.0f;   break;
    case 500:  val = 1 << 3; gyroLSBperDPS_ = 65.5f;    break;
    case 1000: val = 2 << 3; gyroLSBperDPS_ = 32.8f;    break;
    case 2000: val = 3 << 3; gyroLSBperDPS_ = 16.4f;    break;
    default:   val = 0 << 3; gyroLSBperDPS_ = 131.0f;   break;
    }
    writeByte(REG_GYRO_CONFIG, val);
}

void Mpu6x00::calibrate(uint16_t samples, uint16_t delayMs) {
    // �S���Q�L�����u���[�V�����F�Î~��Ԃ�
    double sumGx = 0, sumGy = 0, sumGz = 0;
    double sumPitch = 0, sumRoll = 0;

    for (uint16_t i = 0; i < samples; ++i) {
        update(); // �ŐV�l��
        sumGx += gx_dps_;
        sumGy += gy_dps_;
        sumGz += gz_dps_;

        float ap, ar;
        accelAngles_(ap, ar);
        sumPitch += ap;
        sumRoll += ar;

        delay(delayMs);
    }
    gox_ = sumGx / samples;
    goy_ = sumGy / samples;
    goz_ = sumGz / samples;

    aox_ = sumPitch / samples;
    aoy_ = sumRoll / samples;
}

bool Mpu6x00::update() {
    // 6���ǂݏo���iAccel 6B + Temp 2B + Gyro 6B = 14B�j
    uint8_t buf[14];
    if (!readBytes(REG_ACCEL_XOUT_H, buf, sizeof(buf))) return false;

    auto rd16 = [&](int idx)->int16_t {
        return (int16_t)((buf[idx] << 8) | buf[idx + 1]);
    };
    ax_ = rd16(0);
    ay_ = rd16(2);
    az_ = rd16(4);
    // temp = rd16(6); // ���g�p
    gx_ = rd16(8);
    gy_ = rd16(10);
    gz_ = rd16(12);

    ax_g_ = ax_ / accelLSBperG_;
    ay_g_ = ay_ / accelLSBperG_;
    az_g_ = az_ / accelLSBperG_;

    gx_dps_ = gx_ / gyroLSBperDPS_ - gox_;
    gy_dps_ = gy_ / gyroLSBperDPS_ - goy_;
    gz_dps_ = gz_ / gyroLSBperDPS_ - goz_;

    // ��t
    uint32_t now = micros();
    float dt = (now - lastUs_) * 1e-6f;
    if (dt <= 0 || dt > 0.2f) dt = 0.01f; // �K�[�h
    lastUs_ = now;

    // �����x����p�x�ideg�j
    float aPitch, aRoll;
    accelAngles_(aPitch, aRoll);
    aPitch -= aox_;
    aRoll -= aoy_;

    // ����t�B���^�F�p���x�����{�����x�p�̃u�����h
    // Pitch �� GyroX, Roll �� GyroY �̑Ή��i�Z���T���t���ɂ����ւ̉\������j
    float predPitch = pitchDeg_ + gx_dps_ * dt;
    float predRoll = rollDeg_ + gy_dps_ * dt;

    pitchDeg_ = alpha_ * predPitch + (1.0f - alpha_) * aPitch;
    rollDeg_ = alpha_ * predRoll + (1.0f - alpha_) * aRoll;

    return true;
}

void Mpu6x00::accelAngles_(float& pitchDeg, float& rollDeg) const {
    // �E��n�z��Fpitch = atan2(-Ax, sqrt(Ay^2 + Az^2))�Aroll = atan2(Ay, Az)
    // ���t�������ŕ����E���͒������Ă�������
    const float ax = ax_g_, ay = ay_g_, az = az_g_;
    pitchDeg = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / PI;
    rollDeg = atan2f(ay, az) * 180.0f / PI;
}
