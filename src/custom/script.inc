// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

/**
 * Custom Script Commands
 * Place the body of custom script commands in this file.
 * Format:
 *	BUILDIN_FUNC(command_name)
 *	{
 *		<code>;
 *	}
 **/

//BUILDIN_FUNC(example)
//{
//	ShowInfo("buildin_example: It works!\n");
//	script_pushint(st,1);
//	return 0;
//}

/*==========================================
 * Population / default ammo equip (skill require.ammo support)
 *------------------------------------------*/

/// Right-hand weapon subtype (mmo_charstatus.weapon may still be a stale item id on some spawn paths).
static enum weapon_type script_handr_weapon_subtype(map_session_data *sd)
{
	nullpo_retr(W_FIST, sd);
	pc_setinventorydata(*sd);
	pc_calcweapontype(sd);
	const int16 idx = sd->equip_index[EQI_HAND_R];
	if (idx >= 0 && sd->inventory_data[idx] && sd->inventory_data[idx]->type == IT_WEAPON)
		return (enum weapon_type)sd->inventory_data[idx]->subtype;
	if (sd->weapontype1 != W_FIST)
		return (enum weapon_type)sd->weapontype1;
	return W_FIST;
}

static t_itemid script_find_first_ammo_item(enum e_ammo_type ammo_subtype)
{
	for (const auto& it : item_db) {
		std::shared_ptr<item_data> id = it.second;
		if (!id || id->nameid == UNKNOWN_ITEM_ID)
			continue;
		if (id->type == IT_AMMO && id->subtype == ammo_subtype && id->equip && (id->equip & EQP_AMMO))
			return id->nameid;
	}
	return 0;
}

static bool script_give_equip_ammo(map_session_data *sd, const char *cmd, t_itemid ammo_id)
{
	nullpo_retr(false, sd);
	nullpo_retr(false, cmd);
	std::shared_ptr<item_data> aid = item_db.find(ammo_id);
	if (!aid || !aid->equip) {
		ShowWarning("%s: ammo item %u missing or has no equip flags (%s id=%d)\n",
			cmd, (unsigned)ammo_id, sd->status.name, sd->id);
		return false;
	}
	struct item tmp_item = {};
	tmp_item.nameid = ammo_id;
	tmp_item.amount = 500;
	tmp_item.identify = 1;
	tmp_item.equip = 0;
	enum e_additem_result ares = pc_additem(sd, &tmp_item, 500, LOG_TYPE_NONE, false);
	if (ares != ADDITEM_SUCCESS) {
		ShowWarning("%s: pc_additem ammo %u failed result=%d (%s id=%d max_w=%d cur_w=%d slots=%d)\n",
			cmd, (unsigned)ammo_id, (int)ares, sd->status.name, sd->id,
			sd->max_weight, sd->weight, sd->status.inventory_slots);
		return false;
	}
	for (int16 i = 0; i < MAX_INVENTORY; i++) {
		if (sd->inventory.u.items_inventory[i].nameid == ammo_id && sd->inventory.u.items_inventory[i].amount > 0) {
			if (!pc_equipitem(sd, i, aid->equip, false)) {
				ShowWarning("%s: pc_equipitem failed inv=%d ammo=%u (%s id=%d)\n",
					cmd, (int)i, (unsigned)ammo_id, sd->status.name, sd->id);
				return false;
			}
			return true;
		}
	}
	ShowWarning("%s: stack missing after additem ammo=%u (%s id=%d)\n", cmd, (unsigned)ammo_id, sd->status.name, sd->id);
	return false;
}

/// Equips default ammunition for ranged / huuma jobs so combat skills can pass ammo checks.
///
/// Many weapon skills use skill_db `Ammo` + `AmmoAmount` (validated in
/// skill_check_condition_castend: require.ammo vs EQI_AMMO item subtype and count).
///
/// setarrow {<char_id>};
BUILDIN_FUNC(setarrow)
{
	TBL_PC* sd;

	if (!script_charid2sd(2, sd))
		return SCRIPT_CMD_FAILURE;

	const enum weapon_type wt = script_handr_weapon_subtype(sd);

	if (wt != W_BOW && wt != W_MUSICAL && wt != W_WHIP)
		return SCRIPT_CMD_SUCCESS;

	t_itemid arrow_id = script_find_first_ammo_item(AMMO_ARROW);
	if (arrow_id == 0)
		arrow_id = 1750; // Steel Arrow fallback

	script_give_equip_ammo(sd, "setarrow", arrow_id);

	return SCRIPT_CMD_SUCCESS;
}

/// setbullet {<char_id>};
BUILDIN_FUNC(setbullet)
{
	TBL_PC* sd;

	if (!script_charid2sd(2, sd))
		return SCRIPT_CMD_FAILURE;

	const enum weapon_type wt = script_handr_weapon_subtype(sd);

	t_itemid ammo_id = 0;
	switch (wt) {
	case W_REVOLVER:
	case W_RIFLE:
	case W_GATLING:
		ammo_id = script_find_first_ammo_item(AMMO_BULLET);
		break;
	case W_SHOTGUN:
		ammo_id = script_find_first_ammo_item(AMMO_SHELL);
		break;
#ifdef RENEWAL
	case W_GRENADE:
		ammo_id = script_find_first_ammo_item(AMMO_GRENADE);
		break;
#endif
	default:
		return SCRIPT_CMD_SUCCESS;
	}

	if (ammo_id == 0) {
		ShowWarning("setbullet: no ammo in item_db for weapon subtype %u (%s id=%d)\n", (unsigned)wt, sd->status.name, sd->id);
		return SCRIPT_CMD_SUCCESS;
	}

	script_give_equip_ammo(sd, "setbullet", ammo_id);

	return SCRIPT_CMD_SUCCESS;
}

