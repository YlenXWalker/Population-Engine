#include "test_common.hpp"

using t_itemid = uint32_t;
enum { IT_AMMO, IT_WEAPON, EQP_AMMO = 32, EQI_AMMO = 0, LOG_TYPE_NONE = 0, MAX_INVENTORY = 4 };
enum e_additem_result { ADDITEM_SUCCESS, ADDITEM_FAIL };
struct item { t_itemid nameid = 0; int amount = 0, identify = 0; unsigned equip = 0; };
struct item_data { int type = IT_AMMO, weight = 1; unsigned equip = EQP_AMMO; };
struct map_session_data {
	int id = 95000001, max_weight = 1000, weight = 0;
	struct { char name[24] = "Archer"; int inventory_slots = MAX_INVENTORY; } status;
	struct { struct { item items_inventory[MAX_INVENTORY]; } u; } inventory;
	int equip_index[1] = {-1};
};
struct Database {
	std::shared_ptr<item_data> data = std::make_shared<item_data>();
	std::shared_ptr<item_data> find(t_itemid id) { return id == 1768 ? data : nullptr; }
} item_db;
int adds = 0, equips = 0, last_amount = 0;
bool allow_add = true, allow_equip = true;
#define nullpo_retr(value, pointer) do { if (!(pointer)) return value; } while (false)
e_additem_result pc_additem(map_session_data* sd, item* input, int amount, int, bool) {
	++adds; last_amount = amount;
	if (!allow_add) return ADDITEM_FAIL;
	for (auto& slot : sd->inventory.u.items_inventory) {
		if (slot.nameid == input->nameid || !slot.nameid) {
			slot.nameid = input->nameid; slot.amount += amount; return ADDITEM_SUCCESS;
		}
	}
	return ADDITEM_FAIL;
}
bool pc_equipitem(map_session_data* sd, int index, unsigned pos, bool) {
	++equips;
	auto& slot = sd->inventory.u.items_inventory[index];
	if (!allow_equip || slot.equip) return false;
	slot.equip = pos; sd->equip_index[EQI_AMMO] = index; return true;
}

// @production

int main() {
	map_session_data sd;
	CHECK(script_give_equip_ammo(&sd, "setarrow", 1768));
	CHECK(adds == 1 && equips == 1 && sd.equip_index[EQI_AMMO] == 0);
	CHECK(script_give_equip_ammo(&sd, "setarrow", 1768));
	CHECK(adds == 1 && equips == 1 && sd.inventory.u.items_inventory[0].amount == 500);
	sd.inventory.u.items_inventory[0].equip = 0; sd.equip_index[0] = -1;
	CHECK(script_give_equip_ammo(&sd, "setarrow", 1768));
	CHECK(adds == 1 && equips == 2);
	sd.inventory.u.items_inventory[0].amount = 0;
	sd.inventory.u.items_inventory[0].equip = 0; sd.equip_index[0] = -1;
	CHECK(script_give_equip_ammo(&sd, "setarrow", 1768));
	CHECK(adds == 2 && equips == 3 && last_amount == 500);
	CHECK(!script_give_equip_ammo(&sd, "setarrow", 9999));
	sd = {}; sd.max_weight = 8; sd.weight = 5;
	CHECK(script_give_equip_ammo(&sd, "setarrow", 1768));
	CHECK(last_amount == 3);
	sd = {}; sd.max_weight = sd.weight = 5;
	const int previous = adds;
	CHECK(!script_give_equip_ammo(&sd, "setarrow", 1768));
	CHECK(adds == previous);
	return 0;
}