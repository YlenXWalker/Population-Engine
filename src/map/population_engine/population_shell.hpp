// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder
//
// Population Engine — merged core header.
// Consolidates:
//   population_engine_globals.hpp   — shared extern state (g_population_engine_pcs)
//   population_engine_internal.hpp  — internal function declarations
//   population_engine_types.hpp     — standalone runtime types/enums/structs
//   population_shell_constants.hpp  — timing constants and PAI flag namespace

#ifndef POPULATION_SHELL_HPP
#define POPULATION_SHELL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <common/cbasetypes.hpp>
#include <common/timer.hpp>

/// Which subsystem currently holds the movement token for this shell.
/// Exactly one owner can emit movement per tick.
enum class MovementOwner : uint8 {
	None   = 0,
	Combat = 1,
	Loot   = 2,
	Roam   = 3,
};

enum class MovementOwnerReason : uint8 {
	None           = 0,
	TargetActive   = 1,
	LootActive     = 2,
	NoValidTarget  = 3,
	TargetSearch   = 4,
	PathRecovery   = 5,
};

// ---------------------------------------------------------------------------
// Local navigation FSM
// ---------------------------------------------------------------------------

enum class LocalNavState : uint8 {
	DirectAdvance = 0,
	WallFollow    = 1,
	Recover       = 2,
};

// ---------------------------------------------------------------------------
// Navigation blocked cells
// ---------------------------------------------------------------------------

constexpr int PE_NAV_BLOCKED_CELLS_MAX = 16;
constexpr int PE_TYPE3_HOTSPOTS_MAX    = 8;

struct CANavBlockedCell {
	int    x          = 0;
	int    y          = 0;
	t_tick until_tick = 0;
};

// ---------------------------------------------------------------------------
// Forward-roam hotspot seeding
// ---------------------------------------------------------------------------

struct Type3Hotspot {
	int    x              = 0;
	int    y              = 0;
	int    score          = 0;
	t_tick last_seen_tick = 0;
};



#include <common/timer.hpp> // t_tick

class map_session_data;
struct PopulationEngine;

// ---------------------------------------------------------------------------
// Shared extern state
// ---------------------------------------------------------------------------

extern std::vector<map_session_data *> g_population_engine_pcs;

// ---------------------------------------------------------------------------
// Internal function declarations
// ---------------------------------------------------------------------------

/// Remove one shell from the map (map_quit). Clears chat/wander keys.
void population_engine_shell_release(map_session_data *sd);

/// Removes stale shells from g_population_engine_pcs and returns them.
/// Caller must call population_engine_shell_release on each returned pointer.
std::vector<map_session_data*> population_engine_collect_stale_shells();

/// Increment walk_failures counter in g_population_engine_stats.
void population_engine_stats_record_walk_failure();

// ---------------------------------------------------------------------------
// Timing constants
// ---------------------------------------------------------------------------

/// Defer SC start while warping / inactive
constexpr int PE_SHELL_DEFER_START_MS = 200;
/// Target exclusion after forced clear (matches legacy AC_TARGET_EXCLUSION_DURATION_MS)
constexpr int PE_SHELL_TARGET_EXCLUSION_MS = 500;
/// Stuck target / item pick (matches legacy autocombat timeouts)
constexpr int PE_SHELL_TARGET_ATTACK_TIMEOUT_MS = 4000;
constexpr int PE_SHELL_ITEM_PICK_TIMEOUT_MS = 5000;
constexpr int PE_SHELL_ATTACK_FAIL_CLEAR_COUNT = 5;
constexpr int PE_SHELL_SKILL_FAIL_CLEAR_TARGET_MS = 2000;
constexpr int PE_SHELL_ATTACK_APPROACH_CELLS = 2;
/// Max defer timer retries (~30s at 200ms) before giving up
constexpr intptr_t PE_SHELL_DEFER_MAX_ATTEMPTS = 150;

