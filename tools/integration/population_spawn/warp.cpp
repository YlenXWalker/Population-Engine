#include "test_common.hpp"

enum class LocalNavState { DirectAdvance, WallFollow };
struct map_session_data {
	int id = 5000001, m = 1, mapindex = 2, x = 40, y = 50;
	map_session_data* prev = nullptr;
	bool invisible = false;
	struct { uint32 account_id = 5000001; int party_id = 7; } status;
	struct { bool changemap = true, connect_new = true, warping = true, rewarp = true, debug_remove_map = true; } state;
	struct { t_tick canmove_tick = 9000; } ud;
	struct {
		uint32 companion_owner = 42;
		bool combat_active = true;
		int target_id = 100, sticky_target_id = 101, sticky_until = 999, last_attacker_id = 102, itempick_id = 103;
		std::vector<int> path{1, 2}, recently_cleared_targets{3}, nav_blocked_cells{4}, type3_hotspots{5};
		int path_index = 1, path_last_calc_tick = 10, path_exec_fail_count = 2, nav_blocked_cells_cursor = 2;
		LocalNavState nav_state = LocalNavState::WallFollow;
		int next_nav_attempt_tick = 500;
		struct { std::vector<int> tracked_mobs{99}; int last_scan = 100, map_id = 0; } mob_tracker;
		std::vector<int> detection_cache{1}, mobs{2};
		struct { int map = 1, x = 1, y = 2, dx = 5, dy = 6; } lastposition;
		t_tick last_teleport = 0;
		std::vector<int> skill_next_use_tick{222}, active_buffs{333};
	} pop;
};
struct map_data { int users = 0, users_pvp = 0; } maps[2];
struct { bool dynamic_mobs = true; } battle_config;
bool registered = true, block_failure = false, map_missing = false;
int blocks = 0, spawns = 0, mob_spawns = 0, party_updates = 0, hp_updates = 0, dirtied = 0, resets = 0;
map_session_data* current = nullptr;
std::vector<std::string> calls;
#define IS_POPULATION_ENGINE_ACCOUNT_ID(id) ((id) >= 5000000)
map_session_data* map_id2sd(int id) { return registered && current && id == current->id ? current : nullptr; }
map_data* map_getmapdata(int m) { return !map_missing && m >= 0 && m < 2 ? &maps[m] : nullptr; }
int map_addblock(map_session_data* sd) {
	++blocks; calls.push_back("attach");
	if (block_failure) return 1;
	sd->prev = sd; return 0;
}
void map_spawnmobs(int) { ++mob_spawns; calls.push_back("mobs"); }
bool pc_isinvisible(map_session_data* sd) { return sd->invisible; }
t_tick gettick() { return 1234; }
void population_shell_reset_movement_tick_state(map_session_data*, t_tick) { ++resets; }
void population_engine_shell_index_mark_dirty() { ++dirtied; }
void clif_spawn(map_session_data*) { ++spawns; calls.push_back("spawn"); }
void party_send_movemap(map_session_data*) { ++party_updates; calls.push_back("party"); }
void clif_party_hp(map_session_data&) { ++hp_updates; calls.push_back("hp"); }

// @production

void reset(map_session_data& sd) {
	sd = map_session_data{}; current = &sd;
	registered = true; block_failure = map_missing = false;
	maps[0] = {}; maps[1] = {};
	blocks = spawns = mob_spawns = party_updates = hp_updates = dirtied = resets = 0;
	calls.clear();
}

int main() {
	map_session_data sd;
	reset(sd);
	CHECK(population_engine_complete_shell_warp(nullptr));
	sd.status.account_id = 100;
	CHECK(population_engine_complete_shell_warp(&sd) && blocks == 0); // Real client owns ACK.
	reset(sd); registered = false;
	CHECK(population_engine_complete_shell_warp(&sd) && blocks == 0); // Initial spawn owns placement.
	reset(sd); sd.prev = &sd;
	CHECK(population_engine_complete_shell_warp(&sd) && blocks == 0); // No double attach or counting.
	reset(sd);
	CHECK(population_engine_complete_shell_warp(&sd));
	CHECK(sd.prev == &sd && !sd.state.changemap && !sd.state.warping && !sd.state.rewarp);
	CHECK(!sd.state.connect_new && !sd.state.debug_remove_map);
	CHECK(maps[1].users == 1 && maps[1].users_pvp == 1 && maps[0].users == 0);
	CHECK(blocks == 1 && spawns == 1 && mob_spawns == 1 && resets == 1 && dirtied == 1);
	CHECK((calls == std::vector<std::string>{"attach", "mobs", "spawn", "party", "hp"}));
	CHECK(sd.status.party_id == 7 && sd.pop.companion_owner == 42 && sd.pop.combat_active);
	CHECK(sd.pop.target_id == 0 && sd.pop.sticky_target_id == 0 && sd.pop.sticky_until == 0);
	CHECK(sd.pop.last_attacker_id == 0 && sd.pop.itempick_id == 0 && sd.pop.path.empty());
	CHECK(sd.pop.mob_tracker.tracked_mobs.empty() && sd.pop.mob_tracker.map_id == 1 && sd.pop.mob_tracker.last_scan == 0);
	CHECK(sd.pop.mobs.empty() && sd.pop.detection_cache.empty());
	CHECK(sd.pop.nav_state == LocalNavState::DirectAdvance && sd.pop.nav_blocked_cells.empty());
	CHECK(sd.pop.lastposition.map == 2 && sd.pop.lastposition.x == 40 && sd.pop.lastposition.y == 50);
	CHECK(sd.pop.lastposition.dx == 0 && sd.pop.lastposition.dy == 0 && sd.ud.canmove_tick == 1234);
	CHECK(sd.pop.skill_next_use_tick == std::vector<int>{222} && sd.pop.active_buffs == std::vector<int>{333});
	CHECK(population_engine_complete_shell_warp(&sd) && maps[1].users == 1 && blocks == 1);

	// Mirror the removal/counter decrement of pc_setpos, then attach again.
	for (int i = 0; i < 10; ++i) {
		--maps[sd.m].users; --maps[sd.m].users_pvp;
		sd.prev = nullptr; sd.state.warping = sd.state.changemap = true;
		sd.m = i % 2; sd.mapindex = sd.m + 1;
		CHECK(population_engine_complete_shell_warp(&sd));
		CHECK(sd.prev && !sd.state.warping && maps[0].users + maps[1].users == 1);
		CHECK(maps[0].users_pvp + maps[1].users_pvp == 1);
	}

	reset(sd); block_failure = true;
	CHECK(!population_engine_complete_shell_warp(&sd));
	CHECK(!sd.prev && maps[1].users == 0 && spawns == 0 && party_updates == 0 && dirtied == 0);
	block_failure = false;
	CHECK(population_engine_complete_shell_warp(&sd) && maps[1].users == 1);
	reset(sd); map_missing = true;
	CHECK(!population_engine_complete_shell_warp(&sd) && blocks == 0 && spawns == 0);
	reset(sd); sd.invisible = true; sd.status.party_id = 0; maps[1].users = 2;
	CHECK(population_engine_complete_shell_warp(&sd));
	CHECK(maps[1].users == 3 && maps[1].users_pvp == 0 && mob_spawns == 0 && party_updates == 0);
	return 0;
}