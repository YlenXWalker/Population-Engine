// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder
//
// Population engine: YAML-driven equipment, identity, and name profile types.

#ifndef POPULATION_CONFIG_HPP
#define POPULATION_CONFIG_HPP

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <common/database.hpp>
#include <common/mmo.hpp>

struct script_code;
struct PopulationVendorPlacement;

/// Optional per-job attack skill list (db/population_engine.yml `Skills:` sequence).
struct PopulationShellYamlSkill {
	uint16_t skill_id = 0;
	/// From YAML `Level` (default 10 per entry). 0 = do not force level via pc_skill; else ensure at least this level (capped by skill max).
	uint16_t level_cap = 0;
};

/// High-level shell behavior after spawn (db/population_engine.yml: Behavior).
enum class PopulationBehavior : uint8_t {
	None    = 0, ///< No wander, no combat
	Wander  = 1, ///< Random walk only
	Combat  = 2, ///< AutoCombat + wander (default)
	Support = 3, ///< Reserved for support AI (wander, no combat)
	Sit     = 4, ///< /sit emote, stays near spawn; minimal wander
	Social  = 5, ///< Wander + frequent chat + emotes
	Vendor  = 6, ///< Stand at spawn, display overhead shop message
	Guard   = 7, ///< Stand at spawn, attack on approach; return after combat
	Hangout = 8, ///< Permanent sitting post; retaliates against monsters only
};

/// Named gear set for GearSet: inheritance in db/population_engine.yml.
/// Only gear slot pools and skip_arrow are inherited; Script/identity/stats are not.
struct PopulationGearSet {
	std::vector<uint16_t> weapon_pool;
	std::vector<uint16_t> shield_pool;
	std::vector<uint16_t> head_top_pool;
	std::vector<uint16_t> head_mid_pool;
	std::vector<uint16_t> head_bottom_pool;
	std::vector<uint16_t> armor_pool;
	std::vector<uint16_t> garment_pool;
	std::vector<uint16_t> shoes_pool;
	std::vector<uint16_t> acc_l_pool;
	std::vector<uint16_t> acc_r_pool;
	bool skip_arrow = false;
};

/// Shell combat role for coordinated party play (db/population_engine.yml: Role).
enum class PopulationRoleType : int8_t {
	None     = 0, ///< No role bias (default)
	Tank     = 1, ///< Frontliner: intercepts enemies threatening low-HP allies; prefers melee
	Support  = 2, ///< Healer/buffer: deprioritizes direct combat; prioritizes heal/buff skills
	Attacker = 3, ///< DPS: maximizes offensive output
};

struct PopulationEngine {
	uint16_t job_id = 0;
	/// Equipment slot pools — a random entry is picked each spawn.
	/// YAML scalar 0 = empty slot; scalar item_id/AegisName = one fixed item;
	/// YAML sequence of item_ids/AegisNames = random pick per spawn.
	/// Parser validates each entry against the slot's equip flag at load time.
	std::vector<uint16_t> weapon_pool;
	std::vector<uint16_t> shield_pool;
	std::vector<uint16_t> head_top_pool;
	std::vector<uint16_t> head_mid_pool;
	std::vector<uint16_t> head_bottom_pool;
	std::vector<uint16_t> armor_pool;
	std::vector<uint16_t> garment_pool;
	std::vector<uint16_t> shoes_pool;
	std::vector<uint16_t> acc_l_pool;
	std::vector<uint16_t> acc_r_pool;
	/// Full raw script source (YAML: Script: |).  Run once at spawn via run_script
	/// to execute side-effect commands (setriding, setfalcon, setcart, setarrow).
	std::string script_str;
	/// Filtered script source: script_str with side-effect commands removed.
	/// Registered via pc_bonus_script_add so bonus commands (bonus bStr,
	/// bonus3 bAutoSpell, …) survive every future status_calc_pc call.
	/// Side-effect commands are excluded because they internally call
	/// status_calc_pc / pc_equipitem and would cause a "Double continuation" crash.
	std::string bonus_script_str;
	/// Compiled script block — pre-compiled for validation at load time.
	struct script_code* script = nullptr;
	/// If true, do not auto-equip arrows for bow-class YAML weapons (YAML: Arrow: false).
	bool skip_arrow = false;
	/// If true (default), run normal job skill tree after spawn. If false, only basic survival skills.
	bool grant_skill_tree = true;
	/// Non-empty when `Skills:` is a sequence (attack rotation + optional grants). Empty when `Skills:` is bool only.
	std::vector<PopulationShellYamlSkill> shell_attack_skill_yaml;
	PopulationBehavior behavior = PopulationBehavior::Combat;

