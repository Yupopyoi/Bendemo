#include "ServoArrayController.h"

// ========================= Constructor / Destructor =========================

/// <summary>
/// Specify the number of motors and allocate memory for the required array.
/// </summary>
/// <param name="numMotors">Number of motors</param>
ServoArrayController::ServoArrayController(int numMotors) : numMotors_(numMotors)
{
    // Allocate array dynamically
    servos_ = new Servo[numMotors_];
    minDeg_ = new int[numMotors_];
    maxDeg_ = new int[numMotors_];
    minUs_ = new int[numMotors_];
    maxUs_ = new int[numMotors_];
    lastOutDeg_ = new float[numMotors_];
    lastMicros_ = new unsigned long[numMotors_];

    // Default values
    for (int i = 0; i < numMotors_; ++i)
    {
        minDeg_[i] = 0;
        maxDeg_[i] = 180;
        minUs_[i] = 500;
        maxUs_[i] = 2500;
        lastOutDeg_[i] = 0.0f;
        lastMicros_[i] = micros();
    }
}

/// <summary>
/// Detach the pin and free the dynamically allocated memory.
/// </summary>
ServoArrayController::~ServoArrayController() 
{
    for (int i = 0; i < numMotors_; ++i)
    {
        servos_[i].detach();
    }

    // Memory freeing
    delete[] servos_;
    delete[] minDeg_;
    delete[] maxDeg_;
    delete[] minUs_;
    delete[] maxUs_;
    delete[] lastOutDeg_;
    delete[] lastMicros_;
}

// ========================= Private Helpers =========================

bool ServoArrayController::isIndexValid_(int motorIndex) const
{
    return (motorIndex >= 0 && motorIndex < numMotors_);
}

float ServoArrayController::clamp_(float deg, float min, float max) const
{
    if (deg < min) return min;
    if (deg > max) return max;
    return deg;
}

/// <summary>
/// Deadbanding (for vibration prevention)
/// If the difference between the previous angle and the current angle is too small (e.g., 0.5�� or less),
/// then that is ignored as noise.
/// </summary>
/// <param name="motorIndex"></param>
/// <param name="targetDeg"></param>
/// <returns></returns>
float ServoArrayController::applyDeadband_(const int motorIndex, const float targetDeg) const
{
    // Difference between last time and this time
    float diff = targetDeg - lastOutDeg_[motorIndex];

    if (fabsf(diff) < deadbandDeg_)
    {
        // If it's about the same as last time, don't rotate the motor.
        return lastOutDeg_[motorIndex];
    }
    else /* There is a enough difference between the last time and this time.*/
    {
        return targetDeg;
    }
}

/// <summary>
/// Slew rate control (prevention of sudden change)
/// Determine the maximum angle (speed limit) you can move at one time and avoid moving too rapidly.
/// </summary>
/// <param name="motorIndex"></param>
/// <param name="targetDeg"></param>
/// <param name="dtSec">How many degrees of movement are allowed per second?</param>
/// <returns></returns>
float ServoArrayController::applySlew_(int motorIndex, float targetDeg, float dtSec) const
{
    if (slewRateDegPerSec_ <= 0.0f) return targetDeg;// Invaild

    float maxStep = slewRateDegPerSec_ * dtSec;
    float diff = targetDeg - lastOutDeg_[motorIndex];

    if (diff > maxStep) return lastOutDeg_[motorIndex] + maxStep;
    if (diff < -maxStep) return lastOutDeg_[motorIndex] - maxStep;

    return targetDeg;
}

void ServoArrayController::writeIfChanged_(int motorIndex, float targetDeg)
{
    targetDeg = clamp_(targetDeg, (float)minDeg_[motorIndex], (float)maxDeg_[motorIndex]);

    float usRange = (float)(maxUs_[motorIndex] - minUs_[motorIndex]);
    float us = minUs_[motorIndex] + (targetDeg / (float)(maxDeg_[motorIndex] - minDeg_[motorIndex])) * usRange;

    int pulse = (int)lroundf(us);

    servos_[motorIndex].writeMicroseconds(pulse);

    lastOutDeg_[motorIndex] = targetDeg;
}

