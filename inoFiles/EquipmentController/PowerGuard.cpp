#include "PowerGuard.h"

PowerGuard::PowerGuard()
    : pin(-1), initialized(false), powerOn(false),
    activationThreshold(5), currentActivationCount(0),
    timeoutDuration(5000), lastUpdateTime(0) {}

void PowerGuard::begin(int controlPin) {
    pin = controlPin;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);  // Ensure relay is initially off
    initialized = true;
    powerOn = false;
    lastUpdateTime = millis();
}

void PowerGuard::ping() {
    if (!initialized) return;

    lastUpdateTime = millis();

    if (!powerOn) {
        currentActivationCount++;
        if (currentActivationCount >= activationThreshold) {
            turnOnPower();
        }
    }
}

void PowerGuard::tick() {
    if (!initialized || !powerOn) return;

    unsigned long now = millis();
    if (now - lastUpdateTime > timeoutDuration) {
        turnOffPower();
    }
}

void PowerGuard::setTimeout(unsigned long timeoutMs) {
    timeoutDuration = timeoutMs;
}

void PowerGuard::setActivationCount(unsigned int count) {
    activationThreshold = count;
}

void PowerGuard::forceShutdown() {
    if (powerOn) {
        turnOffPower();
    }
}

bool PowerGuard::isPowerOn() const {
    return powerOn;
}

int PowerGuard::getCurrentActivationCount() const {
    return currentActivationCount;
}

void PowerGuard::turnOnPower() {
    digitalWrite(pin, HIGH);
    powerOn = true;
}

void PowerGuard::turnOffPower() {
    digitalWrite(pin, LOW);
    powerOn = false;
    currentActivationCount = 0;  // Reset activation counter
}
