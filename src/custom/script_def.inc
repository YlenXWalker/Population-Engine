// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

/**
 * Custom Script Commands
 * Place the definition of custom script commands in this file.
 * Format:
 *	BUILDIN_DEF(command_name,"parameters"),
 **/

//BUILDIN_DEF(example,""),

// Population / default ammo (skill require.ammo); see custom script.inc
BUILDIN_DEF(setarrow, "?"),
BUILDIN_DEF(setbullet, "?"),
BUILDIN_DEF(setkunai, "?"),

// Population engine arena
BUILDIN_DEF(population_arena_start, "si????"),
BUILDIN_DEF(population_arena_stop,  "s"),
BUILDIN_DEF(population_arena_jobs,  ""),

BUILDIN_DEF(population_companion_recruit, "i"),
BUILDIN_DEF(population_companion_dismiss, ""),
BUILDIN_DEF(population_hangout_spawn, "i"),
