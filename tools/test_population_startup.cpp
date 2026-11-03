// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// Offline startup check: use the real compiler, builtin table and YAML loaders.
#include "../src/map/script.cpp"
#include "../src/map/population_engine/population_config.hpp"
#include "../src/map/population_engine/population_combat.hpp"
#include "../src/map/pc_groups.hpp"
#include "../src/common/grfio.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

// In-memory maps and registries are initialized through a fixture-only wrapper
// around map.cpp; all lookups and warp/packet functions remain production code.
void population_warp_test_init_registries();
void population_warp_test_register_map(int16 m);
map_session_data* population_test_find_support_target(map_session_data* shell, int condition, int threshold, int sc);
static uint16 warp_map_indexes[2]{};
TIMER_FUNC(population_engine_respawn_shell_timer);

static void require(bool condition, const char* message) {
	if (!condition) {
		std::cerr << "FAIL: " << message << std::endl;
		std::exit(1);
	}
}

static void check_ammo_and_support(map_session_data* shell) {
	// Real item/eligibility code must not insert a candidate into inventory just
	// to check it. Sleep Arrow is legal for archer jobs but not for a mage.
	shell->status.class_ = JOB_MAGE;
	shell->class_ = pc_jobid2mapid(JOB_MAGE);
	shell->status.base_level = 50;
	shell->status.sex = SEX_MALE;
	shell->status.inventory_slots = MAX_INVENTORY;
	require(pc_isequip_item(shell, item_db.find(1768).get()) != ITEM_EQUIP_ACK_OK,
		"Sleep Arrow cannot be selected for an ineligible mage");
	shell->status.class_ = JOB_ARCHER;
	shell->class_ = pc_jobid2mapid(JOB_ARCHER);
	shell->status.weapon = shell->weapontype1 = W_BOW;
	const auto selected = script_find_first_ammo_item(shell, AMMO_ARROW);
	require(selected != 0 && pc_isequip_item(shell, item_db.find(selected).get()) == ITEM_EQUIP_ACK_OK,
		"real ammo picker chooses usable archer ammo");
	auto& ammo = shell->inventory.u.items_inventory[9];
	ammo.nameid = 1768; ammo.amount = 500; ammo.identify = 1; ammo.equip = EQP_AMMO;
	shell->inventory_data[9] = item_db.find(1768).get();
	shell->equip_index[EQI_AMMO] = 9;
	for (int i = 0; i < 20; ++i)
		require(script_give_equip_ammo(shell, "setarrow", 1768), "repeated ammo setup accepts equipped Sleep Arrow");
	require(ammo.amount == 500 && shell->equip_index[EQI_AMMO] == 9, "repeated ammo calls neither refill nor re-equip");
	for (const auto& row : population_engine_db()) {
		const auto& bonus = row.second->bonus_script_str;
		for (const char* setup : {"setarrow", "setcart", "setriding", "setfalcon", "setbullet", "setkunai"})
			require(bonus.find(setup) == std::string::npos, "setup commands never enter persistent profile bonus scripts");
		if (!bonus.empty()) {
			auto* code = parse_script(bonus.c_str(), "filtered population bonus", 1, SCRIPT_IGNORE_EXTERNAL_BRACKETS);
			require(code != nullptr, "filtered conditional profile script compiles");
			script_free_code(code);
		}
	}
	std::cout << "PASS: production ammo eligibility, repeated equipped-stack setup and filtered profile scripts" << std::endl;

	auto make_ally = [&](int id, uint32 cid, int party, bool population) {
		auto* sd = new map_session_data();
		sd->id = sd->status.account_id = id; sd->status.char_id = cid; sd->status.party_id = party;
		sd->type = BL_PC; sd->state.active = true; sd->state.population_combat = population;
		sd->m = shell->m; sd->mapindex = shell->mapindex; sd->x = shell->x + 1; sd->y = shell->y;
		sd->status.hp = sd->battle_status.hp = sd->battle_status.max_hp = 100;
		unit_dataset(sd); map_addiddb(sd);
		require(map_addblock(sd) == 0, "support fixture ally is on map");
		return sd;
	};
	auto* owner = make_ally(2000001, 42, 7, false);
	auto* member = make_ally(2000002, 43, 7, false);
	auto* ambient = make_ally(POPULATION_ENGINE_ACCOUNT_ID_BASE + 2, POPULATION_ENGINE_CHAR_ID_BASE + 2,
		0x70000000, true);
	const auto find = [&](PopSkillCondition condition, int threshold, sc_type sc) {
		return population_test_find_support_target(shell, static_cast<int>(condition), threshold, static_cast<int>(sc));
	};
	require(find(PopSkillCondition::NotAllyStatus, 0, SC_BLESSING) == owner, "companion selects owner before ambient shells");
	owner->sc.createSCE(SC_BLESSING)->timer = INVALID_TIMER;
	require(find(PopSkillCondition::NotAllyStatus, 0, SC_BLESSING) == member, "already-buffed owner does not block party buffs");
	member->sc.createSCE(SC_BLESSING)->timer = INVALID_TIMER;
	require(find(PopSkillCondition::NotAllyStatus, 0, SC_BLESSING) == nullptr, "unrelated populations are not buff targets");
	ambient->battle_status.hp = 1; owner->battle_status.hp = 50; member->battle_status.hp = 20;
	require(find(PopSkillCondition::AllyHpBelow, 60, SC_NONE) == member, "healing chooses injured real party member");
	owner->battle_status.hp = 10;
	require(find(PopSkillCondition::AllyHpBelow, 60, SC_NONE) == owner, "healing chooses injured real owner");
	for (auto* sd : {owner, member, ambient}) { map_delblock(sd); map_deliddb(sd); }
	std::cout << "PASS: production companion buff/heal scans select the real owner and party instead of ambient shells" << std::endl;
}

