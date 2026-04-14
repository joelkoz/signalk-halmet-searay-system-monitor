# Marine Boat Systems Monitoring Device

Firmware for a Hatlabs Halmet board that replaces the obsolete "SeaRay Systems Monitor" on
a large SeaRay yacht with an custom monitoring system that monitors the same signal lines
from the "Systems Monitor" wiring harness and reports the health of key devices via SignalK.

## Overview

This device monitors boat component signal lines that are not monitored elsewhere and
reports the health of that device via SignalK.

### Devices Monitored
- Interior Emergency Bilge Pump (interior of boat near shower sump)
- Forward Bilge Pump (in engine room)
- Forward Emergency Bilge Pump (next to forward pump, higher up)
- Aft Bilge Pump (in lazarette)
- Aft Emergency Bilge Pump (next to aft bilge pump)
- Bilge Heat Detector (tied to engine room fire supression system)
- Port Ignition
- Starboard Ignition

#### Notes
Port and Starboard ignitiion signals indicate only that the engine switch is activated to
allow the engine to run. These are here simply to report the status to signal k, but they
unto themselves do not indicate a problem.

## Hardware

### Digital Inputs (DI)

- DI1 → Pin 6 → Forward Emergency Bilge Pump
- DI2 → Pin 7 → Interior Emergency Bilge Pump
- DI3 → Pin 8 → Aft Emergency Bilge Pump
- DI4 → Pin 9 → Forward Bilge Pump

### Analog Inputs (AI)

- AI1 → Pin 10 → Aft Bilge Pump
- AI2 → Pin 1 → Bilge Heat Detector
- AI3 → Pin 26 → Port Ignition
- AI4 → Pin 36 → Starboard Ignition

### Signal Interpretation

#### Digital Inputs
- HIGH = Active
- LOW = Inactive

#### Analog Inputs (treated as digital via threshold)

- Voltage > 8.0V → Active
- Voltage < 2.0V → Inactive
- Voltage between 2.0V and 8.0V → Indeterminate (hold previous state)

### Notes

- All signals are referenced to system ground.
- Pump signals indicate active operation when HIGH.
- Ignition signals indicate engine running when HIGH.
- Bilge heat detector indicates alarm condition when ACTIVE.

## Problem Being Solved

Most of the components monitored by the old SeaRay Systems Monitor device are available in much more detail over the boats upgraded NMEA 2000 network installed after the boat was
manufacdtured.  The bilge pumps are the remaining systems that do not have alternative
monitoring. SeaRay Systems Monitor display panels are no longer available, and
on my boat, the display no longer functions, thus it no longer is monitoring these
last few systems.  Reporting status and health of the these signals is the missing
piece to retiring the old system monitor altogther and removing it from the boat.
## Local secrets setup

This project expects boat-specific credentials in a local header that is not
committed to git.

Before the first build:

1. Copy `include/device_config.example.h` to `include/device_config.h`
2. Edit the values in `include/device_config.h` for your boat
3. Build the firmware

If `include/device_config.h` is missing, the build will stop with a friendly
compiler message that explains what to do.
