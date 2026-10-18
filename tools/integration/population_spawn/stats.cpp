#include "test_common.hpp"

constexpr int MAX_LEVEL = 300;
int rolls = 0;
uint32 rnd() { return rolls++; }
template<class V, class L, class H> V cap_value(V value, L low, H high) {
	return value < low ? static_cast<V>(low) : value > high ? static_cast<V>(high) : value;
}
struct PopulationEngine {
	int16_t base_level_min = -1, base_level_max = -1, job_level_min = -1, job_level_max = -1;
	int16_t str_min = -1, str_max = -1, agi_min = -1, agi_max = -1, vit_min = -1, vit_max = -1;
	int16_t intl_min = -1, intl_max = -1, dex_min = -1, dex_max = -1, luk_min = -1, luk_max = -1;
};
struct map_session_data {
	struct { int base_level, job_level, str, agi, vit, int_, dex, luk, max_hp, hp, max_sp, sp; } status{};
};

// @production

int main() {
	map_session_data sd;
	population_engine_initialize_shell_stats(&sd, nullptr);
	CHECK(sd.status.base_level == 99 && sd.status.job_level == 70);
	CHECK(sd.status.str == 90 && sd.status.agi == 91 && sd.status.vit == 92);
	CHECK(sd.status.int_ == 93 && sd.status.dex == 94 && sd.status.luk == 95 && rolls == 6);
	CHECK(sd.status.hp == 1 && sd.status.max_hp == 1 && sd.status.sp == 1 && sd.status.max_sp == 1);
	PopulationEngine cfg;
	cfg.base_level_min = 1; cfg.base_level_max = 10;
	cfg.job_level_min = 400; cfg.job_level_max = 400;
	cfg.str_min = 2000; cfg.agi_min = 0; cfg.vit_min = 20;
	cfg.intl_min = 30; cfg.dex_min = 40; cfg.luk_min = 50;
	rolls = 0;
	population_engine_initialize_shell_stats(&sd, &cfg);
	CHECK(sd.status.base_level == 1 && sd.status.job_level == MAX_LEVEL);
	CHECK(sd.status.str == 999 && sd.status.agi == 1 && sd.status.vit == 20);
	CHECK(sd.status.int_ == 30 && sd.status.dex == 40 && sd.status.luk == 50 && rolls == 8);
	cfg.base_level_min = 10; cfg.base_level_max = 1; rolls = 2;
	population_engine_initialize_shell_stats(&sd, &cfg);
	CHECK(sd.status.base_level == 3);
	return 0;
}
