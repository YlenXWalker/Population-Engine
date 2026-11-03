#include "test_common.hpp"

struct map_session_data {
	int m = 7;
	bool dead = false;
	struct { uint32 char_id = 42; } status;
	struct { int arena_team = 1; uint32 arena_owner = 42; bool arena_eliminated = false; } pop;
};
map_session_data owner;
bool online = true;
std::vector<map_session_data*> g_population_engine_pcs;
int16 map_mapname2mapid(const char* name) { return std::string(name) == "arena" ? 7 : -1; }
map_session_data* map_charid2sd(uint32 cid) { return online && cid == 42 ? &owner : nullptr; }
bool pc_isdead(const map_session_data* sd) { return sd->dead; }

// @production

int main() {
	map_session_data enemy, ally, other_match;
	ally.pop.arena_team = 2; other_match.pop.arena_owner = 43;
	g_population_engine_pcs = {&enemy, &ally, &other_match};
	CHECK(population_engine_arena_result("arena", 42) == 0);
	enemy.dead = enemy.pop.arena_eliminated = true;
	CHECK(population_engine_arena_result("arena", 42) == 1);
	enemy.dead = false; // An external raw revive cannot put an eliminated bot back in play.
	CHECK(population_engine_arena_result("arena", 42) == 1);
	owner.pop.arena_eliminated = true;
	CHECK(population_engine_arena_result("arena", 42) == -1); // Double knockout: player loses.
	owner.pop.arena_eliminated = false; owner.dead = true;
	CHECK(population_engine_arena_result("arena", 42) == -1);
	owner.dead = false; owner.m = 8;
	CHECK(population_engine_arena_result("arena", 42) == -1);
	owner.m = 7; online = false;
	CHECK(population_engine_arena_result("arena", 42) == -1);
	online = true;
	CHECK(population_engine_arena_result("unknown", 42) == -2);
	CHECK(population_engine_arena_result(nullptr, 42) == -2);
	CHECK(population_engine_arena_result("arena", 0) == -2);
	g_population_engine_pcs = {&other_match};
	CHECK(population_engine_arena_result("arena", 42) == -2);
	return 0;
}