	/// Per-map-category behavior overrides.
	/// When set (not None), replace `behavior` for shells spawned in that category.
	/// Allows e.g. Combat in fields but Wander in towns.
	PopulationBehavior town_behavior    = PopulationBehavior::None;
	PopulationBehavior field_behavior   = PopulationBehavior::None;
	PopulationBehavior dungeon_behavior = PopulationBehavior::None;

	/// Guard behavior: aggro range in cells (0 = use default 5).
	uint8_t guard_range = 0;

	/// Combat role for party coordination. Influences skill selection priority in combat.
	PopulationRoleType role_type = PopulationRoleType::None;

	/// Vendor behavior: overhead message text (empty = job-default string).
	std::string vendor_message;
	/// Optional key into db/population_vendors.yml (VendorKey:). Empty = built-in default stock.
	std::string vendor_key;

	/// Identity: -1 / unset = use engine defaults (random or job rule).
	int16_t str_min = -1, str_max = -1;
	int16_t agi_min = -1, agi_max = -1;
	int16_t vit_min = -1, vit_max = -1;
	int16_t intl_min = -1, intl_max = -1; // YAML key "Int"
	int16_t dex_min = -1, dex_max = -1;
	int16_t luk_min = -1, luk_max = -1;
	int16_t base_level_min = -1, base_level_max = -1;
	int16_t job_level_min = -1, job_level_max = -1;
	/// -1 = auto (job rule + random); 0 = female; 1 = male
	int8_t sex_override = -1;
	int16_t hair_min = -1, hair_max = -1;
	int16_t hair_color_min = -1, hair_color_max = -1;
	int16_t cloth_color_min = -1, cloth_color_max = -1;
	std::string name_profile;
	std::string name_prefix;
	std::string name_suffix;
	/// Key in db/population_chat.yml (ChatProfile rows); empty = no random overhead lines
	std::string chat_profile;
	/// Shell behavior flags — bitfield of PSF_* values below.
	uint32_t flags = 0;
	/// Name of the Profile: this entry inherited from (empty if none). Used by
	/// PopulationEngineDatabase::jobs_with_profile() so the spawn loader can
	/// translate a profile name from population_spawn.yml into a list of jobs.
	std::string source_profile_name;
	~PopulationEngine();
};

/// Population shell flags (bitfield stored in PopulationEngine::flags and sd->pop.flags).
namespace PSF {
	constexpr uint32_t Mortal      = 1u << 0; ///< Shell can take damage and die (default: immortal)
	constexpr uint32_t AttackOnly  = 1u << 1; ///< Skip skills/buffs; only basic attack
	constexpr uint32_t SkillOnly   = 1u << 2; ///< Skip basic attack; only skill attacks
	constexpr uint32_t FleeOnLow   = 1u << 3; ///< Flee when HP drops below 30% (squishy jobs)
	constexpr uint32_t Kite        = 1u << 4; ///< Maintain max skill range instead of closing to melee
	constexpr uint32_t BossAvoid     = 1u << 5; ///< Avoid targeting MVP/boss monsters (non-tank roles)
	constexpr uint32_t CombatActive  = 1u << 6; ///< Runtime: shell fully set up and not yet in teardown
}

struct PopulationNameProfile {
	enum class Strategy : uint8_t { None = 0, Syllables, AdjectiveNoun, PickOne, BotIndex, PrefixNumber };
	Strategy strategy = Strategy::None;
	std::vector<std::string> syllables_start;
	std::vector<std::string> syllables_mid;
	std::vector<std::string> syllables_end;
	std::vector<std::string> adjectives;
	std::vector<std::string> nouns;
	std::vector<std::string> pool;
	int min_len = 2;
	int max_len = 22;
};

