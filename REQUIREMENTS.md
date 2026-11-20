# Marine Boat Systems Monitoring Device

## Functional Requirements

### 1. Pump running status reporting

For each pump, report to SignalK whether the pump is running.

Required behavior:

- send `true` when the pump is running
- send `false` when the pump is not running
- Each pump is reported indenpendently
- A different signal k path should be used for each pump.
- The running state should also be re-sent periodically even if it has not changed.
- This periodic state report interval should be configurable via the SensESP UI as one shared setting for both pumps.
- The default periodic state report interval should be 10 seconds.
- Each run cycle should be timed. 
- Report a "last cycle run time" and "last cycle idle time" to signal k each time a cycle status changes


### 2. Device heartbeat

The device should send a heartbeat signal to Signal K to indicate that it is alive and functioning. 

Behavior:
- Use the pre-existing `HeartbeatReporter` class found in the source tree
- The default heartbeat report interval should be 10 seconds.
- The hearbeat is for the device itself, not for any specific pump.

### 2a. Signal K connect and reconnect republish behavior

The device must explicitly republish Signal K outputs when the websocket connection
to the Signal K server becomes connected, including reconnects after a disconnect.

Purpose:
- Ensure startup values are not lost when a producer emits before the websocket is ready
- Ensure long-interval or change-only outputs are refreshed immediately after reconnect

Behavior:
- Use the websocket connection-state callback pattern from the reference project, `signalk-halmet-vacuflush` (one `onSKServerConnect()` per component, called from `main.cpp`)
- On each `kSKWSConnected` transition, republish the current value of every Signal K output so startup and reconnect do not miss them. An output behind a `RepeatMutable` must be re-sent through the repeater (`RepeatMutable::resend()`): it drops unchanged values, so notifying its source sends nothing.
- SensESP queues at most 20 outputs between sends (one send every 100 ms) and drops the oldest beyond that, so the connect re-send must not emit more than that at once. The re-send runs in steps, `kResendStepSpacingMs` apart: the device-level outputs first, then one pump per step. Within a step, outputs behind a repeater go first and outputs with no repeater go last, so an overflow drops only values that come back on their next repeat.
- Outputs that already publish frequently may still be republished on connect; no additional
  code is required to suppress that behavior
- For this app, this reconnect republish behavior should cover heartbeat, pump states,
  pump health notifications, fire notifications, cycle statistics, last-cycle values,
  ignition states, and incident alerts that are still active
- A resolved incident alert is not re-sent on reconnect:
  re-sending its "now resolved" warning would raise it again after every server
  restart. It stays in Signal K until it is acknowledged (see Incident alerts)

### 3. Pump Health

For each pump, a SignalK notification should be published periodically to indicate if
a pump is healthy (i.e. the "Normal" state) or if it is in an "Alert" condition.
There are two general classes of bilge pump and each has slightly different health logic:

o Primary pump (e.g. forward, aft)
  - It is normal to run occasionally to clear out rain water and HVAC condensate
  - It is NOT normal for the pump to have never run for an extended period of time

o Emergency pump (e.g. forward emergency, aft emergency)
  - It is normal for this pump to never run
  - It is NOT normal for the pump to be running. This would indicate the
    primary pump is unable to keep up with the load, or it has failed altogether
  - The emergency pumps should be exercised on a test basis periodically. A
    "warning" alert for an emergency pump that has not run at least once in
    the last 60 days (configurable) should prompt the user to test its functionality
  - A run shorter than the emergency alarm delay (configurable 0-30 seconds,
    default 3) is a test, not an emergency: it raises no "running" alarm, neither on
    the health notification nor as an incident alert, but it still counts as a cycle
    and as exercising the pump. The delay is measured from the start of the run to
    when the run input last went inactive; the min stop time spent confirming the
    stop does not count.
  - Only an isolated short run is a test. A run that starts within 24 hours of the
    previous run raises the alarm at once, whatever its length: an emergency pump
    making repeated short runs is keeping up with water. Real tests are weeks apart,
    so the only cost is that a second test of the same pump within a day alarms.
    The time of the previous run is persisted, so the window survives a device
    restart once the clock has been set from Signal K.


