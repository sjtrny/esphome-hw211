from __future__ import annotations

import json
import re
from pathlib import Path

import esphome.codegen as cg
from esphome.components import binary_sensor, number, select, sensor, switch
from esphome.components.modbus.helpers import ModbusRegisterType, SENSOR_VALUE_TYPE
from esphome.components.modbus_controller import ModbusController, SensorItem
from esphome.components.modbus_controller.binary_sensor import ModbusBinarySensor
from esphome.components.modbus_controller.const import (
    CONF_BITMASK,
    CONF_FORCE_NEW_RANGE,
    CONF_MODBUS_CONTROLLER_ID,
    CONF_REGISTER_TYPE,
    CONF_SKIP_UPDATES,
    CONF_USE_WRITE_MULTIPLE,
    CONF_VALUE_TYPE,
    CONF_WRITE_LAMBDA,
)
from esphome.components.modbus_controller.number import ModbusNumber
from esphome.components.modbus_controller.select import ModbusSelect
from esphome.components.modbus_controller.sensor import ModbusSensor
from esphome.components.modbus_controller.switch import ModbusSwitch
import esphome.config_validation as cv
from esphome.core import Lambda
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_ADDRESS,
    CONF_DEVICE_CLASS,
    CONF_DISABLED_BY_DEFAULT,
    CONF_ENTITY_CATEGORY,
    CONF_FORCE_UPDATE,
    CONF_ICON,
    CONF_ID,
    CONF_INTERNAL,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_NAME,
    CONF_OPTIMISTIC,
    CONF_STATE_CLASS,
    CONF_STEP,
    CONF_UNIT_OF_MEASUREMENT,
)


DEPENDENCIES = ["modbus_controller"]
AUTO_LOAD = ["binary_sensor", "number", "select", "sensor", "switch"]
CODEOWNERS = ["@local"]

CONF_INCLUDE_RESERVED = "include_reserved"
CONF_NAME_PREFIX = "name_prefix"
CONF_RAW_REGISTERS = "raw_registers"
CONF_SHEET = "sheet"
CONF_CREATE_CONTROLS = "create_controls"
CONF_CREATE_SENSORS = "create_sensors"
CONF_CREATE_BINARY_SENSORS = "create_binary_sensors"

REGISTER_TYPE_HOLDING = ModbusRegisterType.HOLDING
VALUE_TYPE_U_WORD = SENSOR_VALUE_TYPE["U_WORD"]


def _load_registers(sheet: str) -> list[dict]:
    root = Path(__file__).resolve().parents[2]
    spec = json.loads((root / "protocol" / "hw211_modbus.json").read_text())
    return spec["sheets"][sheet]["registers"]


def _slug(value: str) -> str:
    value = value.lower().replace("/", " ")
    value = re.sub(r"[^a-z0-9]+", "_", value)
    return re.sub(r"_+", "_", value).strip("_") or "register"


def _name(register: dict) -> str:
    name = register["name"] or register["number"] or f"Register {register['address']}"
    return name.replace("/", " or ")


def _entity_id(prefix: str, register: dict, suffix: str):
    return f"{_slug(prefix)}_{register['address']}_{_slug(_name(register))}_{suffix}"


def _entity_name(config: dict, register: dict, label: str | None = None) -> str:
    name = (label or _name(register)).replace("_", " ")
    name = name[:1].upper() + name[1:]
    if not label and _slug(name) in config.get("_hw211_duplicate_names", set()):
        return f"{name} {register['address']}"
    return name


def _has_register_metadata(register: dict) -> bool:
    return bool(
        register["number"]
        or register["name"]
        or register["byte_length"]
        or register["access"]
        or register["data_type"]
    )


def _is_writable(register: dict) -> bool:
    access = register["access"].upper()
    return "W/R" in access or "读/写" in access


def _is_readable(register: dict) -> bool:
    access = register["access"].upper()
    if "R" in access or "读" in access:
        return True
    # The 2000-series telemetry block is marked as function 16/write in the
    # workbook because the original WiFi module receives uploads, but community
    # ESPHome configs read it successfully as holding registers from slave 99.
    return register["address"] >= 2000 and bool(register["name"])


def _is_binary_type(register: dict) -> bool:
    return register["data_type"].lower() == "binary"


