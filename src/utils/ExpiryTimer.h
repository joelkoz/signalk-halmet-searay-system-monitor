#pragma once

/**
 * @brief A class for simple timing. Call start() with the number of milliseconds
 *   to wait before expired() will return true.
 */
class ExpiryTimer {

    public:
        ExpiryTimer();
        
        void start(unsigned long expires);

        bool expired();
        
        long remaining(); // milliseconds until expired. If negative, milliseconds since expired
    private:
       unsigned long _started;
       unsigned long _expires;

};


// A macro that allows logging to be throttled
// easily by setting a timer.

#define _ONCE_EVERY_IMPL(MS, CODE, LINE)           \
    do {                                           \
        static ExpiryTimer _timer##LINE;           \
        if (_timer##LINE.expired()) {              \
            CODE;                                  \
            _timer##LINE.start(MS);                \
        }                                          \
    } while (0)

#define ONCE_EVERY(MS, CODE) \
    _ONCE_EVERY_IMPL(MS, CODE, __LINE__)

// Usage:
//ONCE_EVERY(2000, {
//    statLog.warn("Some warning msg: %s", strWarning);
//});
