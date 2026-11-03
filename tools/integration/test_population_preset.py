#!/usr/bin/env python3
"""Validate the optional BanquetOfHeroes 4,000-shell pre-renewal preset."""
from pathlib import Path
import re
import struct
import unittest
import zlib

import yaml

ROOT = Path(__file__).resolve().parents[2]


def body(name):
    return yaml.safe_load((ROOT / 'db/population_presets/prerenewal_4000' / (name + '.yml')).read_text(encoding='utf-8-sig')).get('Body') or []


def maps(mode='re', include_imports=False):
    result = {}
    files = ['db/map_cache.dat', f'db/{mode}/map_cache.dat']
    if include_imports:
        files.append('db/import/map_cache.dat')
    for name in files:
        if not (ROOT / name).is_file():
            continue
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


class PopulationPresetTest(unittest.TestCase):
    def test_preset_jobs_gear_and_imports_are_self_contained(self):
        gear = {row['GearSetName'] for row in yaml.safe_load(
            (ROOT / 'db/population_gear_sets.yml').read_text())['Body'] if 'GearSetName' in row}
        allowed = {job for row in body('population_engine') for job in row.get('Jobs', {})}
        for source in ('population_engine', 'population_pvp'):
            for row in body(source):
                self.assertNotEqual(row.get('Profile'), 'third_jobs')
                for job, kit in row.get('Jobs', {}).items():
                    self.assertIn(job, allowed)
                    if isinstance(kit, str):
                        self.assertIn(kit, gear)
            data = yaml.safe_load((ROOT / 'db/population_presets/prerenewal_4000' / (source + '.yml')).read_text())
            self.assertEqual(data['Footer']['Imports'], [{'Path': 'db/import/' + source + '.yml'}])

    def test_pre_renewal_population_budget_is_4000(self):
        groups = {
            'novice': (200, ['novice_default']),
            'first': (800, ['combat_pve']),
            'second': (1400, ['pve_priest', 'pve_knight', 'pve_paladin', 'pve_caster',
                              'pve_archer', 'pve_monk', 'pve_assassin', 'pve_rogue',
                              'pve_merchant_cart']),
            'transcended': (1200, ['pve_high_novice', 'pve_high_first', 'pve_transcended']),
            'expanded': (400, ['pve_super_novice', 'pve_taekwon', 'pve_gunslinger', 'pve_ninja']),
        }
        rows = body('population_spawn')
        spawns = {row['Profile']: row for row in rows}
        self.assertEqual(len(rows), len(spawns))
        self.assertEqual(set(spawns), {name for _, names in groups.values() for name in names})
        total = 0
        for group, (expected, names) in groups.items():
            actual = sum(spawns[name].get(category + 'Population', 0)
                         for name in names for category in ('Towns', 'Fields', 'Dungeons'))
            self.assertEqual(actual, expected, group)
            total += actual
        self.assertEqual(total, 4000)

    def test_ambient_jobs_cover_only_pre_renewal_classes_without_shadowing(self):
        profiles = {row['Profile']: row for row in body('population_engine')}
        owners = {}
        for profile in profiles.values():
            for job in profile.get('Jobs', {}):
                self.assertNotIn(job, owners, (job, profile['Profile']))
                owners[job] = profile['Profile']
        active_jobs = {job for row in body('population_spawn')
                       for job in profiles[row['Profile']]['Jobs']}
        first = {'Swordsman', 'Mage', 'Archer', 'Acolyte', 'Merchant', 'Thief'}
        second = {'Knight', 'Priest', 'Wizard', 'Blacksmith', 'Hunter', 'Assassin',
                  'Crusader', 'Monk', 'Sage', 'Rogue', 'Alchemist', 'Bard', 'Dancer'}
        high_first = {'High' + job for job in first}
        transcended = {'LordKnight', 'HighPriest', 'HighWizard', 'Whitesmith', 'Sniper',
                       'AssassinCross', 'Paladin', 'Champion', 'Professor', 'Stalker',
                       'Creator', 'Clown', 'Gypsy'}
        expanded = {'SuperNovice', 'Taekwon', 'StarGladiator', 'SoulLinker', 'Gunslinger', 'Ninja'}
        self.assertEqual(active_jobs, {'Novice', 'HighNovice'} | first | second |
                         high_first | transcended | expanded)
        self.assertEqual(set(profiles['combat_pve']['Jobs']), first)
        self.assertEqual(set(profiles['pve_high_first']['Jobs']), high_first)
        self.assertEqual(set(profiles['pve_transcended']['Jobs']), transcended)
        stats = yaml.safe_load((ROOT / 'db/pre-re/job_stats.yml').read_text())['Body']
        normalize = lambda name: name.replace('_', '').lower()
        available = {normalize(job) for row in stats for job in row.get('Jobs', {})}
        aliases = {'Swordsman': 'Swordman', 'HighNovice': 'Novice_High',
                   **{'High' + job: ('Swordman' if job == 'Swordsman' else job) + '_High'
                      for job in first}}
        parser = (ROOT / 'src/map/population_engine/population_config.cpp').read_text(encoding='utf-8-sig')
        for job in active_jobs:
            self.assertIn(normalize(aliases.get(job, job)), available, job)
            self.assertRegex(parser, r'\{\s*"' + re.escape(job) + r'"\s*,')

    def test_pre_renewal_spawn_maps_and_caps_preserve_the_full_budget(self):
        known_maps = maps('pre-re', include_imports=True)
        for row in body('population_spawn'):
            for category in ('Towns', 'Fields', 'Dungeons'):
                count = row.get(category + 'Population', 0)
                if not count:
                    continue
                names = row.get(category, [])
                self.assertTrue(names, (row['Profile'], category))
                self.assertEqual(len(names), len(set(names)), (row['Profile'], category))
                cap = row.get(category + 'MaxPerMap', 0)
                if cap:
                    self.assertGreaterEqual(cap, (count + len(names) - 1) // len(names))
                for name in names:
                    self.assertIn(name, known_maps, (row['Profile'], category, name))
                    self.assertTrue(any(cell in (0, 3, 6) for cell in known_maps[name][2]), name)

    def test_ambient_job_levels_stay_within_pre_renewal_caps(self):
        caps = {'novice_default': (9, 10), 'pve_high_novice': (9, 10),
                'combat_pve': (99, 50), 'pve_high_first': (99, 50),
                'pve_transcended': (99, 70), 'pve_super_novice': (99, 99)}
        active = {row['Profile'] for row in body('population_spawn')}
        for profile in body('population_engine'):
            name = profile['Profile']
            if name not in active:
                continue
            base_max, job_max = caps.get(name, (99, 50))
            for key, maximum in (('BaseLevel', base_max), ('JobLevel', job_max)):
                low, high = profile[key]
                self.assertTrue(1 <= low <= high <= maximum, (name, key, low, high))


if __name__ == '__main__':
    unittest.main()