/// One item in a vendor's predefined stock list (db/population_vendors.yml).
struct PopulationVendorStock {
	t_itemid nameid = 0;
	int16_t  amount = 1;
	uint32_t price  = 0; ///< 0 = auto (item_data.value_buy)
};

/// A named vendor configuration entry from db/population_vendors.yml.
struct PopulationVendorEntry {
	std::string key;
	std::string title;           ///< Overhead vend title (empty = "Shop")
	bool        dynamic = false; ///< Derive stock from map mob drop tables at spawn time
	int         max_slots = 12;  ///< Cap vend slots (MC_VENDING lv10 = 12)
	uint32_t    price_multiplier = 100; ///< % of item sell value for dynamic entries
	std::vector<PopulationVendorStock> stock;

	// Dynamic-mode source pool: which maps to scan for mob drops.
	// Priority: explicit source_maps > source_category auto-discovery > spawn map (legacy).
	std::vector<std::string> source_maps; ///< Explicit map list (overrides auto-discovery).
	std::string source_category;          ///< "dungeon" | "field" | "both" (empty = legacy/spawn map only).
	bool        randomize_per_shell = false; ///< If true, each shell picks ONE map; else merge all maps.

	// Item type filters for dynamic-mode item inclusion.
	bool allow_equipment = false; ///< Include items where item_data.equip != 0.
	bool allow_cards     = false; ///< Include IT_CARD items.
	bool allow_etc       = true;  ///< Include IT_ETC items.
	bool allow_usable    = true;  ///< Include IT_HEALING / IT_USABLE / IT_DELAYCONSUME.

	// MaxAmount: per-item stack-size randomization range for dynamic-mode stock.
	// When dyn_amount_max > 0, each generated stockpile rolls a random integer in
	// [dyn_amount_min, dyn_amount_max].  Equipment is always capped to 1 (non-stackable).
	int dyn_amount_min = 0; ///< 0 = use built-in default (30).
	int dyn_amount_max = 0; ///< 0 = use built-in default (30).
};

/// Per-map vendor placement constraint (from db/population_engine.yml VendorPlacement: block).
struct PopulationVendorPlacement {
	std::string map;             ///< Map name this entry applies to.
	int min_spacing = 0;         ///< Minimum cells between two vendor shells (0 = no spacing check).
	int max_vendors = 0;         ///< Hard cap on simultaneous vendor shells on this map (0 = unlimited).
	int16_t area_x1 = -1, area_y1 = -1, area_x2 = -1, area_y2 = -1; ///< Optional bounding box (-1 = whole map).
};

// Population skill DB types

// Population shell skill database — per-job skill overrides loaded from db/population_skill_db.yml.
// Access via population_skill_db() (declared in population_config.hpp).

namespace expanded_ai { class ExpandedCondition; }

