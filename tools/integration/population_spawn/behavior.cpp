#include "test_common.hpp"

enum class PopulationBehavior : uint8_t { None, Wander, Combat, Support, Sit, Social, Vendor, Guard, Hangout };
enum class PopulationDbSource : uint8_t { Primary, Secondary };
enum class PopulationCombatStartMode { AutoCombat };
namespace PSF { constexpr unsigned CombatActive = 4; }
constexpr int SCSTART_NOAVOID = 1, SCSTART_LOADED = 2, INVALID_TIMER = -1;
struct PopulationEngine {
	PopulationBehavior behavior = PopulationBehavior::Combat, town_behavior = PopulationBehavior::None;
	PopulationBehavior field_behavior = PopulationBehavior::None, dungeon_behavior = PopulationBehavior::None;
	unsigned flags = 16; int8_t role_type = 2;
};
struct map_session_data {
	int16 m = 7; int fd = 0;
	struct { int party_id = 0; char name[16]{}; } status;
	struct { uint8_t map_category, behavior, behavior_base, db_source; int16_t spawn_x, spawn_y, spawn_map_id; unsigned flags; int8_t role; int arena_team; } pop{};
	struct { int canact_tick = 12, canmove_tick = 13, skilltimer = 14; } ud;
};
struct PopulationCombatStartResult { bool started; int reject_code = 0; };
bool accepted = true, valid = true;
int combat_calls = 0, equips = 0, chats = 0, wanders = 0;
PopulationCombatStartResult population_engine_combat_start_session(map_session_data*, PopulationCombatStartMode, int id, int flags) {
	++combat_calls; valid &= id == -1 && flags == (SCSTART_NOAVOID | SCSTART_LOADED); return {accepted};
}
const char* population_combat_reject_code_name(int) { return "test"; }
void clif_equiplist(map_session_data*) { ++equips; }
void population_engine_register_shell_chat_state(map_session_data*, const PopulationEngine*) { ++chats; }
void population_engine_register_shell_wander_state(map_session_data*) { ++wanders; }

// @production

int main() {
	for (int behavior = 0; behavior <= 7; ++behavior) {
		map_session_data sd; PopulationEngine cfg; cfg.behavior = static_cast<PopulationBehavior>(behavior);
		combat_calls = equips = chats = wanders = 0;
		auto result = population_engine_start_shell_behavior(&sd, 7, 10, 20, 2, PopulationDbSource::Secondary, &cfg);
		CHECK(result == cfg.behavior && sd.pop.behavior == behavior && sd.pop.behavior_base == behavior);
		CHECK(sd.pop.spawn_x == 10 && sd.pop.spawn_y == 20 && sd.pop.spawn_map_id == 7 && sd.pop.map_category == 2);
		CHECK(sd.pop.db_source == 1 && sd.pop.role == 2 && sd.pop.flags == (16 | PSF::CombatActive) && sd.pop.arena_team == 0);
		CHECK(combat_calls == (behavior == 2 || behavior == 7));
		CHECK(wanders == (behavior == 1 || behavior == 2 || behavior == 3 || behavior == 5));
		CHECK(equips == 0 && chats == 1 && sd.ud.skilltimer == INVALID_TIMER && !sd.ud.canact_tick && !sd.ud.canmove_tick);
		CHECK(sd.status.party_id == static_cast<int32>(0x70000007u));
	}
	PopulationEngine cfg; cfg.town_behavior = PopulationBehavior::Sit;
	cfg.field_behavior = PopulationBehavior::Vendor; cfg.dungeon_behavior = PopulationBehavior::Guard;
	map_session_data sd; sd.fd = 1;
	CHECK(population_engine_start_shell_behavior(&sd, 7, 1, 2, 1, PopulationDbSource::Primary, &cfg) == PopulationBehavior::Sit);
	CHECK(population_engine_start_shell_behavior(&sd, 7, 1, 2, 2, PopulationDbSource::Primary, &cfg) == PopulationBehavior::Vendor);
	accepted = false;
	CHECK(population_engine_start_shell_behavior(&sd, 7, 1, 2, 3, PopulationDbSource::Primary, &cfg) == PopulationBehavior::Guard);
	CHECK(population_engine_start_shell_behavior(&sd, 7, 1, 2, 0, PopulationDbSource::Primary, nullptr) == PopulationBehavior::Combat);
	CHECK(valid && equips == 4);
	return 0;
}