def _scale_expr(data_type: str) -> tuple[str, str | None]:
    if data_type == "TEMP":
        return "return x == 32767 ? NAN : x * 0.1f;", "return x * 10.0f;"
    if data_type == "TEMP1":
        return "return (x - 60.0f) * 0.5f;", "return (x * 2.0f) + 60.0f;"
    multipliers = {
        "DIGI2": 10.0,
        "DIGI3": 100.0,
        "DIGI4": 5.0,
        "DIGI5": 0.1,
        "DIGI6": 0.001,
        "DIGI7": 0.5,
        "DIGI8": 2.0,
        "DIGI9": 0.01,
    }
    multiplier = multipliers.get(data_type)
    if multiplier is None:
        return "return x;", None
    return f"return x * {multiplier}f;", f"return x / {multiplier}f;"


def _range(register: dict) -> tuple[float, float] | None:
    text = register["description"]
    match = re.search(r"(-?\d+(?:\.\d+)?)\s*[~～]\s*(-?\d+(?:\.\d+)?)", text)
    if not match:
        return None
    return float(match.group(1)), float(match.group(2))


def _step(data_type: str) -> float:
    return {
        "TEMP": 0.1,
        "TEMP1": 0.5,
        "DIGI5": 0.1,
        "DIGI6": 0.001,
        "DIGI7": 0.5,
        "DIGI9": 0.01,
    }.get(data_type, 1.0)


def _unit(register: dict) -> str | None:
    data_type = register["data_type"]
    text = register["description"].lower()
    if data_type in ("TEMP", "TEMP1") or "℃" in register["description"]:
        return "°C"
    if "min" in text or "minute" in text:
        return "min"
    if "hour" in text or re.search(r"\bh\b", text):
        return "h"
    if "days" in text or "day" in text:
        return "d"
    return None


ENUM_OPTIONS = {
    1011: {"Off": 0, "On": 1},
    1012: {
        "Intelligent Mode": 0,
        "Economic Mode": 2,
        "Hybrid Mode": 3,
        "High Demand Mode": 4,
        "Vacation Mode": 7,
    },
    1013: {
        "Intelligent Mode": 0,
        "Economic Mode": 2,
        "Hybrid Mode": 3,
        "High Demand Mode": 4,
        "Vacation Mode": 7,
    },
    1014: {"Complete Instructions": 0, "On": 1, "Off": 2},
    1015: {"Off": 0, "On": 1},
    1016: {"Off": 0, "Low Speed": 1, "High Speed": 2},
    1020: {"Low Speed Air": 0, "Solar Heat Pump": 2},
    1021: {"No Output": 0, "Solar Heat Pump": 2, "Solar Drain Valve": 3},
    1039: {"Standard": 0, "Eco": 1, "Intelligent": 2},
    1051: {"Non Auto": 0, "Auto": 1},
    1067: {"No": 0, "Yes": 1},
    1069: {"Air Source": 0},
    1073: {"Celsius": 0, "Fahrenheit": 1},
    1074: {"0x07": 0, "0x17": 1, "0x08": 2, "0x18": 3},
    1075: {"No": 0, "Yes": 1},
    1077: {"Centralized Control": 0, "DTU/WiFi": 1},
    1080: {"Bottom": 0, "Top": 1},
    1083: {"No": 0, "Yes": 1},
    1090: {"No": 0, "Yes": 1},
    1107: {"No": 0, "Yes": 1},
    1110: {"No": 0, "Yes": 1},
    1120: {"No": 0, "Yes": 1},
}


def _enum_options(register: dict) -> dict[str, int] | None:
    if register["address"] in ENUM_OPTIONS:
        return ENUM_OPTIONS[register["address"]]
    if register["data_type"] != "ENUM":
        return None
    options: dict[str, int] = {}
    for value, label in re.findall(r"(\d+)\s*[-：:]\s*([^/;；）)]+)", register["description"]):
        label = label.strip(" （()")
        if label:
            options[label[:40]] = int(value)
    return options if len(options) >= 2 else None


def _bit_options(register: dict) -> list[tuple[int, str]]:
    bits = []
    for bit, label in re.findall(
        r"[Bb]it\s*(\d+)\s*[:：]\s*([^/；;\n]+)", register["description"]
    ):
        bits.append((int(bit), label.strip(" （(")))
    return bits