/// Condition type for a population shell skill entry.
/// Numeric thresholds use cond_value_num (0–100 or cell count).
/// Status-name conditions use cond_value_str (e.g. "stun", "freeze").
enum class PopSkillCondition : uint8_t {
	Always          = 0,  ///< No condition — always eligible
	HpBelow         = 1,  ///< Self HP% < cond_value_num
	SpBelow         = 2,  ///< Self SP% < cond_value_num
	HpAbove         = 3,  ///< Self HP% > cond_value_num
	EnemyHpBelow    = 4,  ///< Target HP% < cond_value_num
	EnemyHpAbove    = 5,  ///< Target HP% > cond_value_num
	EnemyStatus     = 6,  ///< Target has SC named in cond_value_str
	NotEnemyStatus  = 7,  ///< Target does NOT have SC named in cond_value_str
	SelfStatus      = 8,  ///< Self has SC named in cond_value_str
	NotSelfStatus   = 9,  ///< Self does NOT have SC named in cond_value_str
	DistanceBelow   = 10, ///< Distance to target <= cond_value_num cells
	DistanceAbove   = 11, ///< Distance to target > cond_value_num cells
	EnemyCountNearby= 12, ///< >= cond_value_num enemies within detection range
	MapZone         = 13, ///< Shell spawned in category: 1=town,2=field,3=dungeon
	MeleeAttacked   = 14, ///< A melee-range mob is targeting this shell
	RangeAttacked   = 15, ///< A ranged mob is targeting this shell
	AllyHpBelow     = 16, ///< Any nearby ally (BL_PC in skill range) has HP% < cond_value_num
	AllyStatus      = 17, ///< Any nearby ally has SC named in cond_value_str
	NotAllyStatus   = 18, ///< No nearby ally has SC named in cond_value_str (or self lacks it)
	HasSphere       = 19, ///< Shell has >= cond_value_num spirit spheres
	// -- Reactive conditions --
	EnemyHidden         = 20, ///< Any tracked enemy has Hiding/Cloaking/Chase Walk
	EnemyCasting        = 21, ///< Current target is casting a skill (ud.skilltimer active)
	EnemyCastingGround  = 22, ///< Current target is casting a ground-targeted skill
	CellHasSkillUnit    = 23, ///< A hostile skill unit exists on the shell's cell (traps, ground effects)
	SelfTargeted        = 24, ///< Shell was damaged within the last N seconds (Value: window in seconds, default 3) OR a mob in the tracker is targeting this shell. Works for PvP / non-mob attackers.
	EnemyElement        = 25, ///< Target's defense element matches cond_value_num (ELE_* constant)
	EnemyRace           = 26, ///< Target's race matches cond_value_num (RC_* constant)
	AllyCountNearby     = 27, ///< >= cond_value_num allied PCs within 9 cells
	EnemyIsBoss         = 28, ///< Target has MD_BOSS mode
	SpAbove             = 29, ///< Self SP% > cond_value_num
	Expanded            = 30, ///< Boolean tree built from `Condition:` map/sequence (see expanded_ai/). cond_value_num/cond_value_str unused; tree stored on `expanded` shared_ptr.
	SelfBeingCastOn     = 31, ///< Any nearby unit is currently casting (skilltimer != INVALID_TIMER) and targeting this shell, OR casting a ground skill within `cond_value_num` cells (default 3) of the shell. Powers proactive dodge logic.
	// -- Combo / reactive conditions --
	AfterSkill          = 32, ///< Shell's previous successful cast matches cond_value_str skill name. Enables sequential combos (e.g. Frostnova → Thunder Storm). cond_sc_resolved holds resolved skill ID.
	DamagedGt           = 33, ///< Shell received a single hit larger than (cond_value_num % of max HP). E.g. CondValue:10 triggers when hit by >10% max HP in one blow.
	SkillUsed           = 34, ///< A specific skill (cond_value_str) was used against this shell within the last 3 seconds. Mirrors mob_skill_db `skillused`. cond_sc_resolved holds resolved skill ID.
};

/// One skill entry from population_skill_db.yml.
struct s_pop_skill_entry {
	uint16_t skill_id  = 0;      ///< Resolved skill ID (0 = invalid/skip)
	uint16_t skill_lv  = 1;      ///< Skill level to cast
	uint16_t rate      = 10000;  ///< 1–10000 (per 10000); 0 = disabled
	uint8_t  state     = 0;      ///< 0=any, 1=has_target(combat), 2=no_target(roam)
	uint8_t  target    = 0;      ///< 0=current mob, 1=self, 2=ally (nearest BL_PC in range)
	uint8_t  around_range = 0;   ///< If >0, place ground/trap skills at a random cell within this radius (mirrors mob_skill_db around1–4 = around self, around5–8 = around target when around_target=true)
	bool     around_target = false; ///< When true, around_range centres on the enemy (mob_skill_db around5–8); when false (default), centres on self (mob_skill_db around1–4).
	uint32_t cooldown_ms = 0;    ///< Per-skill cooldown in ms after a successful cast (0 = no cooldown beyond global skill_cd)
	PopSkillCondition condition = PopSkillCondition::Always;
	uint8_t  cond_value_num = 0; ///< Numeric threshold (HP%, SP%, cell count, zone id)
	std::string cond_value_str;  ///< SC name for status conditions (e.g. "stun")
	std::shared_ptr<expanded_ai::ExpandedCondition> expanded; ///< Set when condition == Expanded; null otherwise.
};