#### Special note regarding SignalK "notifications"
In the example app, notifications are either sent when they are in an alert condition, or they are cleared entirely. This system is different: a notification status is ALWAYS sent. It is either "normal" or in a "warning" or "alert" condition (refer to the SignalK docs for general notification information).


### 4. Configurable max run time

The device must support a maximum continuous run time for each pump.

Purpose:
- Detect failure in the float switch (e.g. stuck float)
- Prevent overheating and damage to the pump

Behavior:
- if a pump has been detected as running longer than the max run time, the pump
goes into an alert state.


### 5. Configurable cycle min stop time

The device must support a minimum stop time used to determine when a pump cycle has truly completed.

Purpose:
- avoid treating short signal drops or brief pauses as a completed stop/start cycle
- debounce the logical transition from running to stopped

Behavior:
- a pump should only be considered stopped after it has been idle for at least the minimum stop time


### 5. Idle time tracking, Configurable min idle time / short cycle detection
The device must track an idle time between pump cycles and track short cycles 

Purpose:
- Track how long a pump has been idle
- Support the max consecutive short cycles feature

Behavor:
- Each pump gets its own idle timer
- When a pump cycle is completed (stopped for at least the minimum stop time), the idle timer starts
- If a new cycle starts before "min idle time", the short cycle count is incremented
- If the idle timer exceeds "min idle time", the short cycle count is reset to zero
- When a new cycle **starts**, report the "last idle time" to Signal K

### 5a. Cycle statistics and counts
The device must track additional cycle statistics for each pump.

Behavior:
- Track a daily completed cycle count for each pump
- Reset the daily cycle count on the midnight boundary using the device wall clock when valid
- Track the current consecutive short cycle count for each pump
- Track the average completed cycle run time since boot for each pump
- Track the average idle time since boot for each pump
- Track a daily cycle count for each pump
- Track a total cycle count for each pump
- Do not reset the averages daily
- Do not reset the total cycle count automatically
- Report these values to Signal K on boot, whenever they change, and at minimum once every configured aggregate report interval
- The aggregate report interval should be configurable in seconds via the SensESP UI as one shared setting for all pumps
- The default aggregate report interval should be 30 seconds
- These values should each use their own configurable Signal K path
- The daily cycle count and total cycle count should persist across reboot


### 6. Configurable Max consecutive short cycles
The device should alert if a pump it has too many consecutive short cycles

Purpose:
- Consistent short cycles is indicative of a continuous leak
- Alert the user of possible system issues

Behavior:
- If a pump's short cycle count exceeds this max consecutive short cycles value, the pump enters an alert state


## Behavioral Notes

### Pump state determination

Pump state is based on the corresponding digital and analog inputs:

#### Digital Inputs (DI)

- DI1 → Pin 6 → Forward Emergency Bilge Pump
- DI2 → Pin 7 → Interior Emergency Bilge Pump
- DI3 → Pin 8 → Aft Emergency Bilge Pump
- DI4 → Pin 9 → Forward Bilge Pump

#### Analog Inputs (AI)

- AI1 → Pin 10 → Aft Bilge Pump
- AI2 → Pin 1 → Bilge Heat Detector
- AI3 → Pin 26 → Port Ignition
- AI4 → Pin 36 → Starboard Ignition

#### Signal Interpretation

##### Digital Inputs
- HIGH = Active
- LOW = Inactive

##### Analog Inputs (treated as digital via threshold)

- Voltage > 8.0V → Active
- Voltage < 2.0V → Inactive
- Voltage between 2.0V and 8.0V → Indeterminate (hold previous state)


## Configuration via SensESP UI

- Any item listed as "configurable" should be configurable via the SensESP UI.
- Use the configuration system native to SensESP.
- Prefer simpler code over runtime reconfiguration for items that are changed infrequently.
- Unless a requirement explicitly calls out live runtime application of a changed setting, a configurable item may be treated as constant during runtime and applied on next reboot.
- It is acceptable for a user to reboot the device after changing such settings.