def _base_modbus_config(config: dict, register: dict, id_):
    return {
        CONF_ID: id_,
        CONF_MODBUS_CONTROLLER_ID: config[CONF_MODBUS_CONTROLLER_ID],
        CONF_REGISTER_TYPE: REGISTER_TYPE_HOLDING,
        CONF_ADDRESS: register["address"],
        CONF_VALUE_TYPE: VALUE_TYPE_U_WORD,
        CONF_SKIP_UPDATES: 0,
        CONF_FORCE_NEW_RANGE: False,
    }


NON_ENTITY_KEYS = {
    CONF_ADDRESS,
    CONF_BITMASK,
    CONF_FORCE_NEW_RANGE,
    CONF_MODBUS_CONTROLLER_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_OPTIMISTIC,
    CONF_REGISTER_TYPE,
    CONF_SKIP_UPDATES,
    CONF_STEP,
    CONF_USE_WRITE_MULTIPLE,
    CONF_VALUE_TYPE,
    CONF_WRITE_LAMBDA,
    "assumed_state",
    "lambda",
    "optionsmap",
}


def _apply_entity_schema(entity: dict, schema):
    entity.update(schema({k: v for k, v in entity.items() if k not in NON_ENTITY_KEYS}))


def _add_duplicate_names(config: dict):
    names = {}
    registers = _load_registers(config[CONF_SHEET])
    for register in registers:
        if not config[CONF_INCLUDE_RESERVED] and not _has_register_metadata(register):
            continue
        if not _has_register_metadata(register):
            continue
        if not _is_readable(register):
            continue
        if _is_binary_type(register) and _bit_options(register):
            continue
        if _is_writable(register) and config[CONF_CREATE_CONTROLS]:
            continue
        if not config[CONF_CREATE_SENSORS]:
            continue
        name = _slug(_name(register).replace("_", " "))
        names.setdefault(name, 0)
        names[name] += 1
    config["_hw211_duplicate_names"] = {
        name for name, count in names.items() if count > 1
    }


def _declare_generated_ids(config: dict):
    _add_duplicate_names(config)
    ids = []
    registers = _load_registers(config[CONF_SHEET])
    for register in registers:
        if not config[CONF_INCLUDE_RESERVED] and not _has_register_metadata(register):
            continue

        if config[CONF_RAW_REGISTERS] and config[CONF_CREATE_SENSORS]:
            ids.append(
                cv.declare_id(ModbusSensor)(
                    _entity_id(config[CONF_NAME_PREFIX], register, "raw_sensor")
                )
            )

        if not _has_register_metadata(register):
            continue

        bits = _bit_options(register) if _is_binary_type(register) else []
        if bits and config[CONF_CREATE_BINARY_SENSORS]:
            for bit, _label in bits:
                if _is_writable(register) and config[CONF_CREATE_CONTROLS]:
                    ids.append(
                        cv.declare_id(ModbusSwitch)(
                            _entity_id(
                                config[CONF_NAME_PREFIX],
                                register,
                                f"bit_{bit}_switch",
                            )
                        )
                    )
                else:
                    ids.append(
                        cv.declare_id(ModbusBinarySensor)(
                            _entity_id(
                                config[CONF_NAME_PREFIX],
                                register,
                                f"bit_{bit}_binary_sensor",
                            )
                        )
                    )
            continue

        options = _enum_options(register)
        if _is_writable(register) and config[CONF_CREATE_CONTROLS]:
            if options and set(options.values()) == {0, 1}:
                ids.append(
                    cv.declare_id(ModbusSwitch)(
                        _entity_id(config[CONF_NAME_PREFIX], register, "switch")
                    )
                )
            elif options:
                ids.append(
                    cv.declare_id(ModbusSelect)(
                        _entity_id(config[CONF_NAME_PREFIX], register, "select")
                    )
                )
            elif register["data_type"] not in ("", "Binary"):
                ids.append(
                    cv.declare_id(ModbusNumber)(
                        _entity_id(config[CONF_NAME_PREFIX], register, "number")
                    )
                )
            elif config[CONF_CREATE_SENSORS] and not config[CONF_RAW_REGISTERS]:
                ids.append(
                    cv.declare_id(ModbusSensor)(
                        _entity_id(config[CONF_NAME_PREFIX], register, "raw_sensor")
                    )
                )
            continue

        if _is_readable(register) and config[CONF_CREATE_SENSORS]:
            ids.append(
                cv.declare_id(ModbusSensor)(
                    _entity_id(config[CONF_NAME_PREFIX], register, "sensor")
                )
            )

    config["_hw211_generated_ids"] = ids
    return config


