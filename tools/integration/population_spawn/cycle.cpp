#include "test_common.hpp"
#include <cstdint>
#include <ctime>
using uint8 = uint8_t;
enum class PopulationBehavior : uint8 { None, Wander, Combat, Support, Sit, Social, Vendor, Guard, Hangout };
struct map_session_data {
	struct { uint32_t companion_owner = 0; uint8 behavior_base = 0, behavior = 0, map_category = 1; } pop;
};
struct { bool population_engine_daily_cycle = true; } battle_config;
int sits = 0, stands = 0, hour = 0;
void pc_setsit(map_session_data*) { ++sits; }
void clif_sitting(map_session_data&) {}
void pc_setstand(map_session_data*, bool) { ++stands; }
void clif_standing(map_session_data&) {}
time_t fake_time(time_t*) { return 0; }
tm* fake_localtime(const time_t*) { static tm value; value.tm_hour = hour; return &value; }
#define time fake_time
#define localtime fake_localtime

// @production

int main() {
	map_session_data sd;
	for (auto stable : { PopulationBehavior::Hangout, PopulationBehavior::Vendor, PopulationBehavior::Sit }) {
		for (int h : {0, 12, 20, 23}) {
			hour = h;
			sd.pop.behavior_base = sd.pop.behavior = static_cast<uint8>(stable);
			population_shell_apply_daily_cycle(&sd);
			CHECK(sd.pop.behavior == static_cast<uint8>(stable));
		}
	}
	CHECK(sits == 0 && stands == 0);
	sd.pop.behavior_base = sd.pop.behavior = static_cast<uint8>(PopulationBehavior::Combat);
	sd.pop.companion_owner = 42;
	hour = 23;
	population_shell_apply_daily_cycle(&sd);
	CHECK(sd.pop.behavior == static_cast<uint8>(PopulationBehavior::Combat));
	sd.pop.companion_owner = 0;
	population_shell_apply_daily_cycle(&sd);
	CHECK(sd.pop.behavior == static_cast<uint8>(PopulationBehavior::Sit) && sits == 1);
	hour = 12;
	population_shell_apply_daily_cycle(&sd);
	CHECK(sd.pop.behavior == static_cast<uint8>(PopulationBehavior::Combat) && stands == 1);
	return 0;
}
