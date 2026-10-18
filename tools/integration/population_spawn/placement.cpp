#include "test_common.hpp"

enum e_setpos { SETPOS_OK, SETPOS_FAILED };
constexpr int CLR_OUTSIGHT = 0;
struct map_session_data {
	int m = 7;
	struct { int changemap = 1, connect_new = 1, warping = 1, debug_remove_map = 1; } state;
};
struct map_data { int users = 0, users_pvp = 0; } mapdata;
struct { bool dynamic_mobs = true; } battle_config;
int fail_at = 0, destroyed = 0;
bool invisible = false;
std::vector<std::string> calls;
uint16 mapindex_name2id(const char*) { return fail_at == 1 ? 0 : 7; }
e_setpos pc_setpos(map_session_data*, uint16, int, int, int) {
	calls.push_back("position"); return fail_at == 2 ? SETPOS_FAILED : SETPOS_OK;
}
void map_addiddb(map_session_data*) { calls.push_back("id_add"); }
int map_addblock(map_session_data*) { calls.push_back("block_add"); return fail_at == 3 ? 1 : 0; }
void map_deliddb(map_session_data*) { calls.push_back("id_remove"); }
void population_engine_destroy_failed_spawn(map_session_data*) { calls.push_back("destroy"); ++destroyed; }
map_data* map_getmapdata(int) { return fail_at == 4 ? nullptr : &mapdata; }
void map_spawnmobs(int) { calls.push_back("spawn_mobs"); }
bool pc_isinvisible(map_session_data*) { return invisible; }

// @production

int main() {
	map_session_data sd;
	CHECK(population_engine_place_shell(&sd, "prontera", 10, 20, 1));
	CHECK((calls == std::vector<std::string>{"position", "id_add", "block_add", "spawn_mobs"}));
	CHECK(mapdata.users == 1 && mapdata.users_pvp == 1 && destroyed == 0);
	CHECK(!sd.state.changemap && !sd.state.connect_new && !sd.state.warping && !sd.state.debug_remove_map);
	calls.clear(); invisible = true;
	CHECK(population_engine_place_shell(&sd, "prontera", 10, 20, 2));
	CHECK(mapdata.users == 2 && mapdata.users_pvp == 1 && calls.size() == 3);
	for (int failure = 1; failure <= 3; ++failure) {
		fail_at = failure; calls.clear(); destroyed = 0; mapdata = {};
		CHECK(!population_engine_place_shell(&sd, "prontera", 10, 20, 3));
		CHECK(destroyed == 1 && mapdata.users == 0 && mapdata.users_pvp == 0);
		if (failure == 1) CHECK((calls == std::vector<std::string>{"destroy"}));
		if (failure == 2) CHECK((calls == std::vector<std::string>{"position", "destroy"}));
		if (failure == 3) CHECK((calls == std::vector<std::string>{"position", "id_add", "block_add", "id_remove", "destroy"}));
	}
	fail_at = 4; destroyed = 0;
	CHECK(population_engine_place_shell(&sd, "prontera", 10, 20, 4) && destroyed == 0);
	return 0;
}
