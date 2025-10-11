#ifndef POWER_GUARD_H
#define POWER_GUARD_H

#include <Arduino.h>

/* [Sample.ino]

#include "PowerGuard.h"
PowerGuard guard;
void setup() {
    Serial.begin(9600);
    guard.begin(7);              // Set relay control pin
    guard.setActivationCount(5); // Require 5 pings to activate power
    guard.setTimeout(1000);     // Shutdown if no ping for 1 second
}

void loop() {
    guard.tick();  // Always call to monitor timeout
    if (Serial.available()) {
        Serial.read();  // Handle incoming data as needed
        guard.ping();   // Notify the guard of communication
    }
}
*/

class PowerGuard {
public:
    PowerGuard();

    // Initialize with the relay control pin
    void begin(int controlPin);

    // Call this whenever serial communication is received
    void ping();

    // Call this continuously in loop() to monitor timeout
    void tick();

    // Configuration setters
    void setTimeout(unsigned long timeoutMs);        // Set timeout duration in milliseconds
    void setActivationCount(unsigned int count);     // Set required ping count to activate power

    // Manually force power shutdown
    void forceShutdown();

    // Get current power status
    bool isPowerOn() const;

    // Get  current activation count
    int getCurrentActivationCount() const ;

private:
    int pin;
    bool initialized;
    bool powerOn;

    unsigned int activationThreshold;
    unsigned int currentActivationCount;

    unsigned long timeoutDuration;
    unsigned long lastUpdateTime;

    void turnOnPower();
    void turnOffPower();
};

#endif
