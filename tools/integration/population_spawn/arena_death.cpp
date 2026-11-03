#include "test_common.hpp"

constexpr int INVALID_TIMER = -1;
struct map_session_data {
	int id = 95000001, respawn_tid = INVALID_TIMER;
	bool mortal = true;
	struct { char name[24] = "Arena fighter"; } status;
	struct { int arena_team = 1, respawn_timer = INVALID_TIMER; bool arena_eliminated = false; } pop;
};
using TimerFunc = int (*)(int, t_tick, int, intptr_t);
int population_engine_respawn_shell_timer(int, t_tick, int, intptr_t) { return 0; }
int pc_respawn_timer(int, t_tick, int, intptr_t) { return 0; }
struct TimerData { TimerFunc func; } population_timer{population_engine_respawn_shell_timer}, player_timer{pc_respawn_timer};
int added = 0, deleted = 0;
bool population_engine_shell_is_mortal(map_session_data* sd) { return sd->mortal; }
const TimerData* get_timer(int id) { return id == 10 ? &population_timer : &player_timer; }
int delete_timer(int, TimerFunc) { ++deleted; return 0; }
int add_timer(t_tick, TimerFunc, int, int) { ++added; return 10; }
t_tick gettick() { return 1000; }
template<class... Args> void ShowDebug(const char*, Args...) {}

// @production

int main() {
	map_session_data sd;
	population_engine_on_shell_death(&sd);
	CHECK(added == 0 && sd.pop.respawn_timer == INVALID_TIMER && sd.pop.arena_eliminated);
	sd.pop.respawn_timer = 10; sd.respawn_tid = 20;
	population_engine_on_shell_death(&sd);
	CHECK(added == 0 && deleted == 2 && sd.pop.respawn_timer == INVALID_TIMER && sd.respawn_tid == INVALID_TIMER);
	sd.pop.arena_team = 0; sd.pop.arena_eliminated = false;
	population_engine_on_shell_death(&sd);
	CHECK(added == 1 && sd.pop.respawn_timer == 10 && !sd.pop.arena_eliminated);
	sd.mortal = false;
	population_engine_on_shell_death(&sd);
	CHECK(added == 1);
	population_engine_on_shell_death(nullptr);
	return 0;
}