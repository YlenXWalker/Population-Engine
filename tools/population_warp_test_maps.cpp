// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// Compile the production map registry with fixture-only initialization access.
#define main population_warp_unused_map_main
#include "../src/map/map.cpp"
#undef main

void population_warp_test_init_registries() {
	id_db = idb_alloc(DB_OPT_BASE);
	pc_db = idb_alloc(DB_OPT_BASE);
	mobid_db = idb_alloc(DB_OPT_BASE);
	bossid_db = idb_alloc(DB_OPT_BASE);
	map_db = uidb_alloc(DB_OPT_BASE);
	nick_db = idb_alloc(DB_OPT_BASE);
	charid_db = uidb_alloc(DB_OPT_BASE);
	regen_db = idb_alloc(DB_OPT_BASE);
	iwall_db = strdb_alloc(DB_OPT_RELEASE_DATA, sizeof(iwall_data::wall_name));
}

void population_warp_test_register_map(int16 m) {
	uidb_put(map_db, map[m].index, &map[m]);
}