// Population spawn DB types

// Per-profile autosummon spawn lists (db/population_spawn.yml).
// Access via population_spawn_db() (declared in population_config.hpp).

/// One autosummon entry: which maps to fill and how many shells per category.
/// Keyed by Profile name (matches a Profile: defined in population_engine.yml);
/// the spawn loader resolves the profile to the list of jobs that inherit
/// from it and distributes shells across them at autosummon time.
struct PopulationSpawnEntry {
	struct Post { std::string map; int16_t x = 0, y = 0; };
	std::vector<Post> posts; ///< Permanent hangout locations, independent of the daily cycle
	std::string profile_name;
	std::vector<std::string> towns;
	int32_t towns_population = 0;
	int32_t towns_max_per_map = 0; ///< Hard cap per individual town map (0 = no cap).
	std::vector<std::string> fields;
	int32_t fields_population = 0;
	int32_t fields_max_per_map = 0; ///< Hard cap per individual field map (0 = no cap).
	std::vector<std::string> dungeons;
	int32_t dungeons_population = 0;
	int32_t dungeons_max_per_map = 0; ///< Hard cap per individual dungeon map (0 = no cap).
};

class PopulationSpawnDatabase : public TypesafeYamlDatabase<std::string, PopulationSpawnEntry> {
public:
	PopulationSpawnDatabase();
	const std::string getDefaultLocation() override;
	uint64 parseBodyNode(const ryml::NodeRef& node) override;
};

// Population config database API

// Population engine: battle_config + db paths, and YAML database classes.

/// Hat effect id from `population_engine_hat_effect` (0 = off). Caller validates HAT_EF range.
int population_config_shell_hat_effect_id();
/// Full paths: `db_path` + "/" + file (same as db/population_*.yml when db_path is `db`).
std::string population_config_db_path_equipment_yaml();
std::string population_config_db_path_pvp_yaml();
std::string population_config_db_path_vendor_pop_yaml();
std::string population_config_db_path_shared_yaml();
std::string population_config_db_path_chat_yaml();
std::string population_config_db_path_names_yaml();
std::string population_config_db_path_skill_yaml();
std::string population_config_db_path_spawn_yaml();
std::string population_config_db_path_vendors_yaml();

// YAML database classes

class PopulationNamesDatabase : public YamlDatabase {
	std::unordered_map<std::string, std::shared_ptr<PopulationNameProfile>> profiles;

public:
	PopulationNamesDatabase();
	void clear() override;
	const std::string getDefaultLocation() override;
	uint64 parseBodyNode(const ryml::NodeRef& node) override;
	const PopulationNameProfile* find_profile(const std::string& key) const;
	const PopulationNameProfile* find_profile_or_default(const std::string& key) const;
	size_t profile_count() const;
};

class PopulationChatDatabase : public YamlDatabase {
	std::unordered_map<std::string, std::vector<std::string>> categories_;
	std::unordered_map<std::string, std::vector<std::string>> profile_pools_;

public:
	PopulationChatDatabase();
	void clear() override;
	const std::string getDefaultLocation() override;
	uint64 parseBodyNode(const ryml::NodeRef& node) override;
	void loadingFinished() override;
	const std::vector<std::string>* pool_for_profile(const std::string& key) const;
	const std::vector<std::string>* lines_for_category(const std::string& category) const;
	size_t profile_count() const;
	size_t category_count() const;
};

/// Source enum used by sd->pop.db_source so runtime lookups can find the
/// correct database for a given shell. Each value matches one of the YAML
/// files loaded into a separate PopulationEngineDatabase instance.
enum class PopulationDbSource : uint8_t {
	Main   = 0, ///< db/population_engine.yml — town/field/dungeon shells
	Pvp    = 1, ///< db/population_pvp.yml      — PvP arena shells
	Vendor = 2, ///< db/population_vendor_pop.yml — vendor shells
};