## SensESP Design Guidance

The project should prefer standard SensESP producer / transform / consumer patterns over ad-hoc polling and direct reporting logic.

- A sensor or input should normally be represented by a SensESP producer such as `DigitalInputState` or `RepeatSensor`.
- A value that is computed or tracked internally should normally be represented by a `ValueProducer` / `ObservableValue` rather than a plain member variable when other code may want to subscribe to it.
- Repeated reporting to Signal K should be implemented using a reusable repeat-style transform or producer, rather than manual "last reported" bookkeeping.
- Logic that transforms one value into another should prefer a SensESP transform, including custom transforms when the built-in ones are not sufficient.
- Driving an output pin from another state should prefer normal SensESP wiring, for example producer -> transform -> `DigitalOutput`.
- Signal K output classes should generally be consumers connected in `main.cpp` or in a component `begin()` convenience method, rather than being tightly coupled to the internal sensing logic.
- Metadata should be attached to all custom Signal K paths. Numeric values should specify units. Writable paths should declare `supportsPut` when appropriate.
- Exception, until SensESP can send metadata in more than one delta: notification paths carry no metadata, and value-path metadata is limited to units and a display name. SensESP sends the metadata of every output in a single delta on each connect; with five pumps the full set (13.9 KB) exceeded the websocket send buffer, so it was dropped on every connect, and building it nearly exhausted the heap (the device rebooted with a panic on a reconnect). The first delta after a connect carries the metadata plus up to 20 queued values, so keep metadata + 20 × the largest value payload (a resolved incident alert is about 240 B) well under `SENSESP_SK_WS_BUFFER_SIZE` when adding outputs. The trimmed metadata is about 6.1 KB.

For "complex components" such as the pump monitor components:

- It is acceptable for a class to encapsulate multiple internal SensESP objects and the logic that ties them together.
- Such a class should expose producer-style accessors for its important outputs so other code can subscribe in multiple places.
- Internal logic should update producers via `set()` / `emit()` so all subscribed consumers are notified through the normal SensESP chain.
- Convenience wiring to default `SKOutput*` consumers may be done in the component's `begin()` method, but the underlying values should still be available as producers.

Specific implementation notes discovered during planning and refactoring:

- `DigitalInputState` is a good fit for pump run input, but the externally reported "running" state is a logical state, not always the raw GPIO state. The raw input should be kept separate from the debounced / qualified logical running state.
- The stop detection for this project is asymmetric: a pump should only become logically stopped after `min_stop_time`, so this should not be replaced with a plain symmetric debounce transform.
- The running state needs periodic re-reporting to Signal K using one shared configurable interval for all pumps.
- Outputs that are not otherwise expected to publish quickly should also be explicitly re-notified on websocket connect/reconnect so startup or reconnect does not miss their latest value.
- Default Signal K paths for pump-specific values should be derived from the pump role (e.g. `forward` or `forward.emercency`) rather than passed around as many unrelated constants.
- Shared or reusable SensESP enhancements are preferred over one-off workarounds, but avoid adding runtime-mutable infrastructure unless the requirements specifically need live reconfiguration without reboot.

## Code Documentation Guidance

- New class headers should include concise Doxygen-style comments.
- At minimum, document each new class with a brief `@brief` description and document public constructors and public methods with parameter and return details when they are not obvious from the signature alone.
- Private helper methods and important member fields in new class headers should also receive short Doxygen comments when they are part of non-trivial logic or lifecycle behavior.
- Keep these comments short and practical; they should help future maintainers understand intent and usage without turning the header into prose.

## Signal K Output Paths
- All Signal K output paths should have defaults consistent with the Signal K spec
- All paths should be configurable via the SensESP UI.
- Suggested default pump statistic paths are:
- `electrical.pumps.bilge.<role>.dailyCycleCount`
- `electrical.pumps.bilge.<role>.shortCycleCount`
- `electrical.pumps.bilge.<role>.averageCycleRunTime`
- `electrical.pumps.bilge.<role>.averageCycleIdleTime`


