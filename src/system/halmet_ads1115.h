#ifndef HALMET_ADS1115_H_
#define HALMET_ADS1115_H_

#include <Adafruit_ADS1X15.h>

namespace halmet {

    // Set the ADS1115 GAIN to adjust the analog input voltage range.
    // On HALMET, this refers to the voltage range of the ADS1115 input
    // AFTER the 33.3/3.3 voltage divider.

    // GAIN_TWOTHIRDS: 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
    // GAIN_ONE:       1x gain   +/- 4.096V  1 bit = 2mV      0.125mV
    // GAIN_TWO:       2x gain   +/- 2.048V  1 bit = 1mV      0.0625mV
    // GAIN_FOUR:      4x gain   +/- 1.024V  1 bit = 0.5mV    0.03125mV
    // GAIN_EIGHT:     8x gain   +/- 0.512V  1 bit = 0.25mV   0.015625mV
    // GAIN_SIXTEEN:   16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV

    const adsGain_t kADS1115Gain = GAIN_ONE;

    // The "Constant Current" measurement current when CCS jumper is active is 10 mA.
    const float kCcsMeasurementCurrent = 0.01f;    

    /**
     * Singleton class for the ADS1115 ADC on the Halmet board
     */
    class HalmetADS1115 {
        public:
            /**
             * Returns the singleton instance of the ADS1115 ADCon the Halmet board
             */
            static Adafruit_ADS1115* instance();

        private:
            static Adafruit_ADS1115* instance_;
    };
}

#endif /* HALMET_ADS1115_H_ */
