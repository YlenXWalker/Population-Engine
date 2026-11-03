#include "test_common.hpp"
#include <cstdarg>

enum { BL_PC, USW_FORCE_STOP = 1, USW_RELEASE_TARGET = 2 };
enum class MovementOwner { Combat };
struct map_session_data {
	int id = 0, m = 0, x = 20, y = 20;
	map_session_data* prev = this;
	bool dead = false, walking = false;
	struct { bool active = true, warping = false; } state;
	struct { uint32 char_id = 42; } status;
	struct { int arena_team = 1; uint32 arena_owner = 42; int target_id = 0; t_tick arena_group_move_tick = 0; bool arena_eliminated = false; } pop;
};
using block_list = map_session_data;
map_session_data owner, leader, follower, enemy;
std::vector<map_session_data*> g_population_engine_pcs;
int walks = 0, stops = 0, attack_stops = 0;
map_session_data* destination = nullptr;
t_tick now = 1000;
bool online = true, movable = true;
map_session_data* map_charid2sd(uint32 cid) { return online && cid == owner.status.char_id ? &owner : nullptr; }
int population_arena_find_player_cb(block_list*, va_list) { return 0; }
void map_foreachinmap(int (*)(block_list*, va_list), int, int, map_session_data** result) { *result = &owner; }
bool pc_isdead(map_session_data* sd) { return sd->dead; }
void population_shell_target_change(map_session_data* sd, int id) { sd->pop.target_id = id; }
void unit_stop_attack(map_session_data*) { ++attack_stops; }
bool unit_is_walking(map_session_data* sd) { return sd->walking; }
void unit_stop_walking(map_session_data* sd, int) { ++stops; sd->walking = false; }
int distance_bl(map_session_data* a, map_session_data* b) { return std::max(std::abs(a->x - b->x), std::abs(a->y - b->y)); }
t_tick gettick() { return now; }
void population_shell_reset_movement_tick_state(map_session_data*, t_tick) {}
bool population_shell_can_emit_movement(map_session_data*, MovementOwner, const char*) { return movable; }
void unit_walktobl(map_session_data* sd, map_session_data* target, int, int) { ++walks; destination = target; sd->walking = true; }

// @production

void reset() {
	owner = {}; leader = {}; follower = {}; enemy = {};
	owner.prev = &owner; leader.prev = &leader; follower.prev = &follower; enemy.prev = &enemy;
	owner.id = 1; owner.x = 50; owner.pop.arena_team = 0;
	leader.id = 100; follower.id = 101; enemy.id = 200; enemy.pop.arena_team = 2;
	g_population_engine_pcs = {&enemy, &follower, &leader};
	walks = stops = attack_stops = 0; now = 1000; online = movable = true; destination = nullptr;
}
int main() {
	reset();
	CHECK(population_engine_arena_group_step(&leader) && destination == &owner && walks == 1);
	CHECK(population_engine_arena_group_step(&leader) && walks == 1); // Retry throttle.
	follower.x = 35; follower.pop.target_id = 77;
	CHECK(population_engine_arena_group_step(&follower) && destination == &leader);
	CHECK(follower.pop.target_id == 0);
	leader.pop.target_id = 77;
	CHECK(population_engine_arena_group_step(&leader) && !leader.walking); // Wait for lagging member.
	follower.x = 22;
	CHECK(!population_engine_arena_group_step(&leader)); // Combat within formation resumes.
	reset(); follower.pop.arena_team = 2; follower.x = 20; follower.pop.target_id = 55;
	CHECK(population_engine_arena_group_step(&follower) && destination == &owner && follower.pop.target_id == 0);
	follower.x = 49; follower.pop.target_id = 0; follower.walking = true;
	CHECK(population_engine_arena_group_step(&follower) && !follower.walking);
	reset(); leader.dead = true;
	CHECK(population_engine_arena_group_step(&follower) && destination == &owner); // Surviving leader elected.
	reset(); owner.state.warping = true; follower.walking = true; follower.pop.target_id = 9;
	CHECK(population_engine_arena_group_step(&follower) && !follower.walking && follower.pop.target_id == 0);
	reset(); online = false; CHECK(population_engine_arena_group_step(&follower) && walks == 0);
	reset(); owner.m = 1; CHECK(population_engine_arena_group_step(&leader) && walks == 0);
	reset(); movable = false; CHECK(population_engine_arena_group_step(&leader) && walks == 0);
	return 0;
}