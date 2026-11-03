#include "test_common.hpp"
#include <climits>

using uint8 = uint8_t;
using t_itemid = uint32_t;
constexpr int MC_VENDING = 1, ADDSKILL_PERMANENT_GRANTED = 1, MAX_CART = 12, MAX_VENDING = 12, LOG_TYPE_NONE = 0;
constexpr int ADDITEM_SUCCESS = 0;
struct PopulationShellVendorStockItem { t_itemid nameid; int16 amount; uint32_t price_override; };
struct item { t_itemid nameid = 0; int amount = 0, identify = 0; };
struct item_data { int equip = 0, value_buy = 0; };
struct map_session_data {
	int cart_weight_max = 0;
	struct { struct { item items_cart[MAX_CART]; } u; } cart;
	struct { bool prevend = false; } state;
};
struct {
	std::map<t_itemid, std::shared_ptr<item_data>> entries;
	std::shared_ptr<item_data> find(t_itemid id) { auto it = entries.find(id); return it == entries.end() ? nullptr : it->second; }
} item_db;
bool valid = true;
int grants = 0, carts = 0, opens = 0;
std::vector<t_itemid> additions;
std::vector<uint8> packet;
void pc_skill(map_session_data*, int skill, int level, int flag) {
	++grants; valid &= skill == MC_VENDING && level == 10 && flag == ADDSKILL_PERMANENT_GRANTED;
}
void pc_setcart(map_session_data*, int value) { ++carts; valid &= value == 1; }
int pc_cart_additem(map_session_data* sd, item* value, int amount, int) {
	valid &= sd->cart_weight_max == INT32_MAX && value->amount == 1 && value->identify == 1;
	additions.push_back(value->nameid);
	if (value->nameid == 3) return 1;
	for (auto& slot : sd->cart.u.items_cart) if (!slot.nameid) { slot = *value; slot.amount = amount; return ADDITEM_SUCCESS; }
	return 1;
}
void vending_openvending(map_session_data& sd, const char* title, uint8* data, int count, void*) {
	++opens; valid &= sd.state.prevend && std::string(title) == "Test shop";
	packet.assign(data, data + count * 8);
}
uint16 read16(int offset) { uint16 value; std::memcpy(&value, packet.data() + offset, sizeof(value)); return value; }
uint32 read32(int offset) { uint32 value; std::memcpy(&value, packet.data() + offset, sizeof(value)); return value; }

// @production

int main() {
	for (int id = 1; id <= 6; ++id) item_db.entries[id] = std::make_shared<item_data>(item_data{0, 500});
	item_db.entries[1]->equip = 1; item_db.entries[4]->value_buy = 0;
	map_session_data sd{};
	std::vector<PopulationShellVendorStockItem> stock{{99, 5, 0}, {1, 99, 0}, {2, 9, 0}, {3, 10, 0}, {4, 4, 0}, {5, 2, 42}, {6, 1, 0}};
	population_engine_open_shell_vendor(&sd, stock, 4, "Test shop");
	CHECK(valid && grants == 1 && carts == 1 && opens == 1 && packet.size() == 32);
	CHECK((additions == std::vector<t_itemid>{1, 2, 3, 4, 5}));
	CHECK(read16(0) == 2 && read16(2) == 1 && read32(4) == 500);
	CHECK(read16(8) == 3 && read16(10) == 9 && read32(12) == 500);
	CHECK(read16(16) == 4 && read16(18) == 4 && read32(20) == 100);
	CHECK(read16(24) == 5 && read16(26) == 2 && read32(28) == 42);
	sd = {}; stock.clear(); packet.clear();
	population_engine_open_shell_vendor(&sd, stock, 4, "Test shop");
	CHECK(opens == 1 && !sd.state.prevend);
	return 0;
}
