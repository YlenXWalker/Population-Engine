#include "test_common.hpp"
#include <unordered_set>

enum { MF_PVP, MF_BATTLEGROUND };
enum { CLR_TELEPORT, SETPOS_OK, SETPOS_FAILED };
struct map_session_data {
	int id = 0, m = 0, mapindex = 1, x = 10, y = 10;
	map_session_data* prev = this;
	bool dead = false, walking = false;
	struct { bool active = true, warping = false; } state;
	struct { int party_id = 7; uint32 char_id = 42; } status;
	struct { uint32 companion_owner = 42; int spawn_map_id = 0, spawn_x = 0, spawn_y = 0, target_id = 99; } pop;
};
struct map_data {
	bool pvp = false, battleground = false;
	bool getMapFlag(int flag) const { return flag == MF_PVP ? pvp : battleground; }
} maps[2];
map_session_data owner, bot;
std::unordered_set<int32> g_population_companions{5000001};
bool online = true, gvg = false, freecell = true, warp_ok = true;
int warps = 0, walks = 0, removed = 0, dirtied = 0;
map_session_data* map_id2sd(int id) { return id == bot.id ? &bot : nullptr; }
map_session_data* map_charid2sd(uint32 id) { return online && id == owner.status.char_id ? &owner : nullptr; }
map_data* map_getmapdata(int m) { return m >= 0 && m < 2 ? &maps[m] : nullptr; }
bool map_flag_gvg2(int) { return gvg; }
bool pc_isdead(map_session_data* sd) { return sd->dead; }
int distance_bl(map_session_data* a, map_session_data* b) {
	return std::max(std::abs(a->x - b->x), std::abs(a->y - b->y));
}
bool map_search_freecell(map_session_data*, int, int16* x, int16* y, int, int, int) {
	++*x; ++*y; return freecell;
}
int pc_setpos(map_session_data* sd, int index, int x, int y, int) {
	++warps;
	if (!warp_ok) return SETPOS_FAILED;
	// pc_setpos must complete the clientless warp before it returns; the warp
	// completion suite independently checks the production map attachment code.
	sd->mapindex = index; sd->m = index - 1; sd->x = x; sd->y = y;
	sd->prev = sd; sd->state.warping = false;
	return SETPOS_OK;
}
void population_engine_shell_index_mark_dirty() { ++dirtied; }
bool unit_is_walking(map_session_data* sd) { return sd->walking; }
void unit_walktobl(map_session_data*, map_session_data*, int, int) { ++walks; }
void population_engine_remove_managed_shell(map_session_data*) { ++removed; }

// @production

void reset() {
	owner = map_session_data{}; owner.prev = &owner;
	bot = map_session_data{}; bot.prev = &bot; bot.id = 5000001;
	online = freecell = warp_ok = true; gvg = false;
	maps[0] = {}; maps[1] = {};
	warps = walks = removed = dirtied = 0;
}

int main() {
	reset();
	// An owner between pc_setpos and LoadEndAck is not a follow destination yet.
	owner.m = 1; owner.mapindex = 2; owner.x = 100;
	owner.state.warping = true; owner.prev = nullptr;
	population_engine_companion_timer();
	CHECK(warps == 0 && walks == 0 && removed == 0);
	owner.state.warping = false;
	population_engine_companion_timer();
	CHECK(warps == 0 && removed == 0); // Still detached from the map.
	owner.prev = &owner; owner.state.active = false;
	population_engine_companion_timer();
	CHECK(warps == 0 && removed == 0);
	owner.state.active = true;
	population_engine_companion_timer();
	CHECK(warps == 1 && bot.m == 1 && bot.prev == &bot && !bot.state.warping);
	CHECK(bot.status.party_id == 7 && bot.pop.companion_owner == 42 && removed == 0);
	CHECK(bot.pop.spawn_map_id == 1 && bot.pop.spawn_x == owner.x && bot.pop.target_id == 0);
	population_engine_companion_timer();
	CHECK(warps == 1); // No repeated warp while already in formation.

	for (int i = 0; i < 6; ++i) {
		owner.m = i % 2; owner.mapindex = owner.m + 1; owner.x += 40;
		population_engine_companion_timer();
		CHECK(bot.m == owner.m && bot.prev == &bot && !bot.state.warping && removed == 0);
	}
	owner.x += 40; // Same-map Fly Wing / Teleport catch-up.
	const int before = warps;
	population_engine_companion_timer();
	CHECK(warps == before + 1 && bot.m == owner.m && bot.prev == &bot);

	reset(); owner.x = 30; freecell = false;
	population_engine_companion_timer();
	CHECK(warps == 0 && dirtied == 0 && removed == 0);
	freecell = true; warp_ok = false;
	population_engine_companion_timer();
	CHECK(warps == 1 && dirtied == 0 && bot.pop.target_id == 99 && removed == 0);
	warp_ok = true;
	population_engine_companion_timer();
	CHECK(warps == 2 && bot.prev && bot.pop.target_id == 0);
	bot.prev = nullptr; // An earlier incomplete warp can also be retried.
	population_engine_companion_timer();
	CHECK(warps == 3 && bot.prev == &bot && removed == 0);

	reset(); owner.x = 16;
	population_engine_companion_timer();
	CHECK(walks == 1 && warps == 0);
	bot.walking = true;
	population_engine_companion_timer();
	CHECK(walks == 1);
	bot.dead = true; owner.x = 100;
	population_engine_companion_timer();
	CHECK(warps == 0 && removed == 0 && bot.pop.spawn_x == 100);

	reset(); maps[1].pvp = true; owner.m = 1; owner.mapindex = 2;
	population_engine_companion_timer();
	CHECK(removed == 1 && warps == 0);
	reset(); maps[0].battleground = true; population_engine_companion_timer(); CHECK(removed == 1);
	reset(); gvg = true; population_engine_companion_timer(); CHECK(removed == 1);
	reset(); online = false; population_engine_companion_timer(); CHECK(removed == 1);
	reset(); owner.status.party_id = 0; population_engine_companion_timer(); CHECK(removed == 1);
	reset(); owner.status.party_id = 8; population_engine_companion_timer(); CHECK(removed == 1);
	return 0;
}