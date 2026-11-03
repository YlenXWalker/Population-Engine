#include "test_common.hpp"
#include <cctype>

// @production

int main() {
	const std::string source = "if (Class == JOB_SNIPER) setarrow;\n"
		"if (Class == JOB_CREATOR) { setcart; bonus bStr,2; }\n"
		"bonus bDex,3;";
	const auto result = make_bonus_script_str(source);
	CHECK(result.find("setarrow") == std::string::npos);
	CHECK(result.find("setcart") == std::string::npos);
	CHECK(result.find("bonus bStr,2;") != std::string::npos);
	CHECK(result.find("bonus bDex,3;") != std::string::npos);
	CHECK(result.find("if (Class == JOB_SNIPER) ;") != std::string::npos);
	CHECK(make_bonus_script_str("setarrow; setcart; setriding;").find("setarrow") == std::string::npos);
	CHECK(make_bonus_script_str("bonus bStr,1; bonus2 bAddRace,RC_All,5;") ==
		"bonus bStr,1; bonus2 bAddRace,RC_All,5;");
	CHECK(make_bonus_script_str("mes \"setarrow; setcart\";") == "mes \"setarrow; setcart\";");
	CHECK(make_bonus_script_str("setarrow2; bonus bStr,1;").find("setarrow2;") != std::string::npos);
	CHECK(make_bonus_script_str("/* setarrow; */ bonus bDex,1;").find("bonus bDex,1;") != std::string::npos);
	CHECK(make_bonus_script_str("// setcart;\nbonus bDex,1;").find("bonus bDex,1;") != std::string::npos);
	CHECK(make_bonus_script_str(".@setarrow = 1; bonus bDex,.@setarrow;") ==
		".@setarrow = 1; bonus bDex,.@setarrow;");
	return 0;
}