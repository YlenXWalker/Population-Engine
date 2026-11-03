// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// Expose the real internal ally finder to the offline production-engine fixture.
#include "../src/map/population_engine/population_combat.cpp"

map_session_data* population_test_find_support_target(map_session_data* shell, int condition, int threshold, int sc) {
	return population_shell_find_ally_target(shell, static_cast<uint8_t>(condition),
		static_cast<uint8_t>(threshold), static_cast<int16_t>(sc), 9);
}