static void compile_npc(const char* path) {
	std::ifstream file(path);
	require(file.good(), path);
	std::ostringstream content;
	content << file.rdbuf();
	const std::string source = content.str();
	const size_t start = source.find('{');
	const size_t end = source.rfind('}');
	require(start != std::string::npos && end > start, "NPC body exists");
	const std::string body = source.substr(start, end - start + 1);
	auto* code = parse_script(body.c_str(), path, 1, 0);
	require(code != nullptr, path);
	script_free_code(code);
	std::cout << "PASS: production script compiler accepts " << path << std::endl;
}

static void check_arena_menu_vm(map_session_data* sd) {
	std::ifstream file("npc/custom/population/arena.txt");
	std::ostringstream input; input << file.rdbuf();
	const std::string text = input.str();
	const auto begin = text.find("// BEGIN arena job choice");
	const auto end = text.find("// END arena job choice", begin);
	require(begin != std::string::npos && end > begin, "arena job-choice source markers exist");
	const auto choice = text.substr(begin, end - begin);
	sd->vars_ok = true;
	sd->regs.vars = i64db_alloc(DB_OPT_BASE);
	sd->npc_idle_timer = INVALID_TIMER;
	sd->state.ignoretimeout = true;
	for (int pick : {1, 2, 3}) {
		const std::string program = "{ .@nj=2; setarray .@ids$[0],\"4008\",\"4009\"; .@i=0; .@pick=" +
			std::to_string(pick) + ";\n" + choice + "\n@arena_test_result=.@jobs[0]; end; }";
		auto* code = parse_script(program.c_str(), "actual arena job choice", 1, 0);
		require(code != nullptr, "arena menu fixture compiles");
		run_script(code, 0, sd->id, 0);
		require(pc_readreg(sd, add_str("@arena_test_result")) == (pick == 1 ? 0 : pick == 2 ? 4008 : 4009),
			"Random Mix and explicit choices execute without negative array indexes");
		script_free_code(code);
	}
	// Menu construction rejects empty pools before any selection/array access.
	require(text.find("if (.@pool$ == \"\")") != std::string::npos, "empty arena job pools are rejected");
	auto* companion = new map_session_data();
	companion->pop.companion_owner = sd->status.char_id;
	g_population_engine_pcs.push_back(companion);
	require(population_engine_companion_count(sd) == 1, "owned companion is counted before arena entry");
	require(population_engine_arena_start("prontera", 1, 0, 0, 0, 1, sd->status.char_id) == 0,
		"arena start refuses an owner who has existing companions without removing them");
	require(g_population_engine_pcs.size() == 1 && companion->pop.companion_owner == sd->status.char_id,
		"arena rejection preserves the companion");
	g_population_engine_pcs.clear();
	std::cout << "PASS: actual arena Random Mix and explicit job branches execute in the production script VM" << std::endl;
}