## Signal K Notification Paths (Alarms & Faults)
While it is normally idiomatic in Signal K to use distinct paths for different fault types to allow for granular alerting and metadata, this app uses a single notification
***per pump** to indicate its overall health.


### Delta Update Implementation
When a fault is detected, the device should emit a JSON delta.
Example: Fail-to-Stop (Continuous Run) Alarm

{
  "updates": [{
    "values": [{
      "path": "notifications.bilge.pumps.forward.health",
      "value": {
        "state": "alarm",
        "message": "Pump exceeded max runtime (Possible Major Leak)",
        "method": ["visual", "sound"]
      }
    }]
  }]
}

### Auto-Clear Logic
To clear an alarm once the condition is resolved, the device should resend the notification on the same path with its `state` set to `normal`.

Example clear payload:

{
  "updates": [{
    "values": [{
      "path": "notifications.bilge.pumps.forward.health",
      "value": {
        "state": "normal",
        "message": "Pump health is normal"
      }
    }]
  }]
}

Every health notification message begins with the pump's name (for example
"Aft Emergency Bilge Pump: Emergency bilge pump has not been exercised recently"),
because a notification forwarded on its own, such as a push message, does not
carry its path.

### Incident alerts
Besides its health notification, each pump has incident alerts under
`notifications.bilge.pumps.<role>.health.alert.<alert>` (`emergencyRunning`,
`maxRunTime`, `shortCycles`), so an incident stays visible after the pump stops.

- While the condition holds, the alert is raised (`alarm`).
- When it ends, the alert changes to a `warn` whose message says when the incident
  started and that it is now resolved.
- The resolved warning returns to `normal` once it has been acknowledged in Signal K.
  The device watches its incident alert paths and acts on the server's
  `status.acknowledged` flag. An alert acknowledged while it was still active
  returns to `normal` as soon as it is resolved. Signal K clears the flag whenever
  an alert's severity rises, so each new incident has to be acknowledged again.
- Only a `warn` (a resolved incident) is ever returned to `normal` this way, never an
  `alarm`: after a reboot an acknowledged alarm may be an incident that is still
  going on but not yet detected again.
- The server sends the current value of these paths on every (re)connect, so an
  acknowledged resolved warning left over from before a reboot is also returned to
  `normal`.
- On a server whose notifications carry no `status`, the resolved warning stays
  until the server restarts.


## Local Reference Patterns
The project should use the reference project [`signalk-halmet-vacuflush`](https://github.com/joelkoz/signalk-halmet-vacuflush) as the primary reference for code structure, startup flow, and organization. 

As a secondary source of SensESP patterns, refer to the SensESP source and examples the build actually uses, in `.pio/libdeps/halmet/SensESP` (`SignalK/SensESP` pinned in `platformio.ini`).

## Signal K Source Of Truth
- The historical Signal K specification repository is useful for general background, terminology, and older examples.
- For current 2.x behavior, metadata conventions, and implementation reality, the Signal K Server source code is the authoritative source of truth.
- When using DeepWiki or other code-aware references, prefer `SignalK/signalk-server` over `SignalK/specification`.

## Initial source tree
- `src/utils` contains timer utilities, including ElapsedTimer, ExpiryTimer, and the ONCE_EVERY macro
- `src/system` contains halmet-specific helper functions and classes

## Debug Logging
- When outputting status, error, or debug messages, use the macros found in `src/Debug.h`
- The `LOG_*` macros use ESP-IDF logging, which SensESP mirrors to the serial port and the device's web log (`/api/log`). There is no RemoteDebug/telnet.

## Device identity and networking
- The default device host name is `sk-monitor` (`DEVICE_HOSTNAME` in `.env`)
- WiFi credentials and the Signal K server come from `.env`, with the same settings and
  mechanism as the reference project.

## OTA Updating
- Over-the-air updates use the same mechanism as the reference project (espota, via the
  `halmet_ota` environment).

## Notes
This is a boat-installed embedded device, so priorities are:
- reliability
- predictable startup
- simple behavior
- clear fault handling
- easy bench testing before installation
