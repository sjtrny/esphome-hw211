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
- `create_schedule`: Create native time controls and enable switches for the two daily timer periods.
- `create_controls`: Create writable number/select/switch entities. Keep this off until you are comfortable writing to the controller.
- `create_binary_sensors`: Create binary sensors for readable bitfields.
- `raw_registers`: Also create disabled-by-default raw register sensors.
- `force_update`: Publish sensor updates in home assistant even when the value has not changed.

Start read-only. Modbus writes can change water heater operating mode, targets, schedules, offsets, and safety-related parameters. Enable `create_schedule` or `create_controls` only after confirming your model, controller, register map, and wiring.

## Daily Timer Schedule

Set `create_schedule: true` to create these entities without enabling the other writable controller parameters:

- `Timer 1 Start`, `Timer 1 End`, and `Timer 1 Enabled`.
- `Timer 2 Start`, `Timer 2 End`, and `Timer 2 Enabled`.

The controller runs during each enabled interval and stops outside enabled intervals. Each enable switch controls the matching start and end events together. Timer settings are read from the controller before writes are accepted, and writes preserve the other timer's enable bits. Times have one-minute precision and use the clock configured on the HW211 controller; this component does not synchronize that clock.

Configure the start and end time before enabling a timer.

## ESPHome Compatibility

The component supports the ESPHome 2026.4 API used by the original EvoHeat installation and the current ESPHome API.

Keep `send_wait_time: 250ms` and `turnaround_time: 100ms` explicit. ESPHome 2026.9 increased the Modbus client defaults to 2000ms and 600ms. An EvoHeat HW211 controller emits traffic about every 500ms, so the newer defaults can prevent the client from finding an idle window in which to send requests.

GitHub Actions compiles a configuration that enables sensors, binary sensors, numbers, selects, and switches. Each change is checked against ESPHome 2026.4.5 and the tracked stable release. A weekly scheduled run also checks the latest stable release and ESPHome's development branch. Dependabot checks for stable ESPHome releases each day and opens a pull request that runs the same compile checks.

## Protocol

Info about the modbus registers is available in `protocol/`.

The script `extract_modbus_spec.py` generates `protocol/hw211_modbus.json` from `datasheets/EVO270-1_MODBUS communication_protocol_6.5.25.xlsx`.

The JSON file is used by the component to automatically generate necessary esphome sensors.

## LLM Disclaimer

Virtually all code here was written by Codex.