static void check_arena_elimination(map_session_data* sd) {
	auto* owner = new map_session_data();
	owner->type = BL_PC;
	owner->id = owner->status.account_id = 2000003;
	owner->status.char_id = 44;
	owner->state.active = true;
	owner->m = sd->m; owner->mapindex = sd->mapindex;
	owner->battle_status.hp = owner->battle_status.max_hp = owner->status.hp = 100;
	map_addiddb(owner);
	sd->pop.arena_team = 1; sd->pop.arena_owner = 44;
	g_population_engine_pcs = {sd};
	require(population_engine_arena_result(map[sd->m].name, 44) == 0, "arena is running before knockout");
	pc_setdead(sd);
	sd->battle_status.hp = sd->status.hp = 0;
	population_engine_on_shell_death(sd);
	require(sd->pop.arena_eliminated && sd->pop.respawn_timer == INVALID_TIMER,
		"arena knockout never schedules a population respawn");
	require(status_revive(sd, 100, 100) == 0 && pc_isdead(sd), "production status_revive refuses an eliminated fighter");
	population_engine_respawn_shell_timer(INVALID_TIMER, gettick(), sd->id, 0);
	require(pc_isdead(sd) && sd->battle_status.hp == 0, "even an old population timer cannot revive arena fighters");
	require(population_engine_arena_result(map[sd->m].name, 44) == 1, "last enemy knockout wins the match");
	owner->pop.arena_eliminated = true;
	require(population_engine_arena_result(map[sd->m].name, 44) == -1, "owner death is latched as defeat");
	owner->pop.arena_eliminated = false; owner->m = sd->m + 1;
	require(population_engine_arena_result(map[sd->m].name, 44) == -1, "leaving the room forfeits the match");
	g_population_engine_pcs.clear(); map_deliddb(owner);
	// The next fixture cases use a living non-arena shell. Arena tags/latch are
	// removed only here, simulating release followed by a fresh match/spawn.
	sd->pop.arena_team = 0; sd->pop.arena_owner = 0; sd->pop.arena_eliminated = false;
	pc_setstand(sd, true); sd->battle_status.hp = sd->status.hp = 100;
	std::cout << "PASS: production arena knockouts stay dead, reject resurrection and resolve victory/defeat/forfeit" << std::endl;
}

static void check_arena_map_rallies() {
	std::map<std::string, std::tuple<int, int, std::vector<uint8>>> cached;
#ifdef RENEWAL
	const char* mode_cache = "db/re/map_cache.dat";
#else
	const char* mode_cache = "db/pre-re/map_cache.dat";
#endif
	for (const char* path : {"db/map_cache.dat", mode_cache, "db/import/map_cache.dat"}) {
		std::ifstream file(path, std::ios::binary);
		if (!file) continue; // db/import is optional on a clean checkout.
		std::vector<uint8> bytes((std::istreambuf_iterator<char>(file)), {});
		require(bytes.size() >= 8, "map cache header exists");
		const auto u16 = [&](size_t at) { uint16 value; std::memcpy(&value, bytes.data()+at, 2); return value; };
		const auto u32 = [&](size_t at) { uint32 value; std::memcpy(&value, bytes.data()+at, 4); return value; };
		size_t cursor = 8;
		for (unsigned i = 0; i < u16(4); ++i) {
			require(cursor + 20 <= bytes.size(), "map cache entry header is complete");
			char name[13]{}; std::memcpy(name, bytes.data()+cursor, 12);
			const int width = u16(cursor+12), height = u16(cursor+14);
			const uint32 length = u32(cursor+16); cursor += 20;
			require(cursor + length <= bytes.size(), "map cache entry data is complete");
			if (std::string(name).find("pvp_y_1-") == 0) {
				std::vector<uint8> cells(width*height);
				unsigned long size = static_cast<unsigned long>(cells.size());
				require(decode_zip(cells.data(), &size, bytes.data()+cursor, length) == 0 && size == cells.size(),
					"arena map cells decompress");
				cached[name] = {width, height, std::move(cells)};
			}
			cursor += length;
		}
	}
	for (int i = 1; i <= 5; ++i) {
		const std::string name = "pvp_y_1-" + std::to_string(i);
		require(cached.count(name) != 0, "all five arena maps are cached");
		auto& [width, height, cells] = cached[name];
		auto& md = map[2]; md.m = 2; md.xs = width; md.ys = height;
		safestrncpy(md.name, name.c_str(), sizeof(md.name)); md.index = mapindex_name2id(name.c_str());
		md.cell = new mapcell[width*height]{};
		for (int cell = 0; cell < width*height; ++cell)
			md.cell[cell].walkable = md.cell[cell].shootable = cells[cell] == 0 || cells[cell] == 3 || cells[cell] == 6;
		population_warp_test_register_map(2);
		int16 ax = 0, ay = 0, bx = 0, by = 0;
		require(population_engine_arena_rally(name.c_str(), 1, bx, by) &&
			population_engine_arena_rally(name.c_str(), 2, ax, ay), "actual arena has valid team rallies");
		require(path_search_long(nullptr, 2, ax, ay, bx, by, CELL_CHKWALL), "actual arena rally lane is visible");
		delete[] md.cell; md.cell = nullptr;
	}
	std::cout << "PASS: all five shipped arena map caches provide valid opposing team rallies" << std::endl;
}

