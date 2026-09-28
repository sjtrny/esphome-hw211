# HW211 ESPHome

[![ESPHome compatibility](https://github.com/sjtrny/esphome-hw211/actions/workflows/esphome-compatibility.yml/badge.svg)](https://github.com/sjtrny/esphome-hw211/actions/workflows/esphome-compatibility.yml)

ESPHome external component for reading HW211-family heat pump controllers locally over Modbus RTU.

Based on the work of others compiled here https://community.home-assistant.io/t/implementation-of-aqua-temp-controller/230400/.

## Potentially Supported Devices

This project is expected to be useful for devices that use the HW211-family controller, the Aqua Temp/HiTemp/Handy Heat Pump app ecosystem, or the same DTU/WiFi Modbus map. However compatibility is not guaranteed.

Known or likely candidates from community reports:

- EvoHeat EVO270 heat pump hot water systems.
- iStore heat pump hot water systems.

## Example Usage

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/sjtrny/esphome-hw211
    components: [hw211]

uart:
  id: rs485
  tx_pin: GPIO17 # Your device's TX pin
  rx_pin: GPIO18 # Your device's RX pin
  baud_rate: 9600
  data_bits: 8
  parity: NONE
  stop_bits: 1

modbus:
  uart_id: rs485
  send_wait_time: 250ms
  turnaround_time: 100ms

modbus_controller:
  - id: hw211_modbus
    address: 99
    update_interval: 30s

hw211:
  modbus_controller_id: hw211_modbus
  create_schedule: true
  create_controls: false
  create_binary_sensors: false
```

## Hardware

You will need an RS485 transceiver connected to your ESP32. Keep in mind that power from the HW211 is 12V so you will likely need to step down the voltage if connecting to an ESP32.
 
You can make your life easier by using a device that can accept 12V and has an isolated RS485 transceiver. Examples are:

- Waveshare ESP32-S3-RS485-CAN
- M5 Atom and the RS485 addon device

## Wiring

| HW211 connector | ESP32 Module or Device |
| --- | --- |
| Black `GND` | `GND` |
| Red `12V` | Do not connect directly to the ESP32 board |
| White `485-A` | GPIOXX |
| Yellow `485-B` | GPIOXY |

where `GPIOXX` and `GPIOXY` are GPIOs that can be used for UART.

## Component Options

```yaml
hw211:
  modbus_controller_id: hw211_modbus
  name_prefix: "HW211"
  sheet: dtu_wifi
  create_sensors: true
  create_operating_state: true
  create_schedule: false
  create_controls: false
  create_binary_sensors: true
  raw_registers: false
  force_update: false
```

- `modbus_controller_id`: Required. The ESPHome `modbus_controller` to attach generated entities to.
- `name_prefix`: Prefix used for generated entity IDs.
- `sheet`: Register sheet to use. Supported values are `dtu_wifi` and `hw211`.
- `create_sensors`: Create readable sensor entities.
- `create_operating_state`: Create the read-only `Operating state` text sensor (default: `true`). It also works when the other sensor/control creation flags are off.
- `create_schedule`: Create native time controls and enable switches for the two daily timer periods.
- `schedule_clock`: Optional local-time correction for timers. Requires
  `create_schedule: true`, a `time_id`, and the controller clock's fixed
  `controller_utc_offset` in signed `HH:MM` format. See below.
- `create_controls`: Create writable number/select/switch entities. Keep this off until you are comfortable writing to the controller.
- `create_binary_sensors`: Create binary sensors for readable bitfields.
- `raw_registers`: Also create disabled-by-default raw register sensors.
- `force_update`: Publish sensor updates in home assistant even when the value has not changed.

Start read-only. Modbus writes can change water heater operating mode, targets, schedules, offsets, and safety-related parameters. Enable `create_schedule` or `create_controls` only after confirming your model, controller, register map, and wiring.

## Entity Names and Home Assistant Groups

The component supplies readable names for both protocol sheets, including binary
status and fault sensors. For example, `bottom temperature` becomes
`Tank temperature lower`, `Setpoint of booster` becomes
`Electric heater target temperature`, and `Duration of defrosting` becomes
`Defrost interval`. Related settings share prefixes such as `Defrost`,
`Disinfection`, `Electric heater`, `Expansion valve`, and `Solar pump`.

Home Assistant uses the component's default entity categories to separate:

- **Sensors:** operating state, tank/display water temperatures, ambient and
  coil temperatures, and the main water/cooling target temperatures.
- **Controls:** everyday writable controls, such as power, mode and target
  temperature, when enabled with `create_controls`.
- **Configuration:** the timer controls and writable engineering settings, when
  enabled with `create_schedule` or `create_controls`.
- **Diagnostics:** read-only engineering settings, firmware details, counters,
  input signals, faults, combined status flags, and raw registers. This also
  includes power and mode readbacks, electric-heater settings, ventilation
  mode, refrigerant suction temperature and solar sensor temperature.

With `create_controls: false`, engineering settings are read-only sensors and
belong in Diagnostics, not Configuration. These defaults work for all users;
no Home Assistant customization is required. Custom headings within one device
are not provided by ESPHome: its web-server sorting groups do not group entities
in Home Assistant. See [Home Assistant entity categories](https://developers.home-assistant.io/docs/core/entity/#registry-properties).

Existing ESPHome IDs and native API keys are retained so Home Assistant can
migrate the names without replacing existing entity IDs, history, or automation
references. User-set names in Home Assistant still take precedence. Fresh
installations receive entity IDs based on the new names.

After updating an existing device, reload its ESPHome integration entry in
Home Assistant if the old sections remain. Home Assistant can cache entity
categories across a firmware reconnect. Reloading refreshes the metadata; no
entity renaming or local category overrides are needed.

Only presentation metadata changes: register addresses, scaling, enum values,
bit polarity, and writes are unchanged. Display units are corrected for defrost
interval, electric-heater delay, expansion-valve positions, and run-time counters
where the protocol documents them. The [entity guide](docs/entities.md) lists the
register names and explains fields that need care when interpreting readings.

## Derived Operating State

`Operating state` describes current activity, not the selected operating mode
(Intelligent, Economic, Hybrid, etc.). It is enabled by default and appears with
the everyday sensors in Home Assistant. No local template or extra binary
sensor configuration is required. Set `create_operating_state: false` to omit it.

The first matching condition wins:

| State | Condition |
| --- | --- |
| `Unknown` | Source data is missing, invalid or stale, or the Modbus controller is offline |
| `Fault / protection` | The fault/protection word is nonzero |
| `Defrosting` | Function-status bit 2 is set |
| `Heating + boost` | Compressor and electric-heater output bits are both set |
| `Heating` | Compressor output bit 8 is set |
| `Electric heating` | Electric-heater output bit 9 is set |
| `Off` | No heating/defrost output is reported and power is 0 |
| `Idle` | Power is 1, with no heating, defrost or fault reported |

The inputs are the following existing registers:

| Input | `dtu_wifi` | `hw211` |
| --- | ---: | ---: |
| Power | 1011 | 1011 |
| Component status flags | 2050 | 2030 |
| Operating status flags | 2051 | 2031 |
| Fault flags | 2085 | 2060 |

The component tests individual bits, not equality with the complete status word.
It reuses existing numeric readers where possible and creates internal word
readers otherwise. It never writes to the heater. Each decision waits for a
fresh report of all four fields to reduce transient states between Modbus
responses. Missing data becomes `Unknown` after three controller polling
intervals (at least 30 seconds); an offline callback invalidates it immediately.
An ESPHome API disconnection still makes the HA entity unavailable as usual.

`Idle` means neither heat source is reported running: fans or pumps can still
operate. It does not distinguish target satisfied, hysteresis, start delay or a
timer restriction. `Heating` refers to the controller's compressor output in
hot-water operation, not an independent measurement of heat transfer. The
reversing valve alone is not treated as defrost. Temperatures, targets, timer
times and whole-hour counters are not used to infer activity.

## Daily Timer Schedule

Set `create_schedule: true` to create these entities without enabling the other writable controller parameters:

- `Timer 1 Enabled`, `Timer 1 Start`, and `Timer 1 Stop`.
- `Timer 2 Enabled`, `Timer 2 Start`, and `Timer 2 Stop`.

Home Assistant sorts the device page by name. These default names put each timer's enable switch first, then its start time, then its stop time. No local name overrides or custom dashboard are needed. Stop is the timer's end time.

Earlier versions called the stop controls `Timer 1 End` and `Timer 2 End`. Their ESPHome IDs and API keys are retained so Home Assistant can migrate the names while keeping existing entity IDs. A user-set name in Home Assistant takes precedence over the component's default; clear that name to use the new default. Updating the component does not change the stored times or enable flags.

The controller runs during each enabled interval and stops outside enabled intervals. Each enable switch controls the matching start and end events together. Timer settings are read from the controller before writes are accepted, and writes preserve the other timer's enable bits. Times have one-minute precision. By default they use the clock configured on the HW211 controller; the optional correction below presents local times instead. Neither mode synchronizes the controller's clock.

Configure the start and end time before enabling a timer.

### Controller clock offset and daylight saving

If the controller keeps a fixed clock while your local time observes daylight
saving, configure the controller clock's **absolute UTC/GMT offset**, not the
number of hours it is behind Home Assistant:

```yaml
time:
  - platform: homeassistant
    id: local_time
    timezone: Australia/Sydney  # Use your actual local time zone, including DST rules.

hw211:
  modbus_controller_id: hw211_modbus
  create_schedule: true
  schedule_clock:
    time_id: local_time
    controller_utc_offset: "+09:00"
```

This adds **Timer clock UTC offset** to Home Assistant's Configuration section.
Enter a signed value such as `+09:00`, `+09:30` or `-03:30` (range -14:00 to
+14:00). It is saved on the adapter; later UI changes take precedence over the
YAML initial value. Use an ESPHome time source with the correct local time
zone. The explicit zone above also works on older supported ESPHome releases.

Start and Stop remain **local wall times** in Home Assistant. Conversion is:

```text
controller timer = local timer + controller UTC offset - local UTC offset
```

The result wraps around midnight. For example, with a UTC+09:00 controller and
Sydney at UTC+10:00, 06:00 in HA writes 05:00 to the controller. When Sydney is
UTC+11:00, the same HA time writes 04:00. Readbacks are translated back to local
time. Editing the offset or a timer keeps the other local timer values intact.

On first enabling correction, the currently displayed timer numbers are kept
as the intended local times and the native registers are adjusted accordingly.
The adapter then checks for local UTC-offset changes every second and updates
the native timers automatically. All four endpoints are written together to
1134–1141; timer-enable bits and the controller clock are not changed.

Correction requires valid synchronized local time and fresh timer readbacks.
Its saved mapping and pending-write state survive restarts, so rebooting does
not apply the offset twice. Native-panel timer edits are read back through the
applied mapping. Writes are confirmed by subsequent register reads and retried
if needed. The device keeps its native schedule while the adapter is offline;
any correction is applied after the adapter recovers its time and readbacks.

This compensates a fixed offset, not clock drift or automatic clock changes
made by the controller itself. Times inside a skipped/repeated DST hour remain
ambiguous; ordinary times such as 06:00 are the intended use. Removing
`schedule_clock` returns the UI to native controller times without undoing
register values already written. Erasing adapter preferences also erases the
saved mapping, so review the timers when enabling correction again afterward.

## ESPHome Compatibility

The component supports the ESPHome 2026.4 API used by the original EvoHeat installation and the current ESPHome API.

Keep `send_wait_time: 250ms` and `turnaround_time: 100ms` explicit. ESPHome 2026.9 increased the Modbus client defaults to 2000ms and 600ms. An EvoHeat HW211 controller emits traffic about every 500ms, so the newer defaults can prevent the client from finding an idle window in which to send requests.

GitHub Actions checks name coverage, migration identities, operating-state logic and timer clock correction (including DST and restart recovery), then compiles a configuration with all entity types and timer correction enabled. Each change is checked against ESPHome 2026.4.5 and the tracked stable release. A weekly scheduled run also checks the latest stable release and ESPHome's development branch. Dependabot checks for stable ESPHome releases each day and opens a pull request that runs the same checks.

## Protocol

Info about the modbus registers is available in `protocol/`.

The script `extract_modbus_spec.py` generates `protocol/hw211_modbus.json` from `datasheets/EVO270-1_MODBUS communication_protocol_6.5.25.xlsx`.

The JSON file is used by the component to automatically generate necessary esphome sensors.

## LLM Disclaimer

Virtually all code here was written by Codex.
