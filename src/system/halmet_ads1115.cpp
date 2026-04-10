#include "halmet_ads1115.h"

#include "sensesp/system/local_debug.h"
#include "halmet_const.h"
#include "halmet_i2c.h"

namespace halmet {

    Adafruit_ADS1115* HalmetADS1115::instance_ = nullptr;

    Adafruit_ADS1115* HalmetADS1115::instance() {
        if (instance_ == nullptr) {
              // Initialize ADS1115
            instance_ = new Adafruit_ADS1115();

            instance_->setGain(kADS1115Gain);
            bool ads_initialized = instance_->begin(kADS1115Address, HalmetI2C::instance());
            debugD("ADS1115 initialized: %d", ads_initialized);
        }
        return instance_;
    }
}