async def _register_modbus_sensor(config: dict, register: dict, *, raw: bool = False):
    data_type = register["data_type"]
    read_lambda, _ = _scale_expr(data_type)
    id_ = cv.declare_id(ModbusSensor)(
        _entity_id(config[CONF_NAME_PREFIX], register, "raw_sensor" if raw else "sensor")
    )
    entity = _base_modbus_config(config, register, id_)
    entity.update(
        {
            CONF_NAME: _entity_name(config, register) + (" Raw" if raw else ""),
            CONF_BITMASK: 0xFFFFFFFF,
            CONF_INTERNAL: False,
            CONF_DISABLED_BY_DEFAULT: raw,
            CONF_FORCE_UPDATE: config[CONF_FORCE_UPDATE],
            CONF_ACCURACY_DECIMALS: 0 if raw else (1 if data_type in ("TEMP", "TEMP1", "DIGI5") else 0),
        }
    )
    unit = None if raw else _unit(register)
    if unit:
        entity[CONF_UNIT_OF_MEASUREMENT] = unit
    if not raw and data_type in ("TEMP", "TEMP1"):
        entity[CONF_DEVICE_CLASS] = "temperature"
        entity[CONF_STATE_CLASS] = "measurement"
    if not raw and data_type and data_type not in ("ENUM", "Binary"):
        entity["lambda"] = Lambda(read_lambda)
    _apply_entity_schema(entity, sensor.sensor_schema(ModbusSensor))

    var = cg.new_Pvariable(
        entity[CONF_ID],
        entity[CONF_REGISTER_TYPE],
        entity[CONF_ADDRESS],
        0,
        entity[CONF_BITMASK],
        entity[CONF_VALUE_TYPE],
        1,
        entity[CONF_SKIP_UPDATES],
        entity[CONF_FORCE_NEW_RANGE],
    )
    await cg.register_component(var, entity)
    await sensor.register_sensor(var, entity)
    parent = await cg.get_variable(entity[CONF_MODBUS_CONTROLLER_ID])
    cg.add(parent.add_sensor_item(var))
    if "lambda" in entity:
        template_ = await cg.process_lambda(
            entity["lambda"],
            [
                (ModbusSensor.operator("ptr"), "item"),
                (cg.float_, "x"),
                (cg.std_vector.template(cg.uint8).operator("const").operator("ref"), "data"),
            ],
            return_type=cg.optional.template(float),
        )
        cg.add(var.set_template(template_))


async def _register_modbus_number(config: dict, register: dict):
    data_type = register["data_type"]
    read_lambda, write_lambda = _scale_expr(data_type)
    range_ = _range(register)
    id_ = cv.declare_id(ModbusNumber)(_entity_id(config[CONF_NAME_PREFIX], register, "number"))
    entity = _base_modbus_config(config, register, id_)
    entity.update(
        {
            CONF_NAME: _entity_name(config, register),
            CONF_BITMASK: 0xFFFFFFFF,
            CONF_MIN_VALUE: range_[0] if range_ else 0,
            CONF_MAX_VALUE: range_[1] if range_ else 65535,
            CONF_STEP: _step(data_type),
            CONF_USE_WRITE_MULTIPLE: False,
            CONF_INTERNAL: False,
            CONF_DISABLED_BY_DEFAULT: False,
        }
    )
    unit = _unit(register)
    if unit:
        entity[CONF_UNIT_OF_MEASUREMENT] = unit
    if data_type in ("TEMP", "TEMP1"):
        entity[CONF_DEVICE_CLASS] = "temperature"
    if data_type and data_type != "DIGI1":
        entity["lambda"] = Lambda(read_lambda)
    if write_lambda:
        entity[CONF_WRITE_LAMBDA] = Lambda(write_lambda)
    _apply_entity_schema(entity, number.number_schema(ModbusNumber))

    var = cg.new_Pvariable(
        entity[CONF_ID],
        entity[CONF_REGISTER_TYPE],
        entity[CONF_ADDRESS],
        0,
        entity[CONF_BITMASK],
        entity[CONF_VALUE_TYPE],
        1,
        entity[CONF_SKIP_UPDATES],
        entity[CONF_FORCE_NEW_RANGE],
    )
    await cg.register_component(var, entity)
    await number.register_number(
        var,
        entity,
        min_value=entity[CONF_MIN_VALUE],
        max_value=entity[CONF_MAX_VALUE],
        step=entity[CONF_STEP],
    )
    parent = await cg.get_variable(entity[CONF_MODBUS_CONTROLLER_ID])
    cg.add(var.set_parent(parent))
    cg.add(parent.add_sensor_item(var))
    cg.add(var.set_use_write_mutiple(entity[CONF_USE_WRITE_MULTIPLE]))
    if "lambda" in entity:
        template_ = await cg.process_lambda(
            entity["lambda"],
            [
                (ModbusNumber.operator("ptr"), "item"),
                (cg.float_, "x"),
                (cg.std_vector.template(cg.uint8).operator("const").operator("ref"), "data"),
            ],
            return_type=cg.optional.template(float),
        )
        cg.add(var.set_template(template_))
    if CONF_WRITE_LAMBDA in entity:
        template_ = await cg.process_lambda(
            entity[CONF_WRITE_LAMBDA],
            [
                (ModbusNumber.operator("ptr"), "item"),
                (cg.float_, "x"),
                (cg.std_vector.template(cg.uint16).operator("ref"), "payload"),
            ],
            return_type=cg.optional.template(float),
        )
        cg.add(var.set_write_template(template_))


