#include "test_common.hpp"
#include <cstdarg>
#include <unordered_set>

constexpr int BL_PC = 1;
enum class sc_type { Blessing = 1, IncreaseAgi = 2 };
enum class PopSkillCondition : uint8_t { Always, AllyHpBelow, AllyStatus, NotAllyStatus };
struct status_change {
	std::unordered_set<int> active;
	bool hasSCE(sc_type sc) const { return active.count(static_cast<int>(sc)) != 0; }
};
struct map_session_data {
	int id = 0, type = BL_PC, m = 0, x = 10, y = 10;
	map_session_data* prev = this;
	bool dead = false;
	struct { bool active = true, warping = false, population_combat = false; } state;
	struct { int party_id = 7; uint32 char_id = 0; } status;
	struct { uint32 companion_owner = 0; int arena_team = 0; bool arena_eliminated = false; } pop;
	struct { uint32 hp = 100, max_hp = 100; } battle_status;
	status_change sc;
};
using block_list = map_session_data;
#define BL_CAST(kind, value) ((value) && (value)->type == (kind) ? (value) : nullptr)
bool status_isdead(const map_session_data& sd) { return sd.dead; }
const status_change* status_get_sc(const map_session_data* sd) { return &sd->sc; }
bool population_engine_arena_is_ally(const map_session_data* a, const map_session_data* b) {
	if (!a->pop.arena_team && !b->pop.arena_team) return false;
	return (a->pop.arena_team ? a->pop.arena_team : 2) == (b->pop.arena_team ? b->pop.arena_team : 2);
}
std::vector<map_session_data*> sessions;
map_session_data* map_charid2sd(uint32 cid) {
	for (auto* sd : sessions) if (sd->status.char_id == cid) return sd;
	return nullptr;
}
int distance_bl(const map_session_data* a, const map_session_data* b) {
	return std::max(std::abs(a->x - b->x), std::abs(a->y - b->y));
}
struct PopAllySearchCtx {
	map_session_data* shell; uint8_t hp_threshold; int16_t sc_resolved;
	bool want_has_status; int best_hp_pct; map_session_data* result;
};
void map_foreachinrange(int32 (*cb)(block_list*, va_list), map_session_data* shell, int range, int kind, ...) {
	va_list ap; va_start(ap, kind);
	for (auto* sd : sessions) {
		if (sd->m != shell->m || sd->type != kind || distance_bl(shell, sd) > range) continue;
		va_list copy; va_copy(copy, ap);
		cb(sd, copy);
		va_end(copy);
	}
	va_end(ap);
}

// @production

int main() {
	map_session_data shell, owner, member, ambient, enemy;
	shell.id = 10; shell.pop.companion_owner = 42; shell.state.population_combat = true;
	owner.id = 1; owner.status.char_id = 42;
	member.id = 2; member.status.char_id = 43;
	ambient.id = 20; ambient.status.party_id = 0x70000000; ambient.state.population_combat = true;
	enemy.id = 30; enemy.pop.arena_team = 1; enemy.state.population_combat = true;
	sessions = {&ambient, &member, &owner, &shell, &enemy};
	CHECK(population_shell_is_support_ally(&shell, &owner));
	CHECK(population_shell_is_support_ally(&shell, &member));
	CHECK(!population_shell_is_support_ally(&shell, &ambient));
	CHECK(!population_shell_is_support_ally(&shell, &enemy));
	CHECK(population_shell_find_ally_target(&shell, uint8_t(PopSkillCondition::NotAllyStatus), 0, 1) == &owner);
	owner.sc.active.insert(1);
	CHECK(population_shell_find_ally_target(&shell, uint8_t(PopSkillCondition::NotAllyStatus), 0, 1) == &member);
	member.sc.active.insert(1);
	CHECK(!population_shell_find_ally_target(&shell, uint8_t(PopSkillCondition::NotAllyStatus), 0, 1));
	CHECK(population_shell_find_ally_target(&shell, uint8_t(PopSkillCondition::AllyStatus), 0, 1) == &owner);
	ambient.battle_status.hp = 1; owner.battle_status.hp = 50; member.battle_status.hp = 20;
	CHECK(population_shell_find_ally_target(&shell, uint8_t(PopSkillCondition::AllyHpBelow), 60, -1) == &member);
	owner.battle_status.hp = 10;
	CHECK(population_shell_find_ally_target(&shell, uint8_t(PopSkillCondition::AllyHpBelow), 60, -1) == &owner);
	owner.state.warping = true;
	CHECK(population_shell_find_ally_target(&shell, uint8_t(PopSkillCondition::Always), 0, -1) == &member);
	owner.state.warping = false; owner.x = 40;
	CHECK(population_shell_find_ally_target(&shell, uint8_t(PopSkillCondition::Always), 0, -1) == &member);
	owner.x = 10; owner.status.party_id = 8;
	CHECK(!population_shell_is_support_ally(&shell, &owner));
	owner.status.party_id = 7; owner.dead = true;
	CHECK(!population_shell_is_support_ally(&shell, &owner));
	owner.dead = false; owner.prev = nullptr;
	CHECK(!population_shell_is_support_ally(&shell, &owner));
	owner.prev = &owner; owner.m = 1;
	CHECK(!population_shell_is_support_ally(&shell, &owner));
	owner.m = 0;
	shell.pop.companion_owner = 0; shell.status.party_id = 0;
	CHECK(population_shell_is_support_ally(&shell, &ambient));
	CHECK(!population_shell_is_support_ally(&shell, &owner));
	shell.pop.arena_team = 2;
	CHECK(population_shell_is_support_ally(&shell, &owner));
	CHECK(!population_shell_is_support_ally(&shell, &enemy));
	return 0;
}