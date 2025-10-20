#ifndef AUTOBENDING_H
#define AUTOBENDING_H

#include <QDebug>
#include <QElapsedTimer>

#include <algorithm>

#define _USE_MATH_DEFINES
#include <math.h>

class AutoBending
{
public:
    AutoBending() = default;

    // —— 制御ループ（誤差[px]を入れて、出力[モーター単位の増分]を返す）——
    // 返り値: true=計算有効, false=無効（enabled_=false）
    bool step(double differenceX_px, double differenceY_px,
              double& outDeltaX, double& outDeltaY);

    // 有効/無効
    void setEnabled(bool on) { enabled_ = on; if (!on) reset(); }
    bool isEnabled() const { return enabled_; }

    // ゲイン・各種設定
    void setGains(double Kp, double Ki, double Kd) { Kp_ = Kp; Ki_ = Ki; Kd_ = Kd; }
    void setDeadband(double px) { deadband_px_ = std::max(0.0, px); }
    void setOutputSaturation(double absMax) { outAbsMax_ = std::max(0.0, absMax); }
    void setDerivativeCutoffHz(double hz) { dCutHz_ = std::max(0.0, hz); }

    // 幾何: px → モーター単位（pxPerUnit=20 → 20pxで1.0 unit）
    void setGeometry(double pxPerUnitX, double pxPerUnitY) {
        pxPerUnitX_ = std::max(1e-9, pxPerUnitX);
        pxPerUnitY_ = std::max(1e-9, pxPerUnitY);
    }

    // モーター index（取得用）
    int motorIndexX() const { return motorIndexX_; }
    int motorIndexY() const { return motorIndexY_; }
    void setMotorIndices(int idxX, int idxY) { motorIndexX_ = idxX; motorIndexY_ = idxY; } // 必要なら使用

    // 状態リセット
    void reset();

private:
    // 片軸 PID
    double pidAxis_(double err_px, double dt, double pxPerUnit,
                    double& integ, double& prevErrUnit, double& dState);

private:
    bool enabled_ = true;

    // PID ゲイン
    double Kp_ = 0.01;
    double Ki_ = 0.00;
    double Kd_ = 0.00;

    // D ローパス
    double dCutHz_ = 5.0;

    // 出力制限
    double outAbsMax_ = 2.0;

    // デッドバンド
    double deadband_px_ = 2.0;

    // 幾何換算
    double pxPerUnitX_ = 25.0;
    double pxPerUnitY_ = 25.0;

    // モーター index（取得用）
    int motorIndexX_ = 0;
    int motorIndexY_ = 1;

    // 内部状態
    double integX_ = 0.0, integY_ = 0.0;
    double prevErrUnitX_ = 0.0, prevErrUnitY_ = 0.0;
    double dStateX_ = 0.0, dStateY_ = 0.0;

    QElapsedTimer t_;
    bool started_ = false;
};

#endif // AUTOBENDING_H
