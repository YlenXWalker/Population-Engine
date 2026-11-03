#include "test_common.hpp"
#include <bitset>
#include <cstdlib>
#include <new>

using uint64 = uint64_t;
constexpr int INVALID_TIMER = -1, CELL_CHKPASS = 1, SEX_MALE = 1, SEX_FEMALE = 0, W_FIST = 0;
constexpr int PC_PERM_MAX = 2, PC_PERM_ALL_SKILL = 0;
constexpr int MIN_HAIR_STYLE = 0, MAX_HAIR_STYLE = 99, MIN_HAIR_COLOR = 0, MAX_HAIR_COLOR = 99;
constexpr int MIN_CLOTH_COLOR = 0, MAX_CLOTH_COLOR = 99, COLOR_YELLOW = 0, AREA_WOS = 0;
constexpr uint32 POPULATION_ENGINE_ACCOUNT_ID_BASE = 1000, POPULATION_ENGINE_CHAR_ID_BASE = 2000;
#define PE_PERF_SCOPE(name) ((void)0)
int allocations = 0, destroyed = 0, released = 0;
#define CREATE(pointer, type, count) do { pointer = static_cast<type*>(std::calloc(count, sizeof(type))); ++allocations; } while (false)
enum class PopulationBehavior : uint8_t { None, Wander, Combat, Support, Sit, Social, Vendor, Guard, Hangout };
enum class PopulationDbSource : uint8_t { Primary, Secondary };
struct script_code {} script;
struct PopulationEngine {
	int16_t hair_min = -1, hair_max = -1, hair_color_min = -1, hair_color_max = -1, cloth_color_min = -1, cloth_color_max = -1;
	std::string vendor_message;
} cfg;
struct s_player_group {
	int id = 1, level = 99; bool log_commands = true;
	bool has_permission(int) const { return true; }
};
struct mmo_charstatus {
	uint32 char_id, account_id; int sex, class_; char name[32];
	int hair, hair_color, clothes_color, body, weapon, shield, head_top, head_mid, head_bottom, robe;
	uint32 option;
	struct { char map[32]; int x, y; } last_point, save_point;
};
struct map_session_data {
	struct { int respawn_timer = 0; } pop;
	int id = 42, group_id = 1;
	std::shared_ptr<s_player_group> group;
	std::bitset<PC_PERM_MAX> permissions{3};
	mmo_charstatus status{};
	uint64 class_ = 0;
};
bool walkable = true, valid_job = true, valid_name = true, placed = true, combat_ready = true;
bool group_available = true, valid = true;
PopulationBehavior behavior = PopulationBehavior::Combat;
std::vector<std::string> calls;
int requested_map = 7;
bool map_getcell(int, int, int, int) { return walkable; }
template<class V, class L, class H> V cap_value(V value, L low, H high) {
	return value < low ? static_cast<V>(low) : value > high ? static_cast<V>(high) : value;
}
int16_t population_roll_closed_range(int16_t low, int16_t) { return low; }
std::string generate_population_pc_name(uint32_t, const PopulationEngine*) { return "Pop42"; }
t_tick gettick() { return 500; }
void pc_setnewpc(map_session_data*, uint32 account, uint32 character, int, t_tick tick, int sex, int) {
	valid &= account == 1042 && character == 2042 && tick == 500 && sex == SEX_MALE;
	calls.push_back("newpc");
}
void pc_group_pc_load(map_session_data* sd) { if (group_available) sd->group = std::make_shared<s_player_group>(); }
void safestrncpy(char* dest, const char* source, std::size_t size) { std::snprintf(dest, size, "%s", source); }
uint64 pc_jobid2mapid(uint16_t) { return valid_job ? 10 : static_cast<uint64>(-1); }
struct { bool exists(uint16_t) { return valid_job; } } job_db;
int32 pc_mapid2jobid(uint64, int) { return 10; }
char get_job_required_sex(uint16_t) { return '\0'; }
const char* map_mapid2mapname(int) { return valid_name ? "prontera" : nullptr; }
void dispose(map_session_data* sd) { sd->~map_session_data(); std::free(sd); }
void population_engine_destroy_failed_spawn(map_session_data* sd) { ++destroyed; calls.push_back("destroy"); dispose(sd); }
void population_engine_shell_release(map_session_data* sd) { ++released; calls.push_back("release"); dispose(sd); }
void population_engine_initialize_shell_stats(map_session_data*, const PopulationEngine*) { calls.push_back("stats"); }
void population_engine_initialize_shell_runtime(map_session_data*, t_tick tick, const PopulationEngine*, uint8_t category) {
	valid &= tick == 500 && category == 2; calls.push_back("runtime");
}
bool population_engine_place_shell(map_session_data* sd, const char* map, int x, int y, uint32_t index) {
	valid &= std::string(map) == "prontera" && x == 10 && y == 20 && index == 42;
	calls.push_back("place"); if (!placed) population_engine_destroy_failed_spawn(sd); return placed;
}
void population_engine_initialize_shell_status(map_session_data*, uint16_t job, uint32_t index) {
	valid &= job == 10 && index == 42; calls.push_back("status");
}
void population_engine_grant_shell_skills(map_session_data*, const PopulationEngine*, uint32_t index) {
	valid &= index == 42; calls.push_back("skills");
}
void population_engine_equip_shell_weapon(map_session_data*, uint16_t weapon, uint32_t index) {
	valid &= weapon == 1000 && index == 42; calls.push_back("weapon");
}
void population_engine_equip_shell_ammo(map_session_data*, uint16_t weapon, bool skip, script_code* init, uint32_t index) {
	valid &= weapon == 1000 && !skip && init == &script && index == 42; calls.push_back("ammo");
}
void population_engine_equip_shell_armor(map_session_data*, uint16_t shield, uint16_t top, uint16_t mid, uint16_t bottom, uint16_t garment) {
	valid &= shield == 1001 && top == 1002 && mid == 1003 && bottom == 1004 && garment == 1005;
	calls.push_back("armor");
}
void population_engine_stock_shell_supplies(map_session_data*, const PopulationEngine*, uint16_t job, uint32_t index) {
	valid &= job == 10 && index == 42; calls.push_back("supplies");
}
void population_engine_apply_shell_scripts(map_session_data*, script_code* init, const PopulationEngine*) {
	valid &= init == &script; calls.push_back("scripts");
}
void population_engine_publish_shell_appearance(map_session_data*) { calls.push_back("appearance"); }
bool population_engine_combat_try_start(map_session_data*) { calls.push_back("combat"); return combat_ready; }
void clif_spawn(map_session_data*) { calls.push_back("spawn"); }
PopulationBehavior population_engine_start_shell_behavior(map_session_data*, int16_t map, int x, int y, uint8_t category, PopulationDbSource source, const PopulationEngine*) {
	valid &= map == 7 && x == 10 && y == 20 && category == 2 && source == PopulationDbSource::Secondary;
	calls.push_back("behavior"); return behavior;
}
void pc_setsit(map_session_data*) { calls.push_back("sit"); }
void clif_sitting(map_session_data&) { calls.push_back("sitting_packet"); }
int color_table[] = {0};
void clif_messagecolor(map_session_data*, int, const char*, bool, int) { calls.push_back("message"); }
void population_engine_start_shell_vendor(map_session_data*, const PopulationEngine*) { calls.push_back("vendor"); }