class PopulationEngineDatabase : public TypesafeYamlDatabase<uint16_t, PopulationEngine> {
	// Vendor profiles may share a job; the job-indexed compatibility view cannot
	// represent their identities. Retain each profile/job pair separately.
	std::vector<std::shared_ptr<PopulationEngine>> m_shell_profiles;
	bool m_validationError = false;
	std::unordered_map<std::string, PopulationGearSet> m_gear_sets;
	std::unordered_map<std::string, std::shared_ptr<PopulationEngine>> m_profiles;
	std::vector<uint16_t> m_arena_job_pool;
	/// Per-instance YAML basename. Set by the constructor; getDefaultLocation()
	/// returns db_path + "/" + this. Allows multiple instances of this class to
	/// load different YAML files (main, pvp, vendor, shared templates).
	std::string m_filename_basename;
	/// Optional shared template source. When set, GearSet: and Profile: lookups
	/// during parseBodyNode fall back to this database if not found locally.
	/// Used so the three job DBs can share gear sets / profiles defined in
	/// db/population_gear_sets.yml.
	const PopulationEngineDatabase* m_shared_source = nullptr;
	/// When true, parseBodyNode rejects Job: entries (used for the shared
	/// templates DB which should only contain GearSetName, Profile, ArenaJobPool).
	bool m_jobs_rejected = false;
	/// When true, loadingFinished() auto-fills m_arena_job_pool from every loaded
	/// job ID if the YAML did not provide an explicit ArenaJobPool block. Used by
	/// the PvP DB so the arena pool is implicitly the set of jobs in that file.
	bool m_auto_arena_pool = false;

	ryml::NodeRef warnAnchorForJob(const ryml::NodeRef& node);
	void markInvalidItem(const ryml::NodeRef& node, const char* yaml_key, uint16_t job_id, uint16_t bad_id);
	bool parseOptionalIntRange(const ryml::NodeRef& node, const std::string& key, int16_t& out_min, int16_t& out_max, uint16_t job_id);
	void readUInt16FirstKey(const ryml::NodeRef& node, std::initializer_list<const char*> keys, uint16_t& out);
	/// Parse an equipment slot pool from a YAML node.
	/// Accepts scalar 0 (empty), scalar item_id or AegisName (single item), or sequence of items.
	/// Validates each resolved item against required_flag (item_data.equip & required_flag must be true).
	/// Warns and skips entries that fail item_db lookup or slot validation.
	void parseEquipSlotPool(const ryml::NodeRef& node, std::initializer_list<const char*> keys,
													uint32_t required_flag, std::vector<uint16_t>& pool, uint16_t job_id);

	/// Look up a gear set by name. Falls back to m_shared_source if not local.
	const PopulationGearSet* find_gear_set(const std::string& name) const;
	/// Look up a profile template by name. Falls back to m_shared_source if not local.
	const PopulationEngine*  find_profile(const std::string& name) const;

public:
	const std::vector<std::shared_ptr<PopulationEngine>>& shell_profiles() const { return m_shell_profiles; }
	/// Default ctor uses the main "population_engine.yml" filename.
	PopulationEngineDatabase();
	/// Use a non-default YAML basename (e.g. "population_pvp.yml").
	/// `auto_arena_pool` = true makes loadingFinished() seed m_arena_job_pool
	/// from every loaded job ID when no explicit ArenaJobPool block was parsed.
	explicit PopulationEngineDatabase(const char* yaml_basename, bool reject_jobs = false,
																		bool auto_arena_pool = false);
	const std::string getDefaultLocation() override;
	void clear() override;
	void loadingFinished() override;
	uint64 parseBodyNode(const ryml::NodeRef& node) override;
	const std::vector<uint16_t>& arena_job_pool() const { return m_arena_job_pool; }
	/// Return all loaded job IDs whose entry inherited from the named Profile.
	std::vector<uint16_t> jobs_with_profile(const std::string& profile_name);
	/// Set after construction; pass the shared-templates DB so parseBodyNode
	/// can fall back for unknown GearSet:/Profile: references.
	void set_shared_source(const PopulationEngineDatabase* src) { m_shared_source = src; }
};

