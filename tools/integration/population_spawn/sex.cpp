#include "test_common.hpp"
#include <cstdint>

struct PopulationEngine { int sex_override = -1; };
int random_value = 0, random_calls = 0;
int rnd() { ++random_calls; return random_value; }
char get_job_required_sex(uint16_t job_id) {
	return job_id == 1 ? 'M' : job_id == 2 ? 'F' : '\0';
}

// @production

int main() {
	PopulationEngine config;
	config.sex_override = 0;
	CHECK(population_engine_choose_shell_sex(1, &config) == 'M');
	config.sex_override = 1;
	CHECK(population_engine_choose_shell_sex(2, &config) == 'F');
	CHECK(population_engine_choose_shell_sex(0, &config) == 'M');
	config.sex_override = 0;
	CHECK(population_engine_choose_shell_sex(0, &config) == 'F');
	CHECK(random_calls == 0);
	config.sex_override = -1;
	CHECK(population_engine_choose_shell_sex(0, &config) == 'F');
	random_value = 1;
	CHECK(population_engine_choose_shell_sex(0, &config) == 'M');
	CHECK(population_engine_choose_shell_sex(0, nullptr) == 'M');
	CHECK(random_calls == 3);
	return 0;
}