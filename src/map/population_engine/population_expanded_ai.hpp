// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder
//
// Population engine — Expanded AI Conditions framework.
// Lets `Condition:` in population_skill_db.yml be a boolean tree of predicates
// joined by AND/OR/NAND/NOR/XOR/NXOR gates instead of a single flat token.
//
// Inspired by Singe-Horizontal's "Expanded AI Conditions" patch (2022) but
// scoped to the population engine and the map_session_data data model.
//
// Lifecycle: parsed once during population_skill_db YAML load, stored as
// std::shared_ptr<ExpandedCondition> on s_pop_skill_entry / runtime skill
// structs. Evaluated every skill-pick tick via operator()(sd, target).

#ifndef POPULATION_EXPANDED_AI_HPP
#define POPULATION_EXPANDED_AI_HPP

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <common/cbasetypes.hpp>

#include "map/map.hpp"        // block_list, sc_type
#include "map/pc.hpp"         // map_session_data
#include "map/status.hpp"     // status_get_sc, sc_type, status_get_hp
#include "map/battle.hpp"     // battle_check_target
#include <common/database.hpp>

class map_session_data;
struct block_list;

namespace expanded_ai {

/// Which side of the engagement a predicate is asking about.
/// Not every predicate honours every target — distance/HP only make sense
/// against an enemy or ally; status checks work on all four.
enum class ExpTarget : uint8_t {
	Self    = 0, ///< The shell itself (always available).
	Enemy   = 1, ///< Current combat target (may be nullptr → predicate fails).
	Ally    = 2, ///< Resolved lazily by the predicate (foreachinrange).
	Master  = 3, ///< Reserved (homunc/merc owner). Not used by population shells.
	_Count  = 4,
};

/// Bag of pre-resolved block_list pointers handed to every predicate.
/// Self+Enemy are populated by the caller; Ally/Master stay nullptr and the
/// predicate that needs them does the lookup itself (so we don't pay the
/// foreachinrange cost when no predicate asks for an ally).
struct TargetBag {
	map_session_data*                                              shell  = nullptr;
	block_list*                                                    enemy  = nullptr;
	std::array<block_list*, static_cast<size_t>(ExpTarget::_Count)> bls   {};
};

/// Abstract base for one node in an expanded-condition tree.
/// Subclasses: SingleCondition (leaf predicate), ConditionContainer (gate).
class ExpandedCondition {
public:
	virtual ~ExpandedCondition() = default;
	/// Evaluate this node against the given context.
	virtual bool operator()(const TargetBag& bag) const = 0;
};

} // namespace expanded_ai

// Logic gates

// Population engine — Logic gates for ExpandedCondition trees.
//
// Each gate owns a vector of children and combines their boolean results.
// AND/OR/NAND/NOR short-circuit; XOR/NXOR don't (they need full parity).

namespace expanded_ai {

/// A gate node: holds children + the function that combines them.
class ConditionContainer : public ExpandedCondition {
public:
	enum class Gate : uint8_t { And, Or, Nand, Nor, Xor, Nxor };

	ConditionContainer(Gate g) : gate_(g) {}

	void add(std::shared_ptr<ExpandedCondition> child) {
		if (child) children_.push_back(std::move(child));
	}

	bool operator()(const TargetBag& bag) const override {
		switch (gate_) {
		case Gate::And:
			for (const auto& c : children_) if (!(*c)(bag)) return false;
			return true;
		case Gate::Or:
			for (const auto& c : children_) if ((*c)(bag)) return true;
			return false;
		case Gate::Nand:
			for (const auto& c : children_) if (!(*c)(bag)) return true;
			return false;
		case Gate::Nor:
			for (const auto& c : children_) if ((*c)(bag)) return false;
			return true;
		case Gate::Xor: {
			int true_count = 0;
			for (const auto& c : children_) if ((*c)(bag)) ++true_count;
			return true_count == 1;
		}
		case Gate::Nxor: {
			int true_count = 0;
			for (const auto& c : children_) if ((*c)(bag)) ++true_count;
			const int n = static_cast<int>(children_.size());
			return true_count == 0 || true_count == n;
		}
		}
		return false;
	}