class PopulationSkillDatabase : public YamlDatabase {
	std::unordered_map<uint16_t, std::vector<s_pop_skill_entry>> jobs_;

public:
	PopulationSkillDatabase();
	void clear() override;
	const std::string getDefaultLocation() override;
	uint64 parseBodyNode(const ryml::NodeRef& node) override;
	const std::vector<s_pop_skill_entry>* find(uint16_t job_id) const;
	size_t job_count() const;
};

class PopulationVendorDatabase : public YamlDatabase {
	std::unordered_map<std::string, PopulationVendorEntry> entries_;
	/// Derived index: Map name -> placement constraint, populated from per-vendor
	/// VendorPlacement: blocks during parseBodyNode. If multiple vendor entries name
	/// the same Map, the last one parsed wins (a warning is emitted).
	std::unordered_map<std::string, PopulationVendorPlacement> placements_by_map_;
	std::unordered_map<std::string, std::unordered_map<std::string, PopulationVendorPlacement>> placements_by_vendor_;

public:
	PopulationVendorDatabase();
	void clear() override;
	const std::string getDefaultLocation() override;
	uint64 parseBodyNode(const ryml::NodeRef& node) override;
	const PopulationVendorEntry* find(const std::string& key) const;
	const PopulationVendorPlacement* placement(const std::string& key, const std::string& map) const {
		auto vendor = placements_by_vendor_.find(key);
		if (vendor == placements_by_vendor_.end()) return nullptr;
		auto entry = vendor->second.find(map);
		return entry == vendor->second.end() ? nullptr : &entry->second;
	}
	const std::unordered_map<std::string, std::unordered_map<std::string, PopulationVendorPlacement>>& placements() const { return placements_by_vendor_; }
	size_t entry_count() const;
	const PopulationVendorPlacement* vendor_placement_for_map(const std::string& map_name) const {
		auto it = placements_by_map_.find(map_name);
		return it != placements_by_map_.end() ? &it->second : nullptr;
	}
	bool any_vendor_placements() const { return !placements_by_map_.empty(); }
	const std::unordered_map<std::string, PopulationVendorPlacement>& vendor_placements() const { return placements_by_map_; }
};

PopulationNamesDatabase& population_names_db();
PopulationChatDatabase& population_chat_db();
PopulationEngineDatabase& population_engine_db();
PopulationEngineDatabase& population_pvp_db();
PopulationEngineDatabase& population_vendor_pop_db();
PopulationEngineDatabase& population_shared_db();
PopulationSkillDatabase& population_skill_db();
PopulationSpawnDatabase& population_spawn_db();
PopulationVendorDatabase& population_vendor_db();

/// Pick the right job DB for a shell based on its source tag.
PopulationEngineDatabase& population_engine_db_for(PopulationDbSource src);

/// Lookup `job_id` in main, then PvP, then vendor DB. Optionally reports which
/// DB owned the entry via `out_src` so the caller can tag the spawned shell.
class map_session_data;
std::shared_ptr<PopulationEngine> population_engine_find_any(uint16_t job_id, PopulationDbSource* out_src = nullptr);
/// Resolve the DB that owns this shell (sd->pop.db_source). Returns the main
/// DB for null/non-shell sd so existing call sites stay safe.
PopulationEngineDatabase& population_engine_db_for_shell(const map_session_data* sd);

const std::vector<std::string>& population_yaml_name_global_prefixes();
const std::vector<std::string>& population_yaml_name_syl_start();
const std::vector<std::string>& population_yaml_name_syl_mid();
const std::vector<std::string>& population_yaml_name_syl_end();
const std::vector<std::string>& population_yaml_name_adjectives();
const std::vector<std::string>& population_yaml_name_nouns();
bool population_yaml_name_hits_blocklist(const std::string& name_lower);

#endif /* POPULATION_CONFIG_HPP */
