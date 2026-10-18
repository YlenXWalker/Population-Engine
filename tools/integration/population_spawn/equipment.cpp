#include "test_common.hpp"

#define RENEWAL
using t_itemid = uint32_t;
constexpr int MAX_INVENTORY = 24, EQI_AMMO = 0, IT_WEAPON = 1, LOG_TYPE_NONE = 0;
constexpr int LOOK_SHIELD = 1, LOOK_HEAD_TOP = 2, LOOK_HEAD_MID = 3, LOOK_HEAD_BOTTOM = 4, LOOK_ROBE = 5;
enum e_ammo_type { AMMO_ARROW, AMMO_BULLET, AMMO_SHELL, AMMO_GRENADE, AMMO_KUNAI };
enum e_additem_result { ADDITEM_SUCCESS, ADDITEM_FAILED };
enum { W_BOW = 1, W_MUSICAL, W_WHIP, W_REVOLVER, W_RIFLE, W_GATLING, W_SHOTGUN, W_GRENADE, W_HUUMA };
struct script_code {};
struct item { t_itemid nameid = 0; int amount = 0, identify = 0; unsigned equip = 0; };
struct item_data { int type = IT_WEAPON, subtype = W_BOW, weight = 5; unsigned equip = 8; };
struct map_session_data {
	int max_weight = 0, weight = 10;
	struct { struct { item items_inventory[MAX_INVENTORY]; } u; } inventory;
	struct { int look[8]{}; } vd;
	struct { int shield = 0, head_top = 0, head_mid = 0, head_bottom = 0, robe = 0; } status;
	int equip_index[8]{};
};
std::map<t_itemid, item_data> items;
std::vector<std::pair<t_itemid, int>> additions;
int equips = 0;
t_itemid rejected = 0;
bool allow_equip = true;
item_data* itemdb_search(t_itemid id) { auto it = items.find(id); return it == items.end() ? nullptr : &it->second; }
e_additem_result pc_additem(map_session_data* sd, item* value, int amount, int, bool) {
	additions.emplace_back(value->nameid, amount);
	if (value->nameid == rejected) return ADDITEM_FAILED;
	for (auto& slot : sd->inventory.u.items_inventory) if (!slot.nameid) { slot = *value; slot.amount = amount; return ADDITEM_SUCCESS; }
	return ADDITEM_FAILED;
}
bool pc_equipitem(map_session_data* sd, int slot, unsigned pos, bool) {
	++equips;
	if (allow_equip) { sd->inventory.u.items_inventory[slot].equip = pos; sd->max_weight = 100; }
	return allow_equip;
}
t_itemid find_arrow_ammo_item() { return 2001; }
t_itemid find_ammo_item_by_subtype(e_ammo_type type) { return 2001 + type; }

// @production

int main() {
	items[1000] = {}; items[2001] = {};
	map_session_data sd;
	population_engine_equip_shell_weapon(&sd, 1000, 42);
	CHECK(equips == 1 && sd.inventory.u.items_inventory[0].equip == 8);
	population_engine_equip_shell_ammo(&sd, 1000, false, nullptr, 42);
	CHECK(additions.back() == std::make_pair(t_itemid{2001}, 18));
	CHECK(equips == 1 && sd.equip_index[EQI_AMMO] == 1 && sd.inventory.u.items_inventory[1].equip == 8);
	const auto count = additions.size(); script_code script;
	population_engine_equip_shell_ammo(&sd, 1000, true, nullptr, 42);
	population_engine_equip_shell_ammo(&sd, 1000, false, &script, 42);
	CHECK(additions.size() == count);
	for (int subtype = W_BOW; subtype <= W_HUUMA; ++subtype) {
		items[1000].subtype = subtype;
		int ammo = subtype <= W_WHIP ? AMMO_ARROW : subtype <= W_GATLING ? AMMO_BULLET
			: subtype == W_SHOTGUN ? AMMO_SHELL : subtype == W_GRENADE ? AMMO_GRENADE : AMMO_KUNAI;
		items[2001 + ammo] = {}; sd = {}; sd.max_weight = 1; sd.weight = 10;
		population_engine_equip_shell_ammo(&sd, 1000, false, nullptr, 42);
		CHECK(additions.back() == std::make_pair(t_itemid(2001 + ammo), 1));
	}
	for (int id = 3001; id <= 3005; ++id) items[id] = {};
	sd = {}; population_engine_equip_shell_armor(&sd, 3001, 3002, 3003, 3004, 3005);
	CHECK(sd.status.shield == 3001 && sd.status.head_top == 3002 && sd.status.head_mid == 3003);
	CHECK(sd.status.head_bottom == 3004 && sd.status.robe == 3005 && sd.vd.look[LOOK_ROBE] == 3005);
	sd = {}; allow_equip = false;
	population_engine_equip_shell_armor(&sd, 3001, 3002, 3003, 3004, 3005);
	CHECK(!sd.status.shield && !sd.status.head_top && !sd.status.head_mid && !sd.status.head_bottom && !sd.status.robe);
	allow_equip = true; rejected = 1000; sd = {}; const int previous = equips;
	population_engine_equip_shell_weapon(&sd, 1000, 42);
	CHECK(equips == previous && !sd.inventory.u.items_inventory[0].nameid);
	return 0;
}
