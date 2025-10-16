#include "AutoBending.h"

void AutoBending::reset()
{
    integX_ = integY_ = 0.0;
    prevErrUnitX_ = prevErrUnitY_ = 0.0;
    dStateX_ = dStateY_ = 0.0;
    started_ = false;
    t_.invalidate();
}

double AutoBending::pidAxis_(double err_px, double dt, double pxPerUnit,
                             double& integ, double& prevErrUnit, double& dState)
{
    // デッドバンド
    if (std::abs(err_px) < deadband_px_) err_px = 0.0;

    // px → unit
    const double err_unit = err_px / pxPerUnit;

    // P
    const double P = Kp_ * err_unit;

    // I
    integ += Ki_ * err_unit * dt;

    // D（LPF付）
    double D = 0.0;
    if (dt > 0.0 && Kd_ > 0.0) {
        const double rawD = (err_unit - prevErrUnit) / dt;
        const double alpha = 1.0 / (1.0 + (2.0 * M_PI * dCutHz_) * dt);
        dState = alpha * dState + (1.0 - alpha) * rawD;
        D = Kd_ * dState;
    }
    prevErrUnit = err_unit;

    // 合成
    double u = P + integ + D;

    // 飽和 + ざっくりアンチワインドアップ
    //const double uSat = std::clamp(u, -outAbsMax_, outAbsMax_);
    //if (u != uSat) {
    //    integ += (uSat - u) * 0.5;
    //    u = uSat;
    //}
    return u;
}

bool AutoBending::step(double differenceX_px, double differenceY_px,
                       double& outDeltaX, double& outDeltaY)
{
    if (!enabled_) { outDeltaX = outDeltaY = 0.0; return false; }

    double dt = 0.02;
    if (!started_) {
        t_.start();
        started_ = true;
    } else {
        dt = std::max(1e-3, t_.restart() / 1000.0);
    }

    const double uX = pidAxis_(differenceX_px, dt, pxPerUnitX_,
                               integX_, prevErrUnitX_, dStateX_);
    const double uY = pidAxis_(differenceY_px, dt, pxPerUnitY_,
                               integY_, prevErrUnitY_, dStateY_);

    outDeltaX = uX;
    outDeltaY = uY;
    return true;
}
