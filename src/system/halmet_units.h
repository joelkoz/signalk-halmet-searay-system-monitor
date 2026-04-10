#ifndef HALMET_UNITS_H_
#define HALMET_UNITS_H_

#include <Arduino.h>

namespace halmet {

    class SignalKUnit {

        public:
            static String toUnitName(const String& unit);

            static String toPhysicalQuantity(const String& unit);
    };


    extern float fToK(float f);
    extern float kToF(float k);

    extern const float psiToPa;
    extern const float paToPsi;

}

#endif /* HALMET_UNITS_H_ */
