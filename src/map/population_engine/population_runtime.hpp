// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder
//
// Population shell runtime helpers (map main thread only).

#ifndef POPULATION_RUNTIME_HPP
#define POPULATION_RUNTIME_HPP

#include <cstdint>
#include <tuple>
#include <vector>

#include <common/cbasetypes.hpp>
#include <common/timer.hpp>

#include "map/pc.hpp"

void population_shell_reset_movement_tick_state(map_session_data *sd, t_tick current_tick);
bool population_shell_can_emit_movement(map_session_data *sd, MovementOwner owner, const char *source);

void population_shell_cleanup_expired_buffs(map_session_data *sd, t_tick current_tick);
bool population_shell_autoshadowspell_autoselect(map_session_data *sd);

int population_shell_move_to_path(std::vector<std::tuple<int, int>> &path, map_session_data *sd, int recursion_depth = 0);
bool population_shell_status_check_reset(map_session_data *sd, t_tick last_tick);

bool population_shell_check_target(map_session_data *sd, unsigned int id);
bool population_shell_check_target_for_movement(map_session_data *sd, unsigned int id);
unsigned int population_shell_check_target_alive(map_session_data *sd);

bool population_shell_simple_find_nearest_mob_and_pathfind(map_session_data *sd, uint32 &out_mob_id, int &out_mob_x, int &out_mob_y);
/// Walk toward a mob that is valid to chase but not yet in attack/skill range (client sight).
bool population_shell_try_approach_combat_target(map_session_data *sd, uint32 target_id);
void population_shell_target_change(map_session_data *sd, int id);

void population_shell_update_mob_tracker(map_session_data *sd);
/// True if any mob within mdetection_cells passes full combat target checks (autocombat-style "valid in range").
bool population_shell_has_valid_mobs_in_detection(map_session_data *sd);
/// Movetype 3 (autocombat ASTAR_ASYNC): path-validated forward roam when `has_valid_mobs_in_detection` is false;
/// else nearest valid mob from tracker. On success: either `out_mob_id` set (chase) or `out_x`/`out_y` (forward step).
bool population_shell_movetype3_get_target(map_session_data *sd, int16 map_id, int16 player_x, int16 player_y,
	bool has_valid_mobs_in_detection, int max_pathfind_attempt, uint32 &out_mob_id, int &out_x, int &out_y);
void population_shell_safe_displaymessage_id(map_session_data *sd, int msg_id);
void population_shell_safe_updatestatus(map_session_data *sd, _sp type);
void population_shell_send_message_id(map_session_data *sd, const char *type, int msg_id, int delay, void *target);

void population_shell_status_checkmapchange(map_session_data *sd);

/// Skill or melee vs mob target (minimal antispam vs. AttackCommand).
bool population_shell_try_attack(map_session_data *sd, uint32 target_id, uint16 skill_id, uint16 skill_lv);

/// Arena PvP: check that `id` is a valid, living, opposing-team arena shell on the same map.
bool population_shell_check_arena_target(map_session_data *sd, unsigned int id);
/// Arena PvP: find the nearest living opposing-team arena shell within detection range. Returns 0 if none.
unsigned int population_shell_find_arena_opponent(map_session_data *sd);
/// Arena PvP: basic attack or skill attack against a PC target (bypasses mob-only checks).
bool population_shell_try_arena_attack(map_session_data *sd, uint32 target_id, uint16 skill_id, uint16 skill_lv);

/// Compute a placement position for ground/trap skills.
/// When around_range > 0, picks a random passable cell within that radius of the shell's
/// own position (mirrors mob_skill_db around2/around3 behaviour).  Falls back to the
/// provided base (x, y) when around_range == 0 or no passable cell is found.
void population_shell_resolve_placement(const map_session_data *sd, uint8_t around_range, int16_t &x, int16_t &y);

// Wander/path runtime

// Population wander AI and pathfinding.

class map_session_data;

void population_engine_path_erase_for_pc(int32 id);
void population_engine_path_clear_all();
void population_engine_path_register_wander_state(map_session_data *sd);

void population_engine_path_register_timer_funcs();
void population_engine_path_restart_wander_timer();
void population_engine_path_stop_wander_timer();

TIMER_FUNC(population_engine_wander_timer);

/// Rolling per-tick instrumentation for the wander timer.
/// All times are in microseconds; counts are per-tick averages over the last N ticks.
/// Goal: identify which sub-step (freecell / fallback / unit_walktoxy) dominates
/// a populated wander tick before committing to A2/A3/A4 architecture work.
struct WanderTimingSnapshot {
	uint64_t ticks_recorded;       ///< Total wander ticks observed since reset.
	uint64_t window_size;          ///< Sliding window length (in ticks).

	// Per-tick averages over the window:
	double   avg_total_us;         ///< Total wall time spent in the wander timer body.
	double   avg_freecell_us;      ///< Time inside map_search_freecell.
	double   avg_fallback_us;      ///< Time inside population_engine_pick_adjacent_passable (incl. path_search).
	double   avg_unit_walktoxy_us; ///< Time inside unit_walktoxy (the suspected hotspot).
	double   avg_processed;        ///< Shells walked per tick.
	double   avg_examined;         ///< Shells examined per tick (cursor sweep).

	// Most recent tick (un-averaged) for spotting outliers:
	uint64_t last_total_us;
	uint64_t last_freecell_us;
	uint64_t last_fallback_us;
	uint64_t last_unit_walktoxy_us;
	uint32_t last_processed;
	uint32_t last_examined;

	// Worst tick seen so far:
	uint64_t worst_total_us;
	uint32_t worst_processed;

	// Cumulative call counters (reset by reset_timing()):
	uint64_t freecell_calls;
	uint64_t fallback_calls;
	uint64_t unit_walktoxy_calls;
};

WanderTimingSnapshot population_engine_path_get_timing_snapshot();
void                 population_engine_path_reset_timing();

#endif /* POPULATION_RUNTIME_HPP */