/// Population AI behavior flags (bitmask, read from battle_config.population_engine_ai):
namespace PAI {
	constexpr int32 ChaseRefresh    = 0x001; ///< Re-evaluate chase path every step
	constexpr int32 TargetSwitch    = 0x002; ///< Switch to closer enemy mid-combat
	constexpr int32 SkillWhileChase = 0x004; ///< Evaluate skills while approaching
	constexpr int32 ReactiveCheck   = 0x008; ///< Re-check conditions every tick
	constexpr int32 FleeOnLowHP    = 0x010; ///< Squishy roles retreat at low HP
	constexpr int32 SupportPriority = 0x020; ///< Support roles heal before self-buff
	constexpr int32 PackBehavior   = 0x040; ///< Share target info with nearby shells
	constexpr int32 KiteRanged     = 0x080; ///< Ranged jobs maintain max skill range
	constexpr int32 ComboAwareness = 0x100; ///< Chain skills fire in sequence
	constexpr int32 BossAvoidance  = 0x200; ///< Non-tank roles avoid boss monsters
}

// ---------------------------------------------------------------------------
// Runtime types
// — pulls in MovementOwner, MovementOwnerReason, LocalNavState,
//   CANavBlockedCell, Type3Hotspot, CA_NAV_BLOCKED_CELLS_MAX, CA_MAX_HOTSPOTS
//   from autocombat_core.hpp, then adds PE-specific aliases and structs.
// ---------------------------------------------------------------------------



constexpr std::size_t PE_RECENT_TARGET_MAX = 32;
constexpr std::size_t PE_SKILL_COOLDOWN_MAX = 50;
constexpr std::size_t PE_MOB_TRACKER_MAX_ENTRIES = 500;

// Dynamic flat map for population-shell runtime state.
//
// AutoCombat uses fixed-size CAFlatMap for small, hard-bounded caches. Population
// also has mob tracking, where a fixed 500-entry array inside every shell would
// waste memory while idle. This wrapper keeps the same pair-like iterator surface
// but stores entries contiguously and allocates only as entries are observed.
template<typename K, typename V, std::size_t MaxEntries = 0>
class PEFlatMap {
public:
	struct Entry {
		K first{};
		V second{};
	};

	using container_type = std::vector<Entry>;
	using iterator = typename container_type::iterator;
	using const_iterator = typename container_type::const_iterator;

	void clear() { entries_.clear(); }
	bool empty() const { return entries_.empty(); }
	std::size_t size() const { return entries_.size(); }

	void reserve(std::size_t count)
	{
		if (MaxEntries > 0 && count > MaxEntries)
			count = MaxEntries;
		entries_.reserve(count);
	}

	iterator find(const K& key)
	{
		for (auto it = entries_.begin(); it != entries_.end(); ++it) {
			if (it->first == key)
				return it;
		}
		return entries_.end();
	}

	const_iterator find(const K& key) const
	{
		for (auto it = entries_.begin(); it != entries_.end(); ++it) {
			if (it->first == key)
				return it;
		}
		return entries_.end();
	}

	V& operator[](const K& key)
	{
		auto it = find(key);
		if (it != end())
			return it->second;
		if (MaxEntries > 0 && entries_.size() >= MaxEntries) {
			entries_.front() = Entry{key, V{}};
			return entries_.front().second;
		}
		entries_.push_back(Entry{key, V{}});
		return entries_.back().second;
	}

	V* append_default(const K& key)
	{
		if (MaxEntries > 0 && entries_.size() >= MaxEntries)
			return nullptr;
		entries_.push_back(Entry{key, V{}});
		return &entries_.back().second;
	}

	iterator erase(iterator it) { return entries_.erase(it); }

	void erase(const K& key)
	{
		auto it = find(key);
		if (it != end())
			entries_.erase(it);
	}

	bool count(const K& key) const { return find(key) != end(); }

	iterator begin() { return entries_.begin(); }
	iterator end() { return entries_.end(); }
	const_iterator begin() const { return entries_.begin(); }
	const_iterator end() const { return entries_.end(); }
	const_iterator cbegin() const { return entries_.cbegin(); }
	const_iterator cend() const { return entries_.cend(); }

private:
	container_type entries_{};
};

// ---------------------------------------------------------------------------
// Active buff tracking
// ---------------------------------------------------------------------------

struct s_pe_active_buff {
	uint16 skill_id   = 0;
	uint16 skill_lv   = 0;
	t_tick expires_at = 0;  ///< When this buff expires (gettick()-based)
	uint32 target_id  = 0;  ///< Who the buff is on (0 = self)

	s_pe_active_buff() = default;
	s_pe_active_buff(uint16 id, uint16 lv, t_tick expires, uint32 target = 0)
		: skill_id(id), skill_lv(lv), expires_at(expires), target_id(target) {}
};

// ---------------------------------------------------------------------------
// Mob tracking
// ---------------------------------------------------------------------------

