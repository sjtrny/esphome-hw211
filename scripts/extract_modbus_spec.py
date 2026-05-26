#!/usr/bin/env python3
"""Extract the EvoHeat Modbus register map from the supplied XLSX file."""

from __future__ import annotations

import json
import re
from pathlib import Path
from xml.etree import ElementTree as ET
from zipfile import ZipFile


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "datasheets" / "EVO270-1_MODBUS communication_protocol_6.5.25.xlsx"
OUTPUT = ROOT / "protocol" / "hw211_modbus.json"
NS = {"a": "http://schemas.openxmlformats.org/spreadsheetml/2006/main"}


def column_number(cell_ref: str) -> int:
    number = 0
    for char in re.sub(r"[^A-Z]", "", cell_ref.upper()):
        number = number * 26 + ord(char) - 64
    return number


def load_shared_strings(archive: ZipFile) -> list[str]:
    root = ET.fromstring(archive.read("xl/sharedStrings.xml"))
    return [
        "".join(text.text or "" for text in item.findall(".//a:t", NS))
        for item in root.findall("a:si", NS)
    ]


def cell_value(cell: ET.Element, strings: list[str]) -> str:
    value = cell.find("a:v", NS)
    if value is None or value.text is None:
        return ""
    if cell.attrib.get("t") == "s":
        return strings[int(value.text)]
    return value.text


def sheet_rows(archive: ZipFile, sheet_path: str, strings: list[str]) -> list[dict[int, str]]:
    root = ET.fromstring(archive.read(sheet_path))
    rows = []
    for row in root.findall(".//a:sheetData/a:row", NS):
        values: dict[int, str] = {}
        for cell in row.findall("a:c", NS):
            values[column_number(cell.attrib["r"])] = cell_value(cell, strings).strip()
        rows.append(values)
    return rows


def extract_registers(rows: list[dict[int, str]]) -> list[dict[str, object]]:
    registers = []
    for row in rows:
        address = row.get(1, "")
        if not address.isdigit():
            continue
        function = row.get(2, "")
        if not function:
            continue
        registers.append(
            {
                "address": int(address),
                "function": function,
                "number": row.get(3, ""),
                "name": row.get(4, ""),
                "byte_length": row.get(7, ""),
                "access": row.get(8, ""),
                "description": row.get(9, ""),
                "mark": row.get(10, ""),
                "data_type": row.get(11, ""),
            }
        )
    return registers


def main() -> None:
    with ZipFile(SOURCE) as archive:
        strings = load_shared_strings(archive)
        sheets = {
            "dtu_wifi": {
                "name": "DTU&WIFI communication protocol",
                "path": "xl/worksheets/sheet1.xml",
                "notes": "Visible sheet. Community ESPHome config uses this map with slave address 99.",
            },
            "hw211": {
                "name": "HW211",
                "path": "xl/worksheets/sheet2.xml",
                "notes": "Hidden sheet in the workbook. Mostly Chinese labels with some English terms.",
            },
        }
        for sheet in sheets.values():
            rows = sheet_rows(archive, sheet["path"], strings)
            sheet["registers"] = extract_registers(rows)

    spec = {
        "device": "EvoHeat EVO270-1 heat pump hot water system",
        "controller": "HW211-family Modbus RTU controller",
        "source": str(SOURCE.relative_to(ROOT)),
        "transport": {
            "protocol": "Modbus RTU",
            "baud_rate": 9600,
            "data_bits": 8,
            "parity": "N",
            "stop_bits": 1,
            "crc": "CRC-16/MODBUS, low byte first on the wire",
            "default_slave_addresses": {
                "centralized_control": "H30 device address parameter",
                "dtu_wifi": 99,
            },
            "supported_function_codes": {
                "3": "read holding registers",
                "6": "write single register",
                "16": "write multiple registers",
            },
        },
        "data_types": {
            "ENUM": {"formula": "x"},
            "Binary": {"formula": "16-bit bitfield"},
            "TEMP": {
                "formula": "signed_int16(x) * 0.1",
                "unit": "degC",
                "fault_value": 32767,
            },
            "TEMP1": {"formula": "(x - 60) * 0.5", "unit": "degC"},
            "DIGI1": {"formula": "x"},
            "DIGI2": {"formula": "x * 10"},
            "DIGI3": {"formula": "x * 100"},
            "DIGI4": {"formula": "x * 5"},
            "DIGI5": {"formula": "x * 0.1"},
            "DIGI6": {"formula": "x * 0.001"},
            "DIGI7": {"formula": "x * 0.5"},
            "DIGI8": {"formula": "x * 2"},
            "DIGI9": {"formula": "x * 0.01"},
        },
        "notable_registers": {
            "current_water_temperature_display": 2025,
            "bottom_tank_temperature": 2020,
            "top_tank_temperature": 2021,
            "target_temperature": 1104,
            "power": 1011,
            "operation_mode": 1013,
            "status0": 2050,
            "status1": 2051,
            "fault0": 2085,
        },
        "sheets": sheets,
    }

    OUTPUT.parent.mkdir(exist_ok=True)
    OUTPUT.write_text(json.dumps(spec, indent=2, ensure_ascii=False) + "\n")
    print(f"Wrote {OUTPUT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
