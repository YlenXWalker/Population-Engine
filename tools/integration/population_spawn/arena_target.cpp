#include "test_common.hpp"
enum { BL_PC = 1, CELL_CHKWALL, AREA_SIZE = 14 };
struct map_session_data {
	int id = 95000001, type = BL_PC, m = 0, x = 10, y = 10;
	map_session_data* prev = this;
	bool dead = false;
	struct { bool active = true, warping = false; } state;
	struct { uint32 char_id = 42; } status;
	struct { int arena_team = 1; uint32 arena_owner = 42; bool arena_eliminated = false; } pop;
};
using block_list = map_session_data;
#define BL_CAST(kind, value) ((value) && (value)->type == (kind) ? (value) : nullptr)
std::map<int, map_session_data*> sessions;
bool line_of_sight = true;
block_list* map_id2bl(unsigned id) { return sessions.count(id) ? sessions[id] : nullptr; }
bool status_isdead(const map_session_data& sd) { return sd.dead; }
bool population_engine_is_population_pc(unsigned id) { return id >= 95000000; }
bool path_search_long(void*, int, int, int, int, int, int) { return line_of_sight; }
int distance_bl(block_list* a, block_list* b) { return std::max(std::abs(a->x-b->x), std::abs(a->y-b->y)); }

// @production

int main() {
	map_session_data enemy, ally, player, spectator, ambient;
	ally.id = 95000002; ally.pop.arena_team = 2;
	player.id = 2000001; player.pop.arena_team = 0;
	spectator.id = 2000002; spectator.status.char_id = 43; spectator.pop.arena_team = 0;
	ambient.id = 95000003; ambient.pop.arena_team = 0;
	sessions = {{ally.id,&ally},{enemy.id,&enemy},{player.id,&player},{spectator.id,&spectator},{ambient.id,&ambient}};
	CHECK(population_shell_check_arena_target(&enemy, player.id));
	CHECK(population_shell_check_arena_target(&enemy, ally.id));
	CHECK(population_shell_check_arena_target(&ally, enemy.id));
	CHECK(!population_shell_check_arena_target(&ally, player.id));
	CHECK(!population_shell_check_arena_target(&enemy, spectator.id));
	CHECK(!population_shell_check_arena_target(&enemy, ambient.id));
	CHECK(!population_shell_check_arena_target(&enemy, enemy.id));
	ally.pop.arena_owner = 43; CHECK(!population_shell_check_arena_target(&enemy, ally.id));
	ally.pop.arena_owner = 42; ally.dead = true; CHECK(!population_shell_check_arena_target(&enemy, ally.id));
	ally.dead = false; ally.state.warping = true; CHECK(!population_shell_check_arena_target(&enemy, ally.id));
	ally.state.warping = false; ally.x = 40; CHECK(!population_shell_check_arena_target(&enemy, ally.id));
	ally.x = 10; line_of_sight = false; CHECK(!population_shell_check_arena_target(&enemy, ally.id));
	return 0;
}