struct s_pe_tracked_mob {
	uint32  mob_id      = 0;
	int16   mob_type_id = 0; ///< mob_data->mob_id (the type, not the instance ID)
	int     x           = 0;
	int     y           = 0;
	t_tick  last_seen   = 0;
	int     hp          = 0;
	int     max_hp      = 0;
	uint32  target_id   = 0;
};

struct s_pe_mob_tracker {
	t_tick last_scan     = 0;
	t_tick scan_interval = 1000; ///< Scan interval in ms (default 1 s)
	PEFlatMap<uint32, s_pe_tracked_mob, PE_MOB_TRACKER_MAX_ENTRIES> tracked_mobs; ///< Key: mob instance ID
	int16  map_id        = 0;
};

// ---------------------------------------------------------------------------
// Position record
// ---------------------------------------------------------------------------

struct s_pe_position {
	int   map = 0;
	short x   = 0;
	short y   = 0;
	short dx  = 0;
	short dy  = 0;
};

// ---------------------------------------------------------------------------
// Mob-map record
// ---------------------------------------------------------------------------

struct s_pe_mobs {
	uint16_t map = 0; ///< mapindex of the most recent mob scan.
};

// ---------------------------------------------------------------------------
// Detection cache
// ---------------------------------------------------------------------------

struct s_pe_detection_cache {
	std::vector<unsigned int> cached_monsters{}; ///< IDs from last mob scan.
	std::vector<unsigned int> cached_items{};    ///< IDs from last item scan.
	t_tick last_update  = 0; ///< Tick of last scan (0 = never).
	int    last_x       = 0; ///< Shell x at time of last scan.
	int    last_y       = 0; ///< Shell y at time of last scan.
	int    cache_radius = 0; ///< Radius used for the cached scan.
};

// Population shell state

//
// Population shell state: skill lists and the full sd->pop runtime struct.

namespace expanded_ai { class ExpandedCondition; }

/// One offensive skill the population shell may rotate through (seeded from the PC skill tree or
/// population_skill_db.yml).  condition / cond_* mirror PopSkillCondition from population_skill_db.hpp
/// but stored as plain integers here to avoid cascading header dependencies.
struct PopulationShellCombatSkill {
	uint16_t skill_id   = 0;
	uint16_t skill_lv   = 0;
	uint16_t rate       = 10000; ///< 1–10000 per-10000; 0 = disabled
	bool     active     = true;
	uint8_t  condition  = 0;     ///< PopSkillCondition cast to uint8_t
	uint8_t  cond_value_num = 0; ///< Numeric threshold (HP%, SP%, cells, zone id)
	int16_t  cond_sc_resolved = -1; ///< Resolved sc_type for status conditions (-1 = none)
	uint8_t  target     = 0;    ///< 0=enemy (default), 2=ally (target=1 self skills go to buff list)
	uint8_t  around_range = 0;  ///< If >0, place ground/trap skills at a random cell within this radius of the shell
	bool     around_target = false; ///< When true, around_range centres on the enemy (mob_skill_db around5-8); when false (default), on self (around1-4)
	uint32_t cooldown_ms  = 0;  ///< Per-skill cooldown in ms after a successful cast (0 = no individual cooldown)
	std::shared_ptr<expanded_ai::ExpandedCondition> expanded; ///< Set iff condition == Expanded; evaluated in lieu of the flat condition fields.
};

/// One maintenance skill (Target:1=self, Target:2=ally) the population shell casts on upkeep.
/// Self skills re-cast whenever the associated SC expires.
/// Ally skills re-cast whenever a suitable ally (HP low, missing status, etc.) is found.
struct PopulationShellBuffSkill {
	uint16_t skill_id   = 0;
	uint16_t skill_lv   = 0;
	uint8_t  condition  = 0;     ///< PopSkillCondition cast to uint8_t
	uint8_t  cond_value_num = 0;
	int16_t  cond_sc_resolved = -1;
	uint8_t  target     = 1;     ///< 1=self, 2=ally
	uint8_t  around_range = 0;   ///< If >0, place ground/trap skills at a random cell within this radius of the shell
	bool     around_target = false; ///< When true, around_range centres on the enemy (mob_skill_db around5-8); when false (default), on self (around1-4)
	uint32_t cooldown_ms  = 0;   ///< Per-skill cooldown in ms after a successful cast (0 = no individual cooldown)
	std::shared_ptr<expanded_ai::ExpandedCondition> expanded; ///< Set iff condition == Expanded; evaluated in lieu of the flat condition fields.
};