static void check_clientless_warps() {
	population_warp_test_init_registries();
	mapindex_init();
	do_init_party();
	battle_config.dynamic_mobs = 0;
	battle_config.clear_unit_onwarp = 0;
	map_num = 2;
	for (int16 m = 0; m < 2; ++m) {
		auto& md = map[m];
		md.m = m; md.xs = md.ys = 80; md.bxs = md.bys = 10;
		safestrncpy(md.name, m == 0 ? "prontera" : "geffen", sizeof(md.name));
		md.index = warp_map_indexes[m] = mapindex_name2id(md.name);
		require(md.index != 0, "fixture map index exists");
		md.cell = new mapcell[80 * 80]{};
		for (int i = 0; i < 80 * 80; ++i) md.cell[i].walkable = md.cell[i].shootable = true;
		md.block = new block_list*[100]{};
		md.block_mob = new block_list*[100]{};
		md.initMapFlags();
		population_warp_test_register_map(m);
	}
	auto* sd = new map_session_data();
	sd->type = BL_PC;
	sd->id = sd->status.account_id = POPULATION_ENGINE_ACCOUNT_ID_BASE + 1;
	sd->status.char_id = POPULATION_ENGINE_CHAR_ID_BASE + 1;
	sd->status.party_id = 7;
	sd->pop.companion_owner = 42;
	sd->state.active = true;
	sd->state.population_combat = true;
	sd->pop.flags |= PSF::CombatActive;
	sd->group = std::make_shared<s_player_group>();
	sd->pvp_timer = sd->followtimer = sd->invincible_timer = INVALID_TIMER;
	sd->status.hp = sd->battle_status.hp = sd->battle_status.max_hp = 100;
	safestrncpy(sd->status.name, "Warp Companion", sizeof(sd->status.name));
	sd->m = 0; sd->mapindex = warp_map_indexes[0]; sd->x = sd->y = 20;
	unit_dataset(sd);
	sd->vd.look[LOOK_BASE] = JOB_NOVICE;
	map_addiddb(sd);
	require(map_addblock(sd) == 0, "fixture shell attaches initially");
	map[0].users = map[0].users_pvp = 1;
	check_arena_map_rallies();
	check_arena_menu_vm(sd);
	int16 ax = -1, ay = -1, bx = -1, by = -1;
	require(population_engine_arena_rally("prontera", 2, ax, ay) &&
		population_engine_arena_rally("prontera", 1, bx, by), "both team rally positions resolve");
	require(map_getcell(0, ax, ay, CELL_CHKPASS) && map_getcell(0, bx, by, CELL_CHKPASS) &&
		path_search_long(nullptr, 0, ax, ay, bx, by, CELL_CHKWALL), "team rallies share a walkable visible lane");
	require(std::max(std::abs(ax-bx), std::abs(ay-by)) == 14, "teams start fourteen cells apart");
	std::cout << "PASS: production arena team rally positions are separate, passable and mutually visible" << std::endl;
	check_arena_elimination(sd);

	for (int i = 0; i < 12; ++i) {
		// Two consecutive transfers to each map also exercise Fly Wing-style
		// teleports where changemap is false, not only cross-map movement.
		const int16 destination = (i / 2) % 2;
		sd->pop.target_id = 900;
		sd->pop.path.emplace_back(60, 60);
		sd->pop.skill_next_use_tick[1] = 9999;
		require(pc_setpos(sd, warp_map_indexes[destination], 30 + i, 35, CLR_TELEPORT) == SETPOS_OK,
			"production pc_setpos succeeds");
		require(sd->prev != nullptr && !sd->state.warping && !sd->state.rewarp && !sd->state.changemap,
			"clientless warp is fully attached before pc_setpos returns");
		require(sd->m == destination && sd->x == 30 + i && sd->y == 35, "warp destination is retained");
		require(map[0].users + map[1].users == 1 && map[0].users_pvp + map[1].users_pvp == 1,
			"map users balance over repeated warps");
		require(sd->status.party_id == 7 && sd->pop.companion_owner == 42 && sd->state.population_combat,
			"party, owner and combat state survive");
		require(sd->pop.target_id == 0 && sd->pop.path.empty() && sd->pop.skill_next_use_tick[1] == 9999,
			"old-map navigation is cleared without resetting skill cooldowns");
		require(population_engine_combat_shell_ac_ok(sd), "stale-shell sweep still considers the companion live");
	}
	const int16 previous_map = sd->m;
	const uint16 remote = static_cast<uint16>(mapindex_addmap(-1, "pop_remote"));
	require(pc_setpos(sd, remote, 20, 20, CLR_TELEPORT) == SETPOS_NO_MAPSERVER,
		"population shell cannot transfer to char-server authentication");
	require(sd->prev && sd->m == previous_map && !sd->state.warping, "rejected remote warp leaves shell attached");
	check_ammo_and_support(sd);
	unit_remove_map(sd, CLR_OUTSIGHT);
	require(map[0].users + map[1].users == 0 && map[0].users_pvp + map[1].users_pvp == 0,
		"final removal balances counters");
	map_deliddb(sd);
	do_final_party();
	std::cout << "PASS: production pc_setpos preserves a companion across 12 same/cross-map warps without client ACK" << std::endl;
}

