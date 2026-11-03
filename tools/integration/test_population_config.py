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
        renewal_profiles = profiles | {row['Profile'] for row in body('re/population_engine')}
        for row in body('re/population_spawn'):
            self.assertIn(row['Profile'], renewal_profiles)
        for source in ('population_engine', 'population_pvp', 'population_vendor_pop', 're/population_engine', 're/population_pvp'):
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
        for row in body('re/population_engine'):
            if row.get('Profile') == 'third_jobs':
                jobs.update(row['Jobs'])
        self.assertEqual(len(jobs), 26)
        self.assertTrue({'RuneKnight', 'ArchBishop', 'Warlock', 'ShadowChaserT'} <= jobs)
        self.assertNotIn('DragonKnight', jobs)

    def test_renewal_rows_are_mode_scoped_before_local_overrides(self):
        for name in ('population_engine', 'population_pvp', 'population_spawn'):
            data = yaml.safe_load((ROOT / 'db' / (name + '.yml')).read_text())
            self.assertFalse(any(row.get('Profile') == 'third_jobs' for row in data.get('Body', [])))
            self.assertEqual(data['Footer']['Imports'], [
                {'Path': 'db/re/' + name + '.yml', 'Mode': 'Renewal'},
                {'Path': 'db/import/' + name + '.yml'},
            ])
            self.assertEqual([row['Profile'] for row in body('re/' + name)], ['third_jobs'])
        self.assertEqual({job for row in body('re/population_engine') for job in row['Jobs']},
                         {job for row in body('re/population_pvp') for job in row['Jobs']})

    def test_engine_keeps_standalone_navigation_types(self):
        text = (ROOT / 'src/map/population_engine/population_shell.hpp').read_text()
        self.assertNotIn('#include "map/autocombat_core.hpp"', text)
        for name in ('MovementOwner', 'MovementOwnerReason', 'LocalNavState'):
            self.assertIn('enum class ' + name, text)

    def test_all_town_npcs_and_fixed_posts_are_walkable(self):
        known_maps = maps()
        text = (ROOT / 'npc/custom/population/arena.txt').read_text()
        locations = re.findall(r'^(\w+),(\d+),(\d+),4\tduplicate', text, re.M)
        self.assertEqual(len(locations), 42)
        recruiter = (ROOT / 'npc/custom/population/companions.txt').read_text()
        locations += re.findall(r'^(\w+),(\d+),(\d+),4\tscript', recruiter, re.M)
        self.assertEqual(len(locations), 43)
        for row in body('population_spawn'):
            locations += [(post['Map'], post['X'], post['Y']) for post in row.get('Posts', [])]
        for name, x, y in locations:
            self.assertIn(name, known_maps)
            width, height, cells = known_maps[name]
            x, y = int(x), int(y)
            self.assertTrue(0 <= x < width and 0 <= y < height, (name, x, y))
            self.assertIn(cells[y * width + x], (0, 3, 6), (name, x, y))


    def test_companion_npc_commands_are_registered_in_the_script_engine(self):
        npc = (ROOT / 'npc/custom/population/companions.txt').read_text()
        commands = set(re.findall(r'\b(population_\w+)\(', npc))
        definitions = (ROOT / 'src/custom/script_def.inc').read_text()
        implementations = (ROOT / 'src/custom/script.inc').read_text()
        commands.update(('population_companion_recruit', 'population_companion_dismiss',
                         'population_hangout_spawn', 'population_companion_count',
                         'population_arena_position', 'population_arena_result'))
        for command in commands:
            self.assertRegex(definitions, r'BUILDIN_DEF\(\s*' + re.escape(command) + r'\s*,')
            self.assertIn('BUILDIN_FUNC(' + command + ')', implementations)

    def test_pc_setpos_completes_population_warps_without_a_client_ack(self):
        pc = (ROOT / 'src/map/pc.cpp').read_text(encoding='utf-8-sig')
        start = pc.index('enum e_setpos pc_setpos(')
        body = pc[start:pc.index('enum e_setpos pc_setpos_savepoint', start)]
        self.assertLess(body.index('m < 0 && IS_POPULATION_ENGINE_ACCOUNT_ID'),
                        body.index('sd->state.changemap ='))
        complete = body.index('population_engine_complete_shell_warp(sd)')
        self.assertGreater(complete, body.index('sd->m = m;'))
        self.assertLess(complete, body.rindex('return SETPOS_OK;'))
        self.assertRegex(body, r'IS_POPULATION_ENGINE_ACCOUNT_ID\(sd->status.account_id\)\s*&&\s*'
                        r'!population_engine_complete_shell_warp\(sd\)')

    def test_arena_menu_guards_array_indexes_and_existing_companions(self):
        text = (ROOT / 'npc/custom/population/arena.txt').read_text()
        choice = text.split('// BEGIN arena job choice', 1)[1].split('// END arena job choice', 1)[0]
        self.assertNotIn('?', choice)
        self.assertIn('if (.@pick == 1)', choice)
        self.assertIn('.@pick >= 2 && .@pick <= .@nj + 1', choice)
        self.assertLess(text.index('population_companion_count()'), text.index('$@pop_arena_owner[.@room] ='))
        self.assertIn('warp .@map$, .@ally_x, .@ally_y;', text)
        self.assertIn('@pop_arena_logging_out = 0;', text)

    def test_arena_wander_and_combat_use_formation(self):
        runtime = (ROOT / 'src/map/population_engine/population_runtime.cpp').read_text(encoding='utf-8-sig')
        self.assertIn('sd->pop.arena_team || sd->pop.companion_owner', runtime)
        combat = (ROOT / 'src/map/population_engine/population_combat.cpp').read_text(encoding='utf-8-sig')
        self.assertIn('population_engine_arena_group_step(sd)', combat)
        self.assertIn('pe.target_id && !hold_formation', combat)
        arena = combat.split('if (sd->pop.arena_team > 0) {', 1)[1].split('// Do not drop a chase target', 1)[0]
        self.assertNotIn('population_shell_status_check_reset(sd, current_tick)', arena)

    def test_arena_elimination_monitor_and_cleanup_are_bounded(self):
        text = (ROOT / 'npc/custom/population/arena.txt').read_text()
        self.assertIn('@pop_arena_deadline = gettimetick(2) + 600;', text)
        self.assertIn('OnMatchCheck:', text)
        self.assertIn('population_arena_result(@pop_arena_map$, getcharid(0))', text)
        for result in (1, -1, -2):
            self.assertIn(f'if (.@result == {result})', text)
        stop = text.split('S_Stop:', 1)[1]
        self.assertIn('deltimer "Population Arena::OnMatchCheck";', stop)
        self.assertIn('$@pop_arena_owner[.@room] = 0;', stop)
        self.assertLess(stop.index('population_arena_stop(@pop_arena_map$)'),
                        stop.index('recovery 0, getcharid(0), 4'))
        pc = (ROOT / 'src/map/pc.cpp').read_text(encoding='utf-8-sig')
        death = pc.split('int32 pc_dead(', 1)[1].split('void pc_revive(', 1)[0]
        self.assertLess(death.index('population_engine_on_shell_death(sd)'), death.index('// pvp'))
        self.assertIn('!population_arena && !battle_config.pk_mode', death)
        status = (ROOT / 'src/map/status.cpp').read_text(encoding='utf-8-sig')
        revive = status.split('int32 status_revive(', 1)[1].split('/**', 1)[0]
        self.assertIn('arena_sd->pop.arena_eliminated', revive)
        self.assertLess(revive.index('arena_sd->pop.arena_eliminated'), revive.index('status->hp += hp'))


if __name__ == '__main__':
    unittest.main()
