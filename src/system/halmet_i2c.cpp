#include "halmet_i2c.h"
#include "halmet_const.h"

namespace halmet {

    TwoWire* HalmetI2C::instance_ = nullptr;

    TwoWire* HalmetI2C::instance() {
        if (instance_ == nullptr) {
            instance_ = new TwoWire(0);
            instance_->begin(kSDAPin, kSCLPin);
        }
        return instance_;
    }
}
