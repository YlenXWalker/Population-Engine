#include "test_common.hpp"

enum e_mode { MD_CANMOVE = 1, MD_CANATTACK = 2 };
constexpr int EFST_BLANK = 0, BSF_PERMANENT = 1, SCO_NONE = 0;
struct script_code {};
struct s_bonus_script_entry {} entry;
struct PopulationEngine { std::string bonus_script_str; };
struct map_session_data {
	int id = 42, max_weight = 0;
	struct { int mode; } base_status{};
	struct { bool active = false, pc_loaded = false; } state;
	struct { int str = 90, sp = 0; } status;
	struct { int max_sp = 500, sp = 0; } battle_status;
	struct { void* head = nullptr; } bonus_script;
};
struct { int id = 123; } npc;
auto* fake_nd = &npc;
map_session_data* current;
bool valid = true, bonus_available = true, reset_weight = true;
std::vector<std::string> calls;
void run_script(script_code*, int pos, int rid, int oid) {
	valid &= current->state.active && current->state.pc_loaded && current->max_weight >= 10000;
	valid &= pos == 0 && rid == 42 && oid == 123; calls.push_back("script");
}
s_bonus_script_entry* pc_bonus_script_add(map_session_data*, const char* text, t_tick duration, int icon, int flag, int type) {
	valid &= std::string(text) == "bonus bStr,1;" && duration == 86400000LL * 365 * 10;
	valid &= icon == EFST_BLANK && flag == BSF_PERMANENT && type == 0;
	calls.push_back("bonus"); return bonus_available ? &entry : nullptr;
}
void linkdb_insert(void**, void*, void* value) { valid &= value == &entry; calls.push_back("link"); }
void status_calc_pc(map_session_data* sd, int) {
	calls.push_back("recalc"); if (reset_weight) sd->max_weight = 0;
}

// @production

int main() {
	map_session_data sd; current = &sd; script_code script; PopulationEngine cfg;
	cfg.bonus_script_str = "bonus bStr,1;";
	population_engine_apply_shell_scripts(&sd, &script, &cfg);
	CHECK(valid && (calls == std::vector<std::string>{"script", "bonus", "link", "recalc"}));
	CHECK(sd.status.sp == 500 && sd.battle_status.sp == 500 && sd.max_weight == 35000);
	CHECK(sd.base_status.mode == (MD_CANMOVE | MD_CANATTACK));
	calls.clear(); sd = {}; reset_weight = false; sd.max_weight = 200000;
	population_engine_apply_shell_scripts(&sd, nullptr, nullptr);
	CHECK((calls == std::vector<std::string>{"recalc"}) && sd.max_weight == 200000);
	calls.clear(); bonus_available = false;
	population_engine_apply_shell_scripts(&sd, nullptr, &cfg);
	CHECK(valid && (calls == std::vector<std::string>{"bonus", "recalc"}));
	return 0;
}
