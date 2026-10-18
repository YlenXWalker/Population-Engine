#include "test_common.hpp"
#include <set>

constexpr int MAX_SKILL = 32, NV_BASIC = 1, NV_FIRSTAID = 2;
constexpr int ADDSKILL_PERMANENT_GRANTED = 1, SKILL_FLAG_PERMANENT = 0, SCO_FORCE = 1;
struct PopulationShellYamlSkill { uint16_t skill_id, level_cap; };
struct PopulationEngine { bool grant_skill_tree = true; std::vector<PopulationShellYamlSkill> shell_attack_skill_yaml; };
struct map_session_data {
	struct { int class_ = 1; struct { uint16_t id = 0, lv = 0; int flag = 0; } skill[MAX_SKILL]; } status;
};
struct Entry { uint16 max_lv; };
struct s_skill_tree { std::map<uint16, std::shared_ptr<Entry>> skills; };
struct { std::shared_ptr<s_skill_tree> tree; std::shared_ptr<s_skill_tree> find(int) { return tree; } } skill_tree_db;
std::vector<std::string> calls;
void pc_calc_skilltree(map_session_data* sd) { calls.push_back("tree"); sd->status.skill[20] = {20, 9, 0}; }
void pc_skill(map_session_data* sd, uint16 id, uint16 level, int) {
	calls.push_back("grant"); sd->status.skill[id] = {id, level, 0};
}
uint16 skill_get_index(uint16 id) { return id < MAX_SKILL ? id : 0; }
uint16 skill_get_max(uint16) { return 7; }
uint16 pc_checkskill(map_session_data* sd, uint16 id) { return sd->status.skill[id].lv; }
void status_calc_pc(map_session_data*, int) { calls.push_back("recalc"); }

// @production

int main() {
	map_session_data sd{};
	skill_tree_db.tree = std::make_shared<s_skill_tree>();
	skill_tree_db.tree->skills[10] = std::make_shared<Entry>(Entry{5});
	skill_tree_db.tree->skills[11] = nullptr;
	skill_tree_db.tree->skills[12] = std::make_shared<Entry>(Entry{0});
	population_engine_grant_shell_skills(&sd, nullptr, 1);
	CHECK(sd.status.skill[10].lv == 5 && sd.status.skill[20].id == 0);
	CHECK(sd.status.skill[11].id == 0 && sd.status.skill[12].id == 0);
	CHECK(calls.front() == "tree" && calls.back() == "recalc");
	CHECK(std::count(calls.begin(), calls.end(), "recalc") == 1);
	PopulationEngine cfg; cfg.grant_skill_tree = false;
	cfg.shell_attack_skill_yaml = {{0, 10}, {100, 10}, {10, 20}, {11, 0}};
	calls.clear();
	population_engine_grant_shell_skills(&sd, &cfg, 2);
	CHECK(sd.status.skill[NV_BASIC].lv == 1 && sd.status.skill[NV_FIRSTAID].lv == 1);
	CHECK(sd.status.skill[10].lv == 7 && sd.status.skill[11].lv == 0);
	CHECK(std::find(calls.begin(), calls.end(), "tree") == calls.end() && calls.back() == "recalc");
	skill_tree_db.tree.reset(); calls.clear();
	population_engine_grant_shell_skills(&sd, nullptr, 3);
	CHECK(calls.back() == "recalc");
	return 0;
}