/// setkunai {<char_id>};
BUILDIN_FUNC(setkunai)
{
	TBL_PC* sd;

	if (!script_charid2sd(2, sd))
		return SCRIPT_CMD_FAILURE;

	const enum weapon_type wt = script_handr_weapon_subtype(sd);

	if (wt != W_HUUMA)
		return SCRIPT_CMD_SUCCESS;

	t_itemid kunai_id = script_find_first_ammo_item(AMMO_KUNAI);
	if (kunai_id == 0) {
		ShowWarning("setkunai: no AMMO_KUNAI with EQP_AMMO in item_db (%s id=%d)\n", sd->status.name, sd->id);
		return SCRIPT_CMD_SUCCESS;
	}

	script_give_equip_ammo(sd, "setkunai", kunai_id);

	return SCRIPT_CMD_SUCCESS;
}

/*==========================================
 * Population Engine Arena Commands
 * population_arena_start("<map>", <shell_count>{, <spawn_x>, <spawn_y>})
 *   Spawns shell_count shells on <map> (must have PvP mode enabled).
 *   Shells target real (non-shell) players so a player can fight/observe AI behaviour.
 *   spawn_x/spawn_y = optional spawn center (0 = random spread).
 *   Returns shells spawned or 0 on error.
 * population_arena_stop("<map>")
 *   Releases all arena shells on <map>.
 *------------------------------------------*/

BUILDIN_FUNC(population_arena_start)
{
	const char* map_name = script_getstr(st, 2);
	const int shell_count = script_getnum(st, 3);

	if (shell_count < 1 || shell_count > 25) {
		ShowError("buildin_population_arena_start: shell_count %d out of range [1,25].\n", shell_count);
		script_pushint(st, 0);
		return SCRIPT_CMD_FAILURE;
	}

	const int sx = script_hasdata(st, 4) ? script_getnum(st, 4) : 0;
	const int sy = script_hasdata(st, 5) ? script_getnum(st, 5) : 0;
	const int job_arg = script_hasdata(st, 6) ? script_getnum(st, 6) : 0;
	if (job_arg < 0 || job_arg > 0xFFFF) {
		ShowError("buildin_population_arena_start: job_id %d out of range [0,65535].\n", job_arg);
		script_pushint(st, 0);
		return SCRIPT_CMD_FAILURE;
	}
	// team_id: 1 = enemy shells (target real players), 2 = allied shells (target team-1 shells).
	const int team_arg = script_hasdata(st, 7) ? script_getnum(st, 7) : 1;

	const int spawned = population_engine_arena_start(map_name, shell_count, sx, sy,
		static_cast<uint16_t>(job_arg), team_arg);
	script_pushint(st, spawned);
	return SCRIPT_CMD_SUCCESS;
}

BUILDIN_FUNC(population_arena_stop)
{
	const char* map_name = script_getstr(st, 2);
	population_engine_arena_stop(map_name);
	return SCRIPT_CMD_SUCCESS;
}

/*==========================================
 * population_arena_jobs()
 *   Returns a CSV string of job IDs in the PvP arena pool
 *   (auto-derived from db/population_pvp.yml). Empty string if no
 *   pool is loaded. Use explode() + jobname() in script to render.
 *------------------------------------------*/
BUILDIN_FUNC(population_arena_jobs)
{
	const std::vector<uint16_t> pool = population_engine_arena_job_pool();
	std::string csv;
	csv.reserve(pool.size() * 6);
	for (size_t i = 0; i < pool.size(); ++i) {
		if (i != 0) csv.push_back(',');
		csv.append(std::to_string(static_cast<unsigned>(pool[i])));
	}
	script_pushstrcopy(st, csv.c_str());
	return SCRIPT_CMD_SUCCESS;
}

BUILDIN_FUNC(population_companion_recruit) {
	map_session_data* sd;
	if (!script_rid2sd(sd)) { script_pushint(st, 0); return SCRIPT_CMD_FAILURE; }
	int job = script_getnum(st, 2);
	script_pushint(st, job > 0 && job <= UINT16_MAX ? population_engine_companion_recruit(sd, static_cast<uint16_t>(job)) : 0);
	return SCRIPT_CMD_SUCCESS;
}
BUILDIN_FUNC(population_companion_dismiss) {
	map_session_data* sd;
	if (!script_rid2sd(sd)) { script_pushint(st, 0); return SCRIPT_CMD_FAILURE; }
	script_pushint(st, population_engine_companion_dismiss(sd));
	return SCRIPT_CMD_SUCCESS;
}
BUILDIN_FUNC(population_hangout_spawn) {
	map_session_data* sd;
	if (!script_rid2sd(sd)) { script_pushint(st, 0); return SCRIPT_CMD_FAILURE; }
	int job = script_getnum(st, 2);
	// Placement is an administrator tool; ordinary players cannot populate maps.
	script_pushint(st, sd->group->level >= 99 && job > 0 && job <= UINT16_MAX ? population_engine_hangout_spawn(sd, static_cast<uint16_t>(job)) : 0);
	return SCRIPT_CMD_SUCCESS;
}
