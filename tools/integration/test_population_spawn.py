#!/usr/bin/env python3
"""Characterize production population-spawn phases without a game database."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest
import uuid

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = Path(__file__).with_name("population_spawn")


def extract_function(text, name):
    match = re.search(r"^static [^;{]*\b" + re.escape(name) + r"\([^;{]*\)\s*\{.*?^\}", text, re.M | re.S)
    if not match:
        raise RuntimeError("Missing production function: " + name)
    return match.group()


def production(suite):
    text = (ROOT / "src/map/population_engine.cpp").read_text(encoding="utf-8-sig")
    if suite == "cycle":
        combat = (ROOT / "src/map/population_engine/population_combat.cpp").read_text(encoding="utf-8-sig")
        return extract_function(combat, "population_shell_apply_daily_cycle")
    if suite == "party":
        return extract_function(text.replace("\nvoid population_engine_restore_party(", "\nstatic void population_engine_restore_party("), "population_engine_restore_party")
    if suite == "catalog":
        config = (ROOT / "src/map/population_engine/population_config.cpp").read_text(encoding="utf-8-sig")
        return extract_function(config, "population_store_shell_profile") + "\n" + extract_function(text, "population_engine_select_vendor_spawn")
    functions = {
        "stats": "population_engine_initialize_shell_stats",
        "placement": "population_engine_place_shell",
        "skills": "population_engine_grant_shell_skills",
        "scripts": "population_engine_apply_shell_scripts",
        "behavior": "population_engine_start_shell_behavior",
        "vendor": "population_engine_open_shell_vendor",
        "orchestration": "population_engine_spawn_shell",
    }
    names = (["population_engine_equip_shell_weapon", "population_engine_equip_shell_ammo", "population_engine_equip_shell_armor"]
             if suite == "equipment" else [functions[suite]])
    result = "\n\n".join(extract_function(text, name) for name in names)
    if suite == "stats":
        result = extract_function(text, "population_roll_closed_range") + "\n" + result
    return result


class PopulationSpawnTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.output = Path(tempfile.gettempdir()) / ("population-spawn-test-" + uuid.uuid4().hex)
        cls.output.mkdir()
        if os.name == "nt":
            vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
            install = subprocess.check_output([
                str(vswhere), "-latest", "-products", "*", "-requires",
                "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath",
            ], text=True).strip()
            if not install:
                raise RuntimeError("Visual Studio C++ tools are required")
            cls.dev = Path(install) / "Common7/Tools/VsDevCmd.bat"
        else:
            cls.compiler = shutil.which("g++") or shutil.which("clang++")
            if not cls.compiler:
                raise RuntimeError("A C++17 compiler is required")

    def run_suite(self, suite):
        source = self.output / (suite + ".cpp")
        source.write_text((FIXTURES / (suite + ".cpp")).read_text().replace("// @production", production(suite)), encoding="utf-8")
        executable = self.output / (suite + (".exe" if os.name == "nt" else ""))
        if os.name == "nt":
            script = self.output / (suite + ".cmd")
            script.write_text(
                f'@echo off\ncall "{self.dev}" -arch=x64 -host_arch=x64 >nul\n'
                'if errorlevel 1 exit /b 1\n'
                f'cl /nologo /std:c++17 /EHsc /I"{FIXTURES}" "{source}" /Fe:"{executable}"\n'
                'exit /b %errorlevel%\n', encoding="utf-8")
            command = ["cmd.exe", "/d", "/c", str(script)]
        else:
            command = [self.compiler, "-std=c++17", "-I" + str(FIXTURES), str(source), "-o", str(executable)]
        result = subprocess.run(command, cwd=self.output, capture_output=True, text=True, timeout=120)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        result = subprocess.run([str(executable)], cwd=self.output, capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_permanent_hangouts_and_companions_ignore_daily_cycle(self):
        self.run_suite("cycle")

    def test_transient_party_refresh_owner_and_capacity(self):
        self.run_suite("party")

    def test_120_vendor_profiles_and_key_scoped_placement(self):
        self.run_suite("catalog")

    def test_stat_defaults_ranges_and_random_order(self):
        self.run_suite("stats")

    def test_map_registration_and_failure_cleanup(self):
        self.run_suite("placement")

    def test_skill_grants_and_recalculation(self):
        self.run_suite("skills")

    def test_scripts_activation_and_persistent_bonuses(self):
        self.run_suite("scripts")

    def test_behavior_overrides_and_combat_start(self):
        self.run_suite("behavior")

    def test_vendor_packet_stock_and_rejected_items(self):
        self.run_suite("vendor")

    def test_spawn_phase_order_and_cleanup_ownership(self):
        self.run_suite("orchestration")

    def test_equipment_ammo_and_script_ownership(self):
        self.run_suite("equipment")


if __name__ == "__main__":
    unittest.main()
