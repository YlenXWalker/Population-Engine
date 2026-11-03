#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

using int16 = int16_t;
using int32 = int32_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using t_tick = int64_t;

#define CHECK(condition) do { if (!(condition)) { \
	std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); return 1; \
} } while (false)

template<class... Args> void ShowWarning(const char*, Args...) {}
template<class... Args> void ShowError(const char*, Args...) {}