// @production

map_session_data* spawn() {
	return population_engine_spawn_shell(requested_map, 10, 20, 42, 10, 'M', 5, 6,
		1000, 1001, 1002, 1003, 1004, 32, 7, 1005, &script, false, &cfg, 2, PopulationDbSource::Secondary);
}
void reset() { calls.clear(); allocations = destroyed = released = 0; }

int main() {
	auto* sd = spawn();
	CHECK(sd && valid && sd->pop.respawn_timer == INVALID_TIMER && !sd->permissions.test(PC_PERM_ALL_SKILL));
	CHECK(sd->status.account_id == 1042 && sd->status.char_id == 2042 && sd->status.class_ == 10);
	CHECK(std::string(sd->status.name) == "Pop42" && sd->status.hair == 5 && sd->status.hair_color == 6);
	CHECK(sd->status.clothes_color == 7 && sd->status.weapon == W_FIST && sd->status.option == 32);
	CHECK(std::string(sd->status.last_point.map) == "prontera" && sd->status.last_point.x == 10 && sd->status.save_point.y == 20);
	CHECK((calls == std::vector<std::string>{"newpc", "stats", "runtime", "place", "status", "skills", "weapon", "ammo", "armor", "supplies", "scripts", "appearance", "combat", "spawn", "behavior"}));
	dispose(sd); reset();
	requested_map = -1; CHECK(!spawn() && allocations == 0 && calls.empty()); requested_map = 7;
	walkable = false; CHECK(!spawn() && allocations == 0); walkable = true;
	valid_job = false; CHECK(!spawn() && destroyed == 1 && released == 0); valid_job = true; reset();
	valid_name = false; CHECK(!spawn() && destroyed == 1 && calls.back() == "destroy"); valid_name = true; reset();
	placed = false; CHECK(!spawn() && destroyed == 1 && released == 0 && calls.back() == "destroy"); placed = true; reset();
	combat_ready = false; CHECK(!spawn() && destroyed == 0 && released == 1 && calls.back() == "release");
	CHECK(std::find(calls.begin(), calls.end(), "spawn") == calls.end()); combat_ready = true; reset();
	behavior = PopulationBehavior::Vendor; cfg.vendor_message = "Shop"; group_available = false;
	sd = spawn(); CHECK(sd && valid && sd->group && sd->group->id == 0 && !sd->group->log_commands);
	CHECK((std::vector<std::string>(calls.end() - 4, calls.end()) == std::vector<std::string>{"sit", "sitting_packet", "message", "vendor"}));
	dispose(sd); reset(); behavior = PopulationBehavior::Sit;
	sd = spawn(); CHECK(sd && calls.back() == "sitting_packet" && std::find(calls.begin(), calls.end(), "vendor") == calls.end());
	dispose(sd);
	return 0;
}