async def _register_modbus_select(config: dict, register: dict, options: dict[str, int]):
    id_ = cv.declare_id(ModbusSelect)(_entity_id(config[CONF_NAME_PREFIX], register, "select"))
    entity = _base_modbus_config(config, register, id_)
    entity.update(
        {
            CONF_NAME: _entity_name(config, register),
            "optionsmap": options,
            CONF_USE_WRITE_MULTIPLE: False,
            CONF_OPTIMISTIC: False,
            CONF_INTERNAL: False,
            CONF_DISABLED_BY_DEFAULT: False,
        }
    )
    _apply_entity_schema(entity, select.select_schema(ModbusSelect))
    var = cg.new_Pvariable(
        entity[CONF_ID],
        entity[CONF_VALUE_TYPE],
        entity[CONF_ADDRESS],
        1,
        entity[CONF_SKIP_UPDATES],
        entity[CONF_FORCE_NEW_RANGE],
        list(options.values()),
    )
    await cg.register_component(var, entity)
    await select.register_select(var, entity, options=list(options.keys()))
    parent = await cg.get_variable(entity[CONF_MODBUS_CONTROLLER_ID])
    cg.add(parent.add_sensor_item(var))
    cg.add(var.set_parent(parent))
    cg.add(var.set_use_write_mutiple(entity[CONF_USE_WRITE_MULTIPLE]))
    cg.add(var.set_optimistic(entity[CONF_OPTIMISTIC]))


