"""Guard protocol coverage and the identities used by installed HA entities."""

import unittest
from unittest.mock import Mock, patch

from components import hw211


def config_for(sheet):
    config = {
        hw211.CONF_SHEET: sheet,
        hw211.CONF_INCLUDE_RESERVED: False,
        hw211.CONF_CREATE_CONTROLS: False,
        hw211.CONF_CREATE_SENSORS: True,
    }
    hw211._add_duplicate_names(config)
    return config


class EntityMetadataTest(unittest.TestCase):
    def test_all_documented_registers_and_bits_have_names(self):
        for sheet in ("dtu_wifi", "hw211"):
            config = config_for(sheet)
            names = []
            for register in hw211._load_registers(sheet):
                address = register["address"]
                if register["name"]:
                    with self.subTest(sheet=sheet, address=address):
                        self.assertIn(address, hw211.REGISTER_NAMES[sheet])
                        names.append(hw211._entity_name(config, register))
                for bit, _ in hw211._bit_options(register):
                    with self.subTest(sheet=sheet, address=address, bit=bit):
                        self.assertIn(bit, hw211.BIT_NAMES[sheet].get(address, {}))
            self.assertEqual(len(names), len(set(names)), sheet)

    def test_existing_sensor_keys_and_ids_are_preserved(self):
        # Golden values from the pre-rename component, including duplicate names,
        # non-ASCII sheet names and the raw register variant. Do not regenerate
        # these from the new display labels.
        cases = (
            ("dtu_wifi", 1011, False, "power_on_or_off", 2463073586),
            ("dtu_wifi", 1104, False, "target_temp", 847406299),
            ("dtu_wifi", 2012, False, "software_code", 3794694830),
            ("dtu_wifi", 2014, False, "software_code", 3794694824),
            ("dtu_wifi", 2020, False, "bottom_temperature", 3670027599),
            ("dtu_wifi", 2020, True, "bottom_temperature", 3840797200),
            ("dtu_wifi", 2025, False, "app_or_display_interface_temperature", 3146396238),
            ("dtu_wifi", 2085, False, "failure_w10", 2360607059),
            ("hw211", 2011, False, "ambient_temperature", 4078325499),
            ("hw211", 2077, False, "register", 556907826),
            ("hw211", 2079, False, "register", 556907836),
        )
        for sheet, address, raw, slug, key in cases:
            with self.subTest(sheet=sheet, address=address, raw=raw):
                config = config_for(sheet)
                register = next(r for r in hw211._load_registers(sheet) if r["address"] == address)
                var = Mock()
                with patch.object(hw211.cg, "add"):
                    hw211._preserve_api_key(var, config, register, raw=raw)
                var.set_api_key.assert_called_once_with(key)
                suffix = "raw_sensor" if raw else "sensor"
                self.assertEqual(
                    hw211._entity_id("HW211", register, suffix),
                    f"hw211_{address}_{slug}_{suffix}",
                )

    def test_existing_bit_keys_are_preserved(self):
        config = config_for("dtu_wifi")
        register = next(r for r in hw211._load_registers("dtu_wifi") if r["address"] == 2050)
        labels = dict(hw211._bit_options(register))
        for bit, key in ((0, 1254751787), (8, 2106425124), (12, 1656979598)):
            with self.subTest(bit=bit):
                var = Mock()
                with patch.object(hw211.cg, "add"):
                    hw211._preserve_api_key(var, config, register, label=f"Bit {bit} {labels[bit]}")
                var.set_api_key.assert_called_once_with(key)

    def test_sheet_specific_addresses_are_not_confused(self):
        # Same address, different meaning: version vs ambient temperature;
        # expansion valve vs faults; clock flag vs clock hour.
        for address in (2011, 2060, 1151):
            with self.subTest(address=address):
                self.assertNotEqual(
                    hw211.REGISTER_NAMES["dtu_wifi"][address],
                    hw211.REGISTER_NAMES["hw211"][address],
                )

    def test_raw_sensor_keys_are_unique_in_both_sheets(self):
        # The old Chinese raw timer labels collapsed to two ASCII IDs for eight
        # registers. Friendly labels must not conceal duplicate native API keys.
        for sheet in ("dtu_wifi", "hw211"):
            for controls in (False, True):
                config = config_for(sheet)
                config[hw211.CONF_CREATE_CONTROLS] = controls
                hw211._add_duplicate_names(config)
                keys = {}
                for register in hw211._load_registers(sheet):
                    with self.subTest(sheet=sheet, controls=controls, address=register["address"]):
                        var = Mock()
                        with patch.object(hw211.cg, "add"):
                            hw211._preserve_api_key(var, config, register, raw=True)
                        key = var.set_api_key.call_args.args[0]
                        self.assertNotIn(key, keys, f"Same key as register {keys.get(key)}")
                        keys[key] = register["address"]

    def test_categories_distinguish_readback_from_writable_settings(self):
        config = config_for("dtu_wifi")
        settings = {"address": 1036}
        reading = {"address": 2020}
        self.assertEqual(hw211._entity_category(config, settings), "diagnostic")
        self.assertEqual(hw211._entity_category(config, settings, writable=True), "config")
        self.assertEqual(hw211._entity_category(config, reading), "")
        self.assertEqual(hw211._entity_category(config, reading, raw=True), "diagnostic")
        self.assertEqual(hw211._entity_category(config, {"address": 2050}, bit=8), "")
        self.assertEqual(hw211._entity_category(config, {"address": 2085}, bit=0), "diagnostic")


if __name__ == "__main__":
    unittest.main()
