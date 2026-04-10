#include "system/timer.h"

namespace app {

Timer::Timer(const String& config_path, unsigned long msDefaultInterval)
    : TimedProducer<unsigned long>([]() { return millis(); }, config_path,
                                   msDefaultInterval) {}

}  // namespace app