/// Population engine shell state (sd->pop). Only meaningful for bots managed by the population engine;
/// real players leave this in its default-constructed state.
struct s_population {
	std::shared_ptr<PopulationEngine> profile_config;
	uint32_t companion_owner = 0; ///< Real player's char ID; zero for ambient shells
	std::string post_key;
	std::vector<PopulationShellCombatSkill> attack_skills; ///< Offensive skill rotation (not `ca.autocombatskills` / not autosupport).
	std::vector<PopulationShellBuffSkill>   buff_skills;   ///< Self-buff maintenance list (Target:1 from population_skill_db.yml).
	t_tick   reactive_buff_cd    = 0;  ///< Independent buff cooldown — decoupled from ca.combat.skill_cooldown_tick so self-buffs and attacks can fire in the same tick.
	t_tick   sticky_until        = 0;  ///< Tick at which the current sticky target commitment expires.
	uint32_t flags               = 0;  ///< PSF_* flag bitmask from population_engine.yml Flags: list.
	uint32_t sticky_target_id    = 0;  ///< Sticky-target commitment so the shell does not ping-pong between equidistant enemies. Standalone of autocombat's t3 sticky.
	int      max_attack_skill_range = -1; ///< Cached max range across attack_skills.
	int16_t  spawn_x             = 0;  ///< Spawn coordinates used by Guard behavior for return-to-post.
	int16_t  spawn_y             = 0;  ///< Spawn coordinates used by Guard behavior for return-to-post.
	int16_t  spawn_map_id        = -1; ///< Internal map ID of the designated spawn map (drift detection / death respawn).
	int      respawn_timer       = -1; ///< INVALID_TIMER when no respawn pending; else timer id from add_timer. Cancel before re-adding to prevent duplicates if a shell dies twice within the respawn window.
	uint8_t  attack_skill_cursor = 0;  ///< Round-robin index into attack_skills.
	uint8_t  map_category        = 0;  ///< Map category at spawn (0=none, 1=town, 2=field, 3=dungeon).
	uint8_t  behavior            = 2;  ///< Resolved behavior (mirrors PopulationBehavior, stored as uint8). Default: Combat.
	uint8_t  behavior_base       = 2;  ///< Original configured behavior (for daily cycle restore).
	int8_t   arena_team          = 0;  ///< PvP arena team (0=not in arena, 1=teamA, 2=teamB). Set by population_arena_start.
	int8_t   role                = 0;  ///< Combat role (0=none, 1=tank, 2=support, 3=attacker). From Role: in population_engine.yml.
	uint8_t  db_source           = 0;  ///< PopulationDbSource (0=Main, 1=Pvp, 2=Vendor). Set at spawn time so runtime lookups can find the correct DB.
	t_tick   last_attacked_tick  = 0;  ///< gettick() when shell last received damage (any source). Powers SelfTargeted condition for PvP / non-mob attackers.
	uint32_t last_attacker_id    = 0;  ///< id of the last source that damaged this shell (0 if unknown).
	int      last_damage_received = 0; ///< Largest single-hit damage received (reset on each hit). Powers damagedgt condition.
	uint16_t last_cast_skill_id   = 0; ///< Skill ID of the last offensive/buff skill successfully cast by this shell. Powers afterskill combo condition.
	uint16_t last_skill_used_on_me = 0; ///< Skill ID of the last skill used AGAINST this shell (set in on_shell_damaged from src unit_data). Powers skillused condition.
	t_tick   last_skill_used_on_me_tick = 0; ///< Tick when last_skill_used_on_me was recorded. Used for 3s stale window in SkillUsed condition eval.
	t_tick   safetywall_kited_since = 0; ///< Tick when target first became unreachable while standing on SC_SAFETYWALL (0 = not kited). Used to abandon the wall after 4s so the bot can chase.

	// -------------------------------------------------------------------------
	// Standalone movement / combat runtime state
	// (was stored in sd->ca / s_autocombat; moved here so the population
	//  engine runtime files are decoupled from sd->ca)
	// -------------------------------------------------------------------------