int main(int argc, char** argv) {
	const bool preset = argc == 2 && std::string(argv[1]) == "--preset";
	// Do not start the server core, sockets, timers, mapreg persistence or SQL.
	malloc_init();
	db_init();
	timer_init();
	battle_set_defaults();
	st_db = idb_alloc(DB_OPT_BASE);
	userfunc_db = strdb_alloc(DB_OPT_DUP_KEY, 0);
	scriptlabel_db = strdb_alloc(DB_OPT_DUP_KEY, 50);
	autobonus_db = strdb_alloc(DB_OPT_DUP_KEY, 0);
	st_ers = ers_new(sizeof(script_state), "population startup states", ERS_CACHE_OPTIONS);
	stack_ers = ers_new(sizeof(script_stack), "population startup stacks", ERS_OPT_FLEX_CHUNK);
	array_ers = ers_new(sizeof(script_array), "population startup arrays", ERS_CLEAN_OPTIONS);
	add_buildin_func();
	constant_db.load();
	script_hardcoded_constants();

	for (const char* name : {"population_companion_recruit", "population_companion_dismiss", "population_hangout_spawn"}) {
		const int id = search_str(name);
		require(id >= LABEL_START && str_data[id].type == C_FUNC, name);
	}
	std::cout << "PASS: all companion and hangout builtins are registered" << std::endl;
	compile_npc("npc/custom/population/companions.txt");
	compile_npc("npc/custom/population/arena.txt");

	require(job_db.load(), "job database loads");
	require(item_db.load(), "item database loads");
	require(population_shared_db().load(), "shared population gear loads");
	require(population_engine_db().load(), "main population database loads");
	require(population_pvp_db().load(), "arena population database loads");
	require(population_vendor_pop_db().load(), "vendor population database loads");
	require(population_spawn_db().load(), "population spawn database loads");
	require(population_engine_db().size() >= 20, "ambient job profiles load");
#ifdef RENEWAL
	require(population_engine_db().jobs_with_profile("third_jobs").size() == 26, "Renewal main third-job catalog loads");
	require(population_pvp_db().jobs_with_profile("third_jobs").size() == 26, "Renewal arena third-job catalog loads");
#else
	require(population_engine_db().jobs_with_profile("third_jobs").empty(), "pre-renewal skips main third-job profile");
	require(population_pvp_db().jobs_with_profile("third_jobs").empty(), "pre-renewal skips arena third-job profile");
#endif
	uint32_t total = 0;
	for (const auto& pair : population_spawn_db()) {
		require(!population_engine_db().jobs_with_profile(pair.second->profile_name).empty(), "every spawn profile resolves to jobs");
		total += pair.second->towns_population + pair.second->fields_population + pair.second->dungeons_population;
	}
	require(total > 0, "ambient population targets load");
	if (preset) {
		require(total == 4000 && population_engine_db().size() == 46,
			"optional preset loads all 46 pre-renewal jobs and the exact 4000-shell budget");
	}
	std::cout << "PASS: production YAML loaders accept the active-mode job catalog and spawn profiles" << std::endl;
	check_clientless_warps();
	return 0;
}