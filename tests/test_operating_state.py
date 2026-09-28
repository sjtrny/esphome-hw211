"""Exercise the actual C++ state logic without writing to a water heater."""

from pathlib import Path
import subprocess
import tempfile
import unittest

from components import hw211


class OperatingStateTest(unittest.TestCase):
    def test_state_transitions_and_missing_data(self):
        root = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory(prefix="hw211-state-test-") as directory:
            binary = Path(directory) / "state-test"
            subprocess.run(
                ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                 "-I", str(root / "components/hw211"),
                 str(root / "tests/operating_state_test.cpp"), "-o", str(binary)],
                check=True, capture_output=True, text=True,
            )
            subprocess.run([str(binary)], check=True)

    def test_source_maps_refer_to_correct_protocol_fields(self):
        meanings = ("Power", "Component status flags", "Operating status flags", "Fault flags")
        for sheet, addresses in hw211.STATE_REGISTERS.items():
            with self.subTest(sheet=sheet):
                registers = {r["address"]: r for r in hw211._load_registers(sheet)}
                self.assertEqual(tuple(hw211.REGISTER_NAMES[sheet][a] for a in addresses), meanings)
                # These must be unscaled words, not temperatures or run-time counters.
                self.assertTrue(all(registers[a]["data_type"] in ("ENUM", "Binary", "") for a in addresses))


if __name__ == "__main__":
    unittest.main()