	/// Which subsystem owns the walk token this tick (Roam, Combat, etc.).
	MovementOwner  movement_owner             = MovementOwner::None;
	MovementOwnerReason movement_owner_reason = MovementOwnerReason::None;
	uint32         movement_decision_trace_id = 0;
	t_tick         movement_owner_since_tick  = 0;
	t_tick         movement_owner_lock_until_tick = 0;
	bool           movement_emitted_this_tick = false;
	bool           movement_conflict_logged_this_tick = false;
	uint32         owner_switch_count  = 0;
	uint32         owner_blocked_count = 0;
	uint32         owner_noop_count    = 0;

	/// Last movement timestamp (gettick()).
	t_tick         last_move = 0;

	/// Last-known position (map/x/y/dx/dy).
	s_pe_position  lastposition{};

	// --- Target / skill global cooldown ---
	int    target_id = 0;  ///< Active combat target (mob id or PC id in arena).
	t_tick skill_cd  = 0;  ///< Global skill cooldown — no new skill until tick >= skill_cd.

	// --- Pathfinding ---
	std::vector<std::tuple<int, int>> path{};
	t_tick path_last_calc_tick = 0;
	int    path_index          = 0;
	int    path_exec_fail_count = 0;
	t_tick last_move_fail      = 0;

	// --- Session state ---
	/// Incremented whenever shell state is fully reset (map-change, disconnect, stop).
	/// 0 is reserved as unset; initial value is 1.
	uint32 session_guard       = 1;
	uint32 client_addr         = 0; ///< Initialized flag: 0 = not yet init, 1 = initialized.
	uint32 unique_id           = 0; ///< char_id or block id, set at init.
	bool   manual_stop_requested = false;
	int    action_on_end         = 0;  ///< 0=none, 1=return-to-savepoint, 2=relog.
	uint16 last_start_reject_code_raw = 0; ///< Raw uint16 of last combat start reject code.
	/// Target IDs recently force-cleared, with the tick they were cleared.
	PEFlatMap<uint32, t_tick, PE_RECENT_TARGET_MAX> recently_cleared_targets{};

	// --- Local navigation FSM ---
	LocalNavState nav_state               = LocalNavState::DirectAdvance;
	int           nav_goal_x              = 0;
	int           nav_goal_y              = 0;
	int           nav_heading_x           = 0; ///< Normalized to -1/0/1.
	int           nav_heading_y           = 0;
	t_tick        next_nav_attempt_tick   = 0;
	t_tick        last_nav_progress_tick  = 0;
	uint8         blocked_heading_streak  = 0;
	std::array<CANavBlockedCell, PE_NAV_BLOCKED_CELLS_MAX> nav_blocked_cells{};
	uint8         nav_blocked_cells_cursor = 0;

	// --- Roam anti-oscillation memory ---
	int    roam_last_target_x  = 0;
	int    roam_last_target_y  = 0;
	int    roam_last_dir_x     = 0;
	int    roam_last_dir_y     = 0;
	t_tick roam_last_move_tick = 0;

	// --- Per-skill cooldown map ---
	PEFlatMap<uint16, t_tick, PE_SKILL_COOLDOWN_MAX> skill_next_use_tick{};

	// --- Active buff tracking ---
	std::vector<s_pe_active_buff> active_buffs{};

	// --- Mob tracker ---
	s_pe_mob_tracker mob_tracker{};

	// --- Movetype-3 hotspot seeder (flattened from Type3RuntimeState) ---
	std::array<Type3Hotspot, PE_TYPE3_HOTSPOTS_MAX> type3_hotspots{};

	// --- Movement emit tick (rate-limiter: one walk per tick) ---
	t_tick movement_emit_tick = 0;

	// --- Attack / target tracking ---
	t_tick last_attack             = 0;
	t_tick last_target_switch_tick = 0;
	t_tick target_lost_tick        = 0;
	int    move_fail_count         = 0;
	int    attack_fail_count       = 0;

	// --- Item pickup ---
	int    itempick_id  = 0;  ///< Block ID of item being picked up (0 = none).
	t_tick last_pick    = 0;  ///< Tick of last item pick attempt.
	t_tick last_teleport = 0; ///< Tick of last warp / teleport.

	// --- Skill fail tracking ---
	t_tick last_skill_fail    = 0;
	uint32 last_failed_target = 0;

	// --- Nearby mob tracking ---
	s_pe_mobs mobs{};

	// --- Detection cache ---
	s_pe_detection_cache detection_cache{};
};

#endif /* POPULATION_SHELL_HPP */