async def _register_modbus_switch(config: dict, register: dict, *, bit: int | None = None, label: str | None = None):
    id_ = cv.declare_id(ModbusSwitch)(
        _entity_id(config[CONF_NAME_PREFIX], register, f"bit_{bit}_switch" if bit is not None else "switch")
    )
    entity = _base_modbus_config(config, register, id_)
    entity.update(
        {
            CONF_NAME: _entity_name(
                config, register, f"Bit {bit} {label}" if bit is not None else label
            ),
            CONF_BITMASK: (1 << bit) if bit is not None else 0x0001,
            CONF_USE_WRITE_MULTIPLE: False,
            CONF_INTERNAL: False,
            CONF_DISABLED_BY_DEFAULT: False,
            "assumed_state": False,
        }
    )
    _apply_entity_schema(
        entity, switch.switch_schema(ModbusSwitch, default_restore_mode="DISABLED")
    )
    var = cg.new_Pvariable(
        entity[CONF_ID],
        entity[CONF_REGISTER_TYPE],
        entity[CONF_ADDRESS],
        0,
        entity[CONF_BITMASK],
        entity[CONF_SKIP_UPDATES],
        entity[CONF_FORCE_NEW_RANGE],
    )
    await cg.register_component(var, entity)
    await switch.register_switch(var, entity)
    parent = await cg.get_variable(entity[CONF_MODBUS_CONTROLLER_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_use_write_mutiple(entity[CONF_USE_WRITE_MULTIPLE]))
    cg.add(var.set_assumed_state(False))
    cg.add(parent.add_sensor_item(var))


async def _register_modbus_binary_sensor(config: dict, register: dict, bit: int, label: str):
    id_ = cv.declare_id(ModbusBinarySensor)(
        _entity_id(config[CONF_NAME_PREFIX], register, f"bit_{bit}_binary_sensor")
    )
    entity = _base_modbus_config(config, register, id_)
    entity.update(
        {
            CONF_NAME: _entity_name(config, register, f"Bit {bit} {label}"),
            CONF_BITMASK: 1 << bit,
            CONF_INTERNAL: False,
            CONF_DISABLED_BY_DEFAULT: False,
        }
    )
    _apply_entity_schema(entity, binary_sensor.binary_sensor_schema(ModbusBinarySensor))
    var = cg.new_Pvariable(
        entity[CONF_ID],
        entity[CONF_REGISTER_TYPE],
        entity[CONF_ADDRESS],
        0,
        entity[CONF_BITMASK],
        entity[CONF_SKIP_UPDATES],
        entity[CONF_FORCE_NEW_RANGE],
    )
    await cg.register_component(var, entity)
    await binary_sensor.register_binary_sensor(var, entity)
    parent = await cg.get_variable(entity[CONF_MODBUS_CONTROLLER_ID])
    cg.add(parent.add_sensor_item(var))


async def to_code(config):
    cg.add_global(
        cg.RawStatement(
            '#include "esphome/components/hw211/modbus_binarysensor.h"\n'
            '#include "esphome/components/hw211/modbus_number.h"\n'
            '#include "esphome/components/hw211/modbus_select.h"\n'
            '#include "esphome/components/hw211/modbus_sensor.h"\n'
            '#include "esphome/components/hw211/modbus_switch.h"'
        )
    )
    registers = _load_registers(config[CONF_SHEET])
    for register in registers:
        if not config[CONF_INCLUDE_RESERVED] and not _has_register_metadata(register):
            continue

        if config[CONF_RAW_REGISTERS] and config[CONF_CREATE_SENSORS]:
            await _register_modbus_sensor(config, register, raw=True)

        if not _has_register_metadata(register):
            continue

        bits = _bit_options(register) if _is_binary_type(register) else []
        if bits and config[CONF_CREATE_BINARY_SENSORS]:
            for bit, label in bits:
                if _is_writable(register) and config[CONF_CREATE_CONTROLS]:
                    await _register_modbus_switch(config, register, bit=bit, label=label)
                else:
                    await _register_modbus_binary_sensor(config, register, bit, label)
            continue

        options = _enum_options(register)
        if _is_writable(register) and config[CONF_CREATE_CONTROLS]:
            if options and set(options.values()) == {0, 1}:
                await _register_modbus_switch(config, register)
            elif options:
                await _register_modbus_select(config, register, options)
            elif register["data_type"] not in ("", "Binary"):
                await _register_modbus_number(config, register)
            elif config[CONF_CREATE_SENSORS] and not config[CONF_RAW_REGISTERS]:
                await _register_modbus_sensor(config, register, raw=True)
            continue

        if _is_readable(register) and config[CONF_CREATE_SENSORS]:
            await _register_modbus_sensor(config, register)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_MODBUS_CONTROLLER_ID): cv.use_id(ModbusController),
            cv.Optional(CONF_SHEET, default="dtu_wifi"): cv.one_of("dtu_wifi", "hw211"),
            cv.Optional(CONF_NAME_PREFIX, default="HW211"): cv.string_strict,
            cv.Optional(CONF_INCLUDE_RESERVED, default=False): cv.boolean,
            cv.Optional(CONF_RAW_REGISTERS, default=False): cv.boolean,
            cv.Optional(CONF_FORCE_UPDATE, default=False): cv.boolean,
            cv.Optional(CONF_CREATE_CONTROLS, default=False): cv.boolean,
            cv.Optional(CONF_CREATE_SENSORS, default=True): cv.boolean,
            cv.Optional(CONF_CREATE_BINARY_SENSORS, default=True): cv.boolean,
        }
    ),
    _declare_generated_ids,
)