	bool empty() const { return children_.empty(); }

	/// Resolve a YAML gate-key string (case-insensitive) to the enum.
	/// Returns false if the name is not a recognised gate.
	static bool name2gate(std::string name, Gate& out) {
		for (auto& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		if (name == "and")  { out = Gate::And;  return true; }
		if (name == "or")   { out = Gate::Or;   return true; }
		if (name == "nand") { out = Gate::Nand; return true; }
		if (name == "nor")  { out = Gate::Nor;  return true; }
		if (name == "xor")  { out = Gate::Xor;  return true; }
		if (name == "nxor") { out = Gate::Nxor; return true; }
		return false;
	}

private:
	Gate gate_;
	std::vector<std::shared_ptr<ExpandedCondition>> children_;
};

} // namespace expanded_ai

// Leaf predicates

// Population engine — Leaf predicates for ExpandedCondition trees.
//
// Each predicate evaluates one fact about a target slot in the TargetBag.
// All predicates respect an `inverted` flag so the parser can collapse the
// `not_` prefix into the leaf without an extra wrapper class.
//
// Predicates:
//   - LegacyPredicate     — defers to population_shell_skill_condition_ok so every
//                           existing PopSkillCondition token (self_targeted,
//                           enemy_hidden, hp_below, …) is usable inside trees.
//   - StatusPredicate     — `<target>_<scname>` (boolean status check).
//   - NumericPredicate    — `<target>_(hp|sp|hp_pct|sp_pct|distance|range)_<cmp><n>`.

// Forward decl in GLOBAL namespace — defined in population_engine_combat.cpp.
// Must NOT be inside namespace expanded_ai or the mangled name won't match.
bool population_shell_skill_condition_ok(
	map_session_data* sd,
	uint8_t condition, uint8_t cond_value_num, int16_t cond_sc_resolved,
	block_list* target_bl);

namespace expanded_ai {

enum class Cmp : uint8_t { Lt, Le, Gt, Ge, Eq };

inline bool cmp_eval(Cmp c, int lhs, int rhs) {
	switch (c) {
	case Cmp::Lt: return lhs <  rhs;
	case Cmp::Le: return lhs <= rhs;
	case Cmp::Gt: return lhs >  rhs;
	case Cmp::Ge: return lhs >= rhs;
	case Cmp::Eq: return lhs == rhs;
	}
	return false;
}

inline bool cmp_from_str(const std::string& s, Cmp& out) {
	if (s == "lt") { out = Cmp::Lt; return true; }
	if (s == "le") { out = Cmp::Le; return true; }
	if (s == "gt") { out = Cmp::Gt; return true; }
	if (s == "ge") { out = Cmp::Ge; return true; }
	if (s == "eq") { out = Cmp::Eq; return true; }
	return false;
}

/// Resolve a TargetBag slot to a block_list. Returns nullptr if the slot is
/// empty (e.g. Enemy when the shell has no target).
inline block_list* resolve_bl(const TargetBag& bag, ExpTarget t) {
	switch (t) {
	case ExpTarget::Self:   return bag.shell ? static_cast<block_list*>(bag.shell) : nullptr;
	case ExpTarget::Enemy:  return bag.enemy;
	case ExpTarget::Ally:   return bag.bls[static_cast<size_t>(ExpTarget::Ally)];
	case ExpTarget::Master: return bag.bls[static_cast<size_t>(ExpTarget::Master)];
	default: return nullptr;
	}
}

// ----------------------------------------------------------------------------
// Adapter for legacy PopSkillCondition tokens (so the entire flat enum is
// available inside expanded trees with zero re-implementation).
// ----------------------------------------------------------------------------
// (Forward declaration of population_shell_skill_condition_ok lives at the
// top of this file, in the global namespace, so its mangled name resolves
// against the definition in population_engine_combat.cpp.)

class LegacyPredicate : public ExpandedCondition {
public:
	LegacyPredicate(uint8_t cond, uint8_t v, int16_t sc, bool inverted)
		: cond_(cond), v_(v), sc_(sc), inv_(inverted) {}

