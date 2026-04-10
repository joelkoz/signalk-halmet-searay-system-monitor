#pragma once

/**
 * @brief A class for simple timing. Call start() to start the timer, and
 *   elapsed() to see how many milliseconds has passed since started.
 */
class ElapsedTimer {

    public:
        ElapsedTimer();
        
        void start();

        unsigned long elapsed();

        unsigned long seconds();

        unsigned long minutes();
    private:
       unsigned long _started;
};
