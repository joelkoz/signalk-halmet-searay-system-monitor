#ifndef HALMET_I2C_H_
#define HALMET_I2C_H_

#include <Arduino.h>
#include <Wire.h>

namespace halmet {

    /**
     * Singleton class for the I2C bus on the Halmet board
     */
    class HalmetI2C {
        public:
            /**
             * Returns the singleton instance of the I2C bus on the Halmet board
             */
            static TwoWire* instance();

        private:
            static TwoWire* instance_;
    };
}

#endif /* HALMET_I2C_H_ */