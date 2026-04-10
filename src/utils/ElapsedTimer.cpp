#include "ElapsedTimer.h"
#include "Arduino.h"

ElapsedTimer::ElapsedTimer() {
    start();
}

void ElapsedTimer::start() {
    _started = millis();
}

unsigned long ElapsedTimer::elapsed() {
    return millis() - _started;
}

unsigned long ElapsedTimer::seconds() {
    return elapsed() / 1000;
}

unsigned long ElapsedTimer::minutes() {
    return seconds() / 60;
}
