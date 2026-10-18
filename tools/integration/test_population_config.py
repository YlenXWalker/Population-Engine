#!/usr/bin/env python3
"""Validate shipped Population data and NPC locations without a game database."""
from pathlib import Path
import re
import struct
import unittest
import zlib

import yaml

ROOT = Path(__file__).resolve().parents[2]


def body(name):
    return yaml.safe_load((ROOT / 'db' / (name + '.yml')).read_text(encoding='utf-8-sig')).get('Body') or []


def maps():
    result = {}
    for name in ('db/map_cache.dat', 'db/re/map_cache.dat'):
        data = (ROOT / name).read_bytes()
        count = struct.unpack_from('<H', data, 4)[0]
        cursor = 8
        for _ in range(count):
            name, width, height, length = struct.unpack_from('<12shhi', data, cursor)
            cursor += 20
            cells = zlib.decompress(data[cursor:cursor + length])
            cursor += length
            result[name.split(b'\0')[0].decode()] = width, height, cells
    return result


class PopulationConfigTest(unittest.TestCase):
    def test_spawn_profiles_and_gear_references(self):
        gear = {row['GearSetName'] for row in body('population_gear_sets') if 'GearSetName' in row}
        profiles = {row['Profile'] for row in body('population_engine') if 'Profile' in row}
        for row in body('population_spawn'):
            self.assertIn(row['Profile'], profiles)
        for source in ('population_engine', 'population_pvp', 'population_vendor_pop'):
            for row in body(source):
                for name in row.get('Jobs', {}).values():
                    if isinstance(name, str):
                        self.assertIn(name, gear)

    def test_vendor_shell_keys_and_placement_maps(self):
        vendors = {row['VendorKey']: row for row in body('population_vendors')}
        known_maps = maps()
        for row in body('population_vendor_pop'):
            self.assertIn(row['VendorKey'], vendors)
        for row in vendors.values():
            placements = row.get('VendorPlacement', [])
            if isinstance(placements, dict):
                placements = [placements]
            for placement in placements:
                self.assertIn(placement['Map'], known_maps)

    def test_third_job_catalog_has_all_thirteen_branches(self):
        jobs = set()
        for row in body('population_engine'):
            if row.get('Profile') == 'third_jobs':
                jobs.update(row['Jobs'])
        self.assertEqual(len(jobs), 26)
        self.assertTrue({'RuneKnight', 'ArchBishop', 'Warlock', 'ShadowChaserT'} <= jobs)
        self.assertNotIn('DragonKnight', jobs)

    def test_all_town_npcs_and_fixed_posts_are_walkable(self):
        known_maps = maps()
        text = (ROOT / 'npc/custom/population/arena.txt').read_text()
        locations = re.findall(r'^(\w+),(\d+),(\d+),4\tduplicate', text, re.M)
        self.assertEqual(len(locations), 42)
        for row in body('population_spawn'):
            locations += [(post['Map'], post['X'], post['Y']) for post in row.get('Posts', [])]
        for name, x, y in locations:
            self.assertIn(name, known_maps)
            width, height, cells = known_maps[name]
            x, y = int(x), int(y)
            self.assertTrue(0 <= x < width and 0 <= y < height, (name, x, y))
            self.assertIn(cells[y * width + x], (0, 3, 6), (name, x, y))


if __name__ == '__main__':
    unittest.main()
