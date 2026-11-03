#include "test_common.hpp"
#include <climits>

using t_itemid = uint32_t;
enum e_ammo_type { AMMO_ARROW, AMMO_BULLET };
enum { IT_AMMO, IT_WEAPON, EQP_AMMO = 32, UNKNOWN_ITEM_ID = 512, ITEM_EQUIP_ACK_OK = 0 };
struct item_data {
	t_itemid nameid = 0;
	int type = IT_AMMO, subtype = AMMO_ARROW, weight = 1, equip = EQP_AMMO;
	int min_level = 1, max_level = 0, sex = -1, job = 1;
	bool map_blocked = false;
};
struct map_session_data { int level = 10, job = 1, sex = 1; };
std::map<t_itemid, std::shared_ptr<item_data>> item_db;
int pc_isequip_item(const map_session_data* sd, const item_data* item) {
	return sd && sd->level >= item->min_level && (!item->max_level || sd->level <= item->max_level) &&
		(sd->job == item->job) && (item->sex < 0 || sd->sex == item->sex) && !item->map_blocked ? 0 : 1;
}

// @production

int main() {
	map_session_data sd;
	for (int id : {1750, 1768, 1770}) {
		auto item = std::make_shared<item_data>(); item->nameid = id;
		item_db[id] = item;
	}
	CHECK(script_find_first_ammo_item(&sd, AMMO_ARROW) == 1750);
	item_db[1750]->job = 2;
	CHECK(script_find_first_ammo_item(&sd, AMMO_ARROW) == 1768);
	item_db[1768]->min_level = 40;
	CHECK(script_find_first_ammo_item(&sd, AMMO_ARROW) == 1770);
	item_db[1770]->map_blocked = true;
	CHECK(script_find_first_ammo_item(&sd, AMMO_ARROW) == 0);
	item_db[1768]->min_level = 1; item_db[1768]->sex = 0;
	CHECK(script_find_first_ammo_item(&sd, AMMO_ARROW) == 0);
	item_db[1768]->sex = 1; item_db[1768]->max_level = 9;
	CHECK(script_find_first_ammo_item(&sd, AMMO_ARROW) == 0);
	item_db[1768]->max_level = 0;
	CHECK(script_find_first_ammo_item(&sd, AMMO_ARROW) == 1768);
	item_db[1750]->job = 1; item_db[1750]->weight = 5;
	CHECK(script_find_first_ammo_item(&sd, AMMO_ARROW) == 1768);
	CHECK(script_find_first_ammo_item(&sd, AMMO_BULLET) == 0);
	CHECK(script_find_first_ammo_item(nullptr, AMMO_ARROW) == 0);
	return 0;
}