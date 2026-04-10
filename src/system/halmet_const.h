#ifndef HALMET_SRC_HALMET_CONST_H_
#define HALMET_SRC_HALMET_CONST_H_

#include <Arduino.h>

namespace halmet {

// I2C pins on HALMET.
const int kSDAPin = 21;
const int kSCLPin = 22;

// ADS1115 I2C address
const int kADS1115Address = 0x4b;

// Scale to apply to Halmet analog inputs
const float kVoltageDividerScale = 33.3f / 3.3f;

// Current measurement when CCS jumpers are used on analog inputs
const float kConstantCurrentValue = 0.01f;

// CAN bus (NMEA 2000) pins on HALMET
const gpio_num_t kCANRxPin = GPIO_NUM_18;
const gpio_num_t kCANTxPin = GPIO_NUM_19;

// HALMET digital input pins
const int kDigitalInputPin1 = GPIO_NUM_23;
const int kDigitalInputPin2 = GPIO_NUM_25;
const int kDigitalInputPin3 = GPIO_NUM_27;
const int kDigitalInputPin4 = GPIO_NUM_26;

}  // namespace halmet

#endif /* HALMET_SRC_HALMET_CONST_H_ */