// ========================= Public API =========================

void ServoArrayController::AttachPin(int motorIndex, int pin)
{
    if (!isIndexValid_(motorIndex)) return;

    servos_[motorIndex].attach(pin);
    lastMicros_[motorIndex] = micros();
}

void ServoArrayController::AttachAllPin(const int* pins)
{
    for (int i = 0; i < numMotors_; ++i) 
    {
        AttachPin(i, pins[i]);
    }
}

void ServoArrayController::DetachPin(int motorIndex) 
{
    if (!isIndexValid_(motorIndex)) return;

    servos_[motorIndex].detach();
}

void ServoArrayController::SetMaxMin(int motorIndex, int minDeg, int maxDeg) 
{
    if (!isIndexValid_(motorIndex)) return;

    if (maxDeg < minDeg) 
    {
        int t = maxDeg; maxDeg = minDeg; minDeg = t;
    }

    minDeg_[motorIndex] = minDeg;
    maxDeg_[motorIndex] = maxDeg;
}

void ServoArrayController::SetPulseRangeUs(int motorIndex, int minUs, int maxUs)
{
  if (!isIndexValid_(motorIndex)) return;

    if (maxUs < minUs) 
    {
        int t = maxUs; maxUs = minUs; minUs = t;
    }

    minUs_[motorIndex] = minUs;
    maxUs_[motorIndex] = maxUs;
}

void ServoArrayController::SetDeadbandDeg(float deg) 
{
    deadbandDeg_ = clamp_(deg, 0.0f, 180.0f);
}

void ServoArrayController::SetSlewRateDegPerSec(float degPerSec) 
{
    if (degPerSec < 0.0f) 
    {
        slewRateDegPerSec_ = -1.0f; // Invalid
    }
    else 
    {
        slewRateDegPerSec_ = degPerSec;
    }
}

// ========================= Public API (Rotation) =========================

void ServoArrayController::RotateWithAngleValue(const int motorIndex, const float angleValue)
{
    if (!isIndexValid_(motorIndex)) return;

    unsigned long now = micros();
    float dt = (now - lastMicros_[motorIndex]) * 1e-6f; // [sec]

    // If dt is 0 or unusually large (e.g., after the first or pause),
    // Temporarily use a safe fixed value (0.01s=10ms) to prevent unexpected jumps.
    if (dt <= 0.0f || dt > 0.2f) dt = 0.01f;
    lastMicros_[motorIndex] = now;

    float t = clamp_(angleValue, minDeg_[motorIndex], maxDeg_[motorIndex]);
    t = applyDeadband_(motorIndex, t);
    t = applySlew_(motorIndex, t, dt);

    writeIfChanged_(motorIndex, t);
}

void ServoArrayController::RotateWithAnalogInput(int motorIndex, float analogInput)
{
    if (!isIndexValid_(motorIndex)) return;

    analogInput = clamp_(analogInput, 0.0f, 1023.0f);

    float minD = (float)minDeg_[motorIndex];
    float maxD = (float)maxDeg_[motorIndex];
    float span = maxD - minD;

    float angle = minD + (analogInput / 1023.0f) * span;
    RotateWithAngleValue(motorIndex, angle);
}

void ServoArrayController::RotateToMax(int motorIndex) 
{
    if (!isIndexValid_(motorIndex)) return;

    RotateWithAngleValue(motorIndex, (float)maxDeg_[motorIndex]);
}

void ServoArrayController::RotateToMin(int motorIndex) 
{
    if (!isIndexValid_(motorIndex)) return;

    RotateWithAngleValue(motorIndex, (float)minDeg_[motorIndex]);
}

float ServoArrayController::GetLastAngle(int motorIndex) const 
{
    if (!isIndexValid_(motorIndex)) return 0.0f;

    return lastOutDeg_[motorIndex];
}