	bool operator()(const TargetBag& bag) const override {
		if (!bag.shell) return false;
		const bool r = ::population_shell_skill_condition_ok(bag.shell, cond_, v_, sc_, bag.enemy);
		return inv_ ? !r : r;
	}

private:
	uint8_t  cond_;
	uint8_t  v_;
	int16_t  sc_;
	bool     inv_;
};

// ----------------------------------------------------------------------------
// Boolean status check: `<target>_<scname>` (e.g. self_hiding, enemy_stun).
// ----------------------------------------------------------------------------
class StatusPredicate : public ExpandedCondition {
public:
	StatusPredicate(ExpTarget t, sc_type sc, bool inverted)
		: t_(t), sc_(sc), inv_(inverted) {}

	bool operator()(const TargetBag& bag) const override {
		block_list* bl = resolve_bl(bag, t_);
		if (!bl) return inv_;  // missing target counts as "no, doesn't have it"
		const status_change* sc = status_get_sc(bl);
		const bool has = (sc != nullptr && sc->getSCE(sc_) != nullptr);
		return inv_ ? !has : has;
	}

private:
	ExpTarget t_;
	sc_type   sc_;
	bool      inv_;
};

// ----------------------------------------------------------------------------
// Numeric predicate: HP%, SP%, raw HP/SP, or distance, with comparator+value.
// ----------------------------------------------------------------------------
enum class NumKind : uint8_t { HpPct, SpPct, Hp, Sp, Distance };

class NumericPredicate : public ExpandedCondition {
public:
	NumericPredicate(ExpTarget t, NumKind k, Cmp cmp, int value, bool inverted)
		: t_(t), kind_(k), cmp_(cmp), val_(value), inv_(inverted) {}

	bool operator()(const TargetBag& bag) const override {
		block_list* bl = resolve_bl(bag, t_);
		if (!bl) return inv_;
		int lhs = 0;
		switch (kind_) {
		case NumKind::HpPct: {
			const int max = status_get_max_hp(bl);
			if (max <= 0) return inv_;
			lhs = static_cast<int>(static_cast<int64>(status_get_hp(bl)) * 100 / max);
			break;
		}
		case NumKind::SpPct: {
			const int max = status_get_max_sp(bl);
			if (max <= 0) return inv_;
			lhs = static_cast<int>(static_cast<int64>(status_get_sp(bl)) * 100 / max);
			break;
		}
		case NumKind::Hp:       lhs = status_get_hp(bl); break;
		case NumKind::Sp:       lhs = status_get_sp(bl); break;
		case NumKind::Distance: {
			block_list* sbl = bag.shell ? static_cast<block_list*>(bag.shell) : nullptr;
			if (!sbl || bl->m != sbl->m) return inv_;
			lhs = distance_bl(sbl, bl);
			break;
		}
		}
		const bool r = cmp_eval(cmp_, lhs, val_);
		return inv_ ? !r : r;
	}

	static bool kind_from_str(const std::string& s, NumKind& out) {
		if (s == "hp_pct")   { out = NumKind::HpPct;    return true; }
		if (s == "sp_pct")   { out = NumKind::SpPct;    return true; }
		if (s == "hp")       { out = NumKind::Hp;       return true; }
		if (s == "sp")       { out = NumKind::Sp;       return true; }
		if (s == "distance") { out = NumKind::Distance; return true; }
		return false;
	}

private:
	ExpTarget t_;
	NumKind   kind_;
	Cmp       cmp_;
	int       val_;
	bool      inv_;
};

} // namespace expanded_ai

// Parser API

// Population engine — public entry point for the expanded-condition parser.

// Forward-declare ryml::NodeRef indirectly via the YAML database header.
namespace expanded_ai {

/// Build an ExpandedCondition tree from a YAML node. Accepts scalar / sequence /
/// map shapes (see parser .cpp for grammar). Returns a non-null tree even on
/// errors (a constant-false sentinel) so callers can rely on the pointer
/// without null-checking each time.
std::shared_ptr<ExpandedCondition> parse_expanded_condition(const ryml::NodeRef& node);

} // namespace expanded_ai

#endif /* POPULATION_EXPANDED_AI_HPP */
