#include "test_common.hpp"
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
using int32 = int32_t;
constexpr int NAME_LENGTH = 24;
struct Member { uint32_t account_id = 0, char_id = 0; char name[24]{}, map[16]{}; int class_ = 0, lv = 0, online = 0; };
struct party { int party_id = 1, count = 0; Member member[3]; };
struct map_session_data {
	int mapindex = 1;
	struct { int party_id = 1; uint32_t account_id = 5000001, char_id = 5000002; char name[24] = "Companion"; int class_ = 5, base_level = 99; } status;
	struct { uint32_t companion_owner = 42; } pop;
};
std::unordered_set<int32> g_population_companions;
std::unordered_map<int32, map_session_data*> sessions;
map_session_data* map_id2sd(int32 id) { auto it = sessions.find(id); return it == sessions.end() ? nullptr : it->second; }
const char* mapindex_id2name(int) { return "prontera"; }
void safestrncpy(char* dest, const char* source, size_t length) { std::strncpy(dest, source, length); dest[length - 1] = 0; }

// @production

int main() {
	map_session_data bot;
	sessions[bot.status.account_id] = &bot;
	g_population_companions.insert(bot.status.account_id);
	party incoming;
	incoming.member[0].account_id = 7;
	incoming.member[0].char_id = 42;
	incoming.count = 1;
	population_engine_restore_party(&incoming);
	CHECK(incoming.count == 2 && incoming.member[1].char_id == bot.status.char_id);
	CHECK(incoming.member[1].class_ == 5 && incoming.member[1].online == 1);
	population_engine_restore_party(&incoming);
	CHECK(incoming.count == 2); // A duplicate refresh is idempotent.
	incoming = party{}; incoming.count = 1; incoming.member[0].char_id = 99;
	population_engine_restore_party(&incoming);
	CHECK(incoming.count == 1); // Owner left: never append the fake member.
	incoming = party{}; incoming.count = 3;
	for (int i = 0; i < 3; ++i) { incoming.member[i].account_id = i + 1; incoming.member[i].char_id = i + 42; }
	population_engine_restore_party(&incoming);
	CHECK(incoming.count == 3 && incoming.member[2].char_id == 44); // Humans fill the party.
	incoming = party{}; incoming.count = 1; incoming.member[0].char_id = 42;
	bot.status.party_id = 2;
	population_engine_restore_party(&incoming);
	CHECK(incoming.count == 1); // Another party cannot acquire this companion.
	return 0;
}
