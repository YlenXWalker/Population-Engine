#include "test_common.hpp"
#include <atomic>
#include <unordered_set>

struct map_session_data {
	int m = 7;
	struct { uint32 char_id = 42; } status;
	struct { int arena_team = 1; uint32 arena_owner = 42; bool arena_eliminated = true; } pop;
};
map_session_data owner;
std::vector<map_session_data*> g_population_engine_pcs, released;
std::atomic<size_t> g_population_engine_count{4};
struct { uint32 active_units = 4; } g_population_engine_stats;
int dirtied = 0;
int16 map_mapname2mapid(const char* name) { return std::string(name) == "arena" ? 7 : -1; }
map_session_data* map_charid2sd(uint32 cid) { return cid == 42 ? &owner : nullptr; }
void population_engine_shell_index_mark_dirty() { ++dirtied; }
void population_engine_shell_release(map_session_data* sd) { released.push_back(sd); }

// @production

int main() {
	map_session_data enemy, ally, ambient, other_room;
	ally.pop.arena_team = 2; ambient.pop.arena_team = 0; other_room.m = 8;
	g_population_engine_pcs = {&enemy, &ambient, &ally, &other_room};
	population_engine_arena_stop("arena");
	CHECK(released.size() == 2 && released[0] == &enemy && released[1] == &ally);
	CHECK(g_population_engine_pcs == (std::vector<map_session_data*>{&ambient, &other_room}));
	CHECK(g_population_engine_count == 2 && g_population_engine_stats.active_units == 2);
	CHECK(!owner.pop.arena_eliminated && dirtied == 1);
	population_engine_arena_stop("arena");
	CHECK(released.size() == 2 && g_population_engine_count == 2 && dirtied == 1);
	population_engine_arena_stop(nullptr);
	population_engine_arena_stop("unknown");
	CHECK(released.size() == 2);
	return 0;
}