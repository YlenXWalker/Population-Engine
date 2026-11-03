#include "test_common.hpp"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
using int16 = int16_t;
enum class PopulationBehavior { None, Combat, Vendor };
struct PopulationEngine {
	uint16_t job_id = 0;
	std::string source_profile_name, vendor_key;
	PopulationBehavior behavior = PopulationBehavior::Vendor, town_behavior = PopulationBehavior::None;
};
struct PopulationVendorPlacement { std::string map; int max_vendors = 0; };
struct VendorDatabase {
	std::unordered_map<std::string, std::unordered_map<std::string, PopulationVendorPlacement>> entries;
	const auto& placements() const { return entries; }
} vendors;
auto& population_vendor_db() { return vendors; }
int16 map_mapname2mapid(const char* map) {
	return std::string(map) == "prontera" ? 1 : std::string(map) == "prt_in" ? 2 : -1;
}
struct Session {
	int16 m;
	struct { std::shared_ptr<PopulationEngine> profile_config; } pop;
};
std::vector<Session*> g_population_engine_pcs;

// @production

int main() {
	std::vector<std::shared_ptr<PopulationEngine>> profiles;
	for (int i = 0; i < 120; ++i) {
		auto profile = std::make_shared<PopulationEngine>();
		profile->source_profile_name = "shop_" + std::to_string(i);
		profile->vendor_key = profile->source_profile_name;
		population_store_shell_profile(profiles, 5, profile); // all Merchant
		vendors.entries[profile->vendor_key]["prontera"] = {"prontera", 1};
	}
	CHECK(profiles.size() == 120);
	// Replacing one profile does not delete the other 119 sharing its job.
	auto replacement = std::make_shared<PopulationEngine>(*profiles[37]);
	population_store_shell_profile(profiles, 5, replacement);
	CHECK(profiles.size() == 120);
	CHECK(profiles.back() == replacement);
	std::unordered_set<std::string> excluded;
	std::vector<std::unique_ptr<Session>> sessions;
	for (int i = 0; i < 120; ++i) {
		std::shared_ptr<PopulationEngine> selected;
		int16 map = -1;
		CHECK(population_engine_select_vendor_spawn(profiles, excluded, selected, map));
		CHECK(map == 1);
		for (auto* old : g_population_engine_pcs) CHECK(old->pop.profile_config != selected);
		sessions.push_back(std::make_unique<Session>(Session{map, {selected}}));
		g_population_engine_pcs.push_back(sessions.back().get());
	}
	std::shared_ptr<PopulationEngine> selected;
	int16 map;
	CHECK(!population_engine_select_vendor_spawn(profiles, excluded, selected, map));
	// A blacksmith-only vendor cannot borrow Prontera's placement.
	auto smith = std::make_shared<PopulationEngine>();
	smith->source_profile_name = smith->vendor_key = "crafting";
	population_store_shell_profile(profiles, 5, smith);
	vendors.entries["crafting"]["prt_in"] = {"prt_in", 1};
	CHECK(population_engine_select_vendor_spawn(profiles, excluded, selected, map));
	CHECK(selected == smith && map == 2);
	excluded.insert("crafting@prt_in");
	CHECK(!population_engine_select_vendor_spawn(profiles, excluded, selected, map));
	// A profile with no placement never borrows another vendor's map.
	auto orphan = std::make_shared<PopulationEngine>();
	orphan->source_profile_name = orphan->vendor_key = "unplaced";
	population_store_shell_profile(profiles, 5, orphan);
	CHECK(!population_engine_select_vendor_spawn(profiles, excluded, selected, map));
	return 0;
}
