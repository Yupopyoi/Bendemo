#pragma once
#include <Arduino.h>
#include <Servo.h>

class ServoArrayController {
public:
	// Specify the number of motors in the constructor.
	// Allocate memory for the required array within the constructor.
	explicit ServoArrayController(int numMotors);
	~ServoArrayController();

	void AttachPin(int motorIndex, int pin);
	void AttachAllPin(const int* pins /* length = numMotors_ */);
	void DetachPin(int motorIndex);

	// Angle Upper and Lower Limit Settings, Default : 0, 180
	void SetMaxMin(int motorIndex, int minDeg, int maxDeg);

	// Pulse Upper and Lower Limit Settings, Default : 500, 2500
  void SetPulseRangeUs(int motorIndex, int minUs, int maxUs);

	void SetDeadbandDeg(float deg);
	void SetSlewRateDegPerSec(float degPerSec);

	// Rotate by specified angle
	void RotateWithAngleValue(const int motorIndex, const float angleValue);

	// Rotate by linearly mapping analog inputs of 0-1023 to min-max angles.
	void RotateWithAnalogInput(int motorIndex, float analogInput);

	void RotateToMax(int motorIndex);
	void RotateToMin(int motorIndex);

	// For Debug
	float GetLastAngle(int motorIndex) const;

private:
	int numMotors_;

	// First address of arrays
	Servo* servos_;
	int* minDeg_;
	int* maxDeg_;
	int* minUs_; // us = microseconds
	int* maxUs_;
	float* lastOutDeg_;       // Actual output angle
	unsigned long* lastMicros_; // Timestamp per motor

	float deadbandDeg_ = 0.5f;
	float slewRateDegPerSec_ = -1.0f;

	// Private Utils
	bool isIndexValid_(int motorIndex) const;
	float clamp_(float deg, float min, float max) const;
	float applyDeadband_(const int motorIndex, const float targetDeg) const;
	float applySlew_(int motorIndex, float targetDeg, float dtSec) const;
	void  writeIfChanged_(int motorIndex, float targetDeg);
};
