#include "ExpiryTimer.h"
#include "Arduino.h"

ExpiryTimer::ExpiryTimer() {
    _expires = 0;
}

void ExpiryTimer::start(unsigned long expires) {
    _expires = millis() + expires;
}

bool ExpiryTimer::expired() {
    return millis() > _expires;
}

long ExpiryTimer::remaining() {
    return _expires - millis();
}
