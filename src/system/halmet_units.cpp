#include "halmet_units.h"
#include <cstring>

using namespace halmet;

// SignalK unit definitions from https://github.com/SignalK/specification/blob/master/schemas/definitions.json
struct UnitDefinition {
    const char* unit;
    const char* display;
    const char* quantity;
};

static const UnitDefinition signalKUnits[] = {
    {"s", "seconds", "time"},
    {"Hz", "hertz", "frequency"},
    {"m3", "cubic meters", "volume"},
    {"m3/s", "cubic meters per second", "flow"},
    {"kg/s", "kilograms per second", "mass flow rate"},
    {"kg/m3", "kilograms per cubic meter", "density"},
    {"deg", "degrees", "angle"},
    {"rad", "radians", "angle"},
    {"rad/s", "radians per second", "rotation"},
    {"A", "amperes", "current"},
    {"C", "coulombs", "charge"},
    {"V", "volts", "voltage"},
    {"W", "watts", "power"},
    {"Nm", "newton meters", "torque"},
    {"J", "joules", "energy"},
    {"ohm", "ohms", "resistance"},
    {"m", "meters", "distance"},
    {"m/s", "meters per second", "speed"},
    {"m2", "square meters", "area"},
    {"K", "kelvin", "temperature"},
    {"Pa", "pascals", "pressure"},
    {"kg", "kilograms", "mass"},
    {"ratio", "ratio", "ratio"},
    {"m/s2", "meters per second squared", "acceleration"},
    {"rad/s2", "radians per second squared", "angular acceleration"},
    {"N", "newtons", "force"},
    {"T", "tesla", "magnetic field"},
    {"Lux", "lux", "light intensity"},
    {"Pa/s", "pascals per second", "pressure rate"},
    {"Pa.s", "pascal seconds", "viscosity"}
};

static const int numUnits = sizeof(signalKUnits) / sizeof(signalKUnits[0]);

String SignalKUnit::toUnitName(const String& unit) {
    if (unit == nullptr) {
        return "";
    }
    
    for (int i = 0; i < numUnits; i++) {
        if (strcmp(signalKUnits[i].unit, unit.c_str()) == 0) {
            return signalKUnits[i].display;
        }
    }
    
    // If not found, return the original unit string
    return unit;
}

String SignalKUnit::toPhysicalQuantity(const String& unit) {
    if (unit == nullptr) {
        return "";
    }
    
    for (int i = 0; i < numUnits; i++) {
        if (strcmp(signalKUnits[i].unit, unit.c_str()) == 0) {
            return signalKUnits[i].quantity;
        }
    }
    
    // If not found, return nullptr
    return "invalid";
}

namespace halmet {
    float fToK(float f) {
        return (f + 459.67f) * (5.0f / 9.0f);
    }

    float kToF(float k) {
        return (k * 9.0f / 5.0f) - 459.67f;
    }

    const float psiToPa = 6894.757f;
    const float paToPsi = 1.0f / 6894.757f;
}
