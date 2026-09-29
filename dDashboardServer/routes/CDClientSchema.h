#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "json.hpp"
#include "GeneralUtils.h"
#include "magic_enum.hpp"
#include "eReplicaComponentType.h"

/**
 * The CDClient browser's view of the CDClient database: the tables and columns (read from the database's own schema
 * at startup, never listed here), which columns point at rows of other tables, and safe row queries built from them.
 *
 * Queries only ever name tables and columns that are in the schema, quoted, and every value is a bound parameter, so
 * nothing a browser sends becomes SQL. Pure, so it is unit tested.
 */
namespace CDClientSchema {
	enum class eValueType : uint8_t { INTEGER, REAL, TEXT };

	struct Column {
		std::string name;
		std::string type; // as declared: int32, int64, int_bool, real, text_4, ...
		eValueType valueType{ eValueType::TEXT };
	};

	struct Table {
		std::string name;
		std::vector<Column> columns;

		const Column* Find(std::string_view column) const {
			const auto it = std::ranges::find_if(columns, [&](const Column& c) { return c.name == column; });
			return it == columns.end() ? nullptr : &*it;
		}
	};

	// How a declared column type is compared and bound
	inline eValueType ValueType(std::string type) {
		std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c) { return std::tolower(c); });
		if (type.find("int") != std::string::npos) return eValueType::INTEGER;
		if (type.find("real") != std::string::npos || type.find("float") != std::string::npos || type.find("double") != std::string::npos) return eValueType::REAL;
		return eValueType::TEXT;
	}

	inline std::string Lower(std::string_view text) {
		std::string lower(text);
		std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
		return lower;
	}

	class Schema {
	public:
		void Add(Table table) {
			for (auto& column : table.columns) column.valueType = ValueType(column.type);
			m_ByLowerName[Lower(table.name)] = table.name;
			m_Tables[table.name] = std::move(table);
		}

		const Table* Find(std::string_view name) const {
			const auto it = m_Tables.find(std::string(name));
			return it == m_Tables.end() ? nullptr : &it->second;
		}

		// A table by name ignoring case (the CDClient's names mix cases)
		const Table* FindIgnoringCase(std::string_view name) const {
			const auto it = m_ByLowerName.find(Lower(name));
			return it == m_ByLowerName.end() ? nullptr : Find(it->second);
		}

		const std::map<std::string, Table>& Tables() const { return m_Tables; }

	private:
		std::map<std::string, Table> m_Tables;
		std::map<std::string, std::string> m_ByLowerName;
	};

	// ---- Links between tables ----

	// What a column's values point at. The browser links such values to the rows they point at.
	enum class eLink : uint8_t { OBJECT, LOOT_MATRIX, LOOT_TABLE, RARITY_TABLE, BEHAVIOR, SKILL, MISSION, ACTIVITY, ICON, ZONE, EMOTE };

	struct LinkTarget {
		std::string_view table;
		std::string_view column;
	};

	// The table and column each kind of link points at
	inline LinkTarget Target(eLink link) {
		switch (link) {
		case eLink::OBJECT: return { "Objects", "id" };
		case eLink::LOOT_MATRIX: return { "LootMatrix", "LootMatrixIndex" };
		case eLink::LOOT_TABLE: return { "LootTable", "LootTableIndex" };
		case eLink::RARITY_TABLE: return { "RarityTable", "RarityTableIndex" };
		case eLink::BEHAVIOR: return { "BehaviorTemplate", "behaviorID" };
		case eLink::SKILL: return { "SkillBehavior", "skillID" };
		case eLink::MISSION: return { "Missions", "id" };
		case eLink::ACTIVITY: return { "Activities", "ActivityID" };
		case eLink::ICON: return { "Icons", "IconID" };
		case eLink::ZONE: return { "ZoneTable", "zoneID" };
		case eLink::EMOTE: return { "Emotes", "id" };
		}
		return {};
	}

	/**
	 * The CDClient declares no foreign keys, so which columns point where is a matter of naming. Each rule names a
	 * column (lowercase) and optionally the only table it applies to; the first match wins, so table-specific rules
	 * come first. A rule only takes effect for integer columns the schema actually has, pointing at a table it has.
	 */
	struct LinkRule {
		eLink link;
		std::string_view table; // empty: any table
		std::string_view column;
	};

	inline const std::vector<LinkRule>& LinkRules() {
		static const std::vector<LinkRule> rules{
			// ActivityRewards.objectTemplate is the activity (Loot looks rewards up by activity ID)
			{ eLink::ACTIVITY, "ActivityRewards", "objecttemplate" },
			{ eLink::MISSION, "MissionTasks", "id" },
			{ eLink::MISSION, "MissionText", "id" },
			{ eLink::OBJECT, "ComponentsRegistry", "id" },
			// Only these tables' behaviorID is a skill behavior; DevModelBehaviors and ObjectBehaviors hold model behaviors
			{ eLink::BEHAVIOR, "SkillBehavior", "behaviorid" },
			{ eLink::BEHAVIOR, "BehaviorParameter", "behaviorid" },
			{ eLink::BEHAVIOR, "BehaviorTemplate", "behaviorid" },
			{ eLink::MISSION, "CollectibleComponent", "requirement_mission" },

			{ eLink::OBJECT, "", "lot" }, { eLink::OBJECT, "", "itemid" }, { eLink::OBJECT, "", "objecttemplate" },
			{ eLink::OBJECT, "", "ndobjectid" }, { eLink::OBJECT, "", "offer_objectid" }, { eLink::OBJECT, "", "target_objectid" },
			{ eLink::OBJECT, "", "reward_item1" }, { eLink::OBJECT, "", "reward_item2" }, { eLink::OBJECT, "", "reward_item3" },
			{ eLink::OBJECT, "", "reward_item4" }, { eLink::OBJECT, "", "reward_item1_repeatable" }, { eLink::OBJECT, "", "reward_item2_repeatable" },
			{ eLink::OBJECT, "", "reward_item3_repeatable" }, { eLink::OBJECT, "", "reward_item4_repeatable" },
			{ eLink::OBJECT, "", "attachmentlot" }, { eLink::OBJECT, "", "npclot" }, { eLink::OBJECT, "", "puzzlemodellot" },
			{ eLink::OBJECT, "", "commendationlot" }, { eLink::OBJECT, "", "currencylot" }, { eLink::OBJECT, "", "createdlot" },
			{ eLink::OBJECT, "", "optionalcostlot" }, { eLink::OBJECT, "", "zonecontroltemplate" },
			{ eLink::LOOT_MATRIX, "", "lootmatrixindex" }, { eLink::LOOT_MATRIX, "", "lootmatrixid" },
			{ eLink::LOOT_TABLE, "", "loottableindex" },
			{ eLink::RARITY_TABLE, "", "raritytableindex" },
			{ eLink::SKILL, "", "skillid" },
			{ eLink::MISSION, "", "missionid" },
			{ eLink::ACTIVITY, "", "activityid" },
			{ eLink::ICON, "", "iconid" },
			// Zones: where an activity is played, where a launchpad or property goes
			{ eLink::ZONE, "", "zoneid" }, { eLink::ZONE, "", "instancemapid" }, { eLink::ZONE, "", "mapid" }, { eLink::ZONE, "", "vendormapid" },
			{ eLink::ZONE, "", "targetzone" }, { eLink::ZONE, "", "defaultzoneid" },
			// Emotes a mission unlocks (Character::UnlockEmote)
			{ eLink::EMOTE, "", "reward_emote" }, { eLink::EMOTE, "", "reward_emote2" }, { eLink::EMOTE, "", "reward_emote3" },
			{ eLink::EMOTE, "", "reward_emote4" }, { eLink::EMOTE, "", "emoteid" },
		};
		return rules;
	}

	// What a column of a table points at, if anything
	inline std::optional<eLink> LinkFor(const Schema& schema, const Table& table, const Column& column) {
		if (column.valueType != eValueType::INTEGER || column.type.find("bool") != std::string::npos) return std::nullopt;
		const auto name = Lower(column.name);
		for (const auto& rule : LinkRules()) {
			if (!rule.table.empty() && rule.table != table.name) continue;
			if (rule.column != name) continue;
			const auto target = Target(rule.link);
			const auto* targetTable = schema.Find(target.table);
			if (!targetTable || !targetTable->Find(target.column)) return std::nullopt;
			// A table's own key isn't a link to itself
			if (target.table == table.name && target.column == column.name) return std::nullopt;
			return rule.link;
		}
		return std::nullopt;
	}

	// The link's name as the browser sends it ("loot_matrix")
	inline std::string LinkName(eLink link) {
		return Lower(magic_enum::enum_name(link));
	}

	/**
	 * The table holding a component type's rows (ComponentsRegistry.component_id is its id). The CDClient doesn't say,
	 * so the type's name is matched to a table called <Name>Component (VENDOR -> VendorComponent, BASE_COMBAT_AI ->
	 * BaseCombatAIComponent). The types below use differently named tables, as the server's components read them.
	 */
	inline const Table* ComponentTable(const Schema& schema, eReplicaComponentType type) {
		using enum eReplicaComponentType;
		static const std::map<eReplicaComponentType, std::string_view> DIFFERENT{
			{ DESTROYABLE, "DestructibleComponent" },
			{ CONTROLLABLE_PHYSICS, "PhysicsComponent" }, { SIMPLE_PHYSICS, "PhysicsComponent" }, { PHANTOM_PHYSICS, "PhysicsComponent" },
			{ RIGID_BODY_PHANTOM_PHYSICS, "PhysicsComponent" }, { HAVOK_VEHICLE_PHYSICS, "VehiclePhysics" },
			{ QUICK_BUILD, "RebuildComponent" }, { MISSION_OFFER, "MissionNPCComponent" }, { ROCKET_LAUNCH, "RocketLaunchpadControlComponent" },
		};
		if (const auto it = DIFFERENT.find(type); it != DIFFERENT.end()) return schema.Find(it->second);
		std::string name;
		for (const char c : magic_enum::enum_name(type)) if (c != '_') name += c;
		if (name.empty()) return nullptr;
		return schema.FindIgnoringCase(name + "Component");
	}

	// ---- Row queries ----

	using Value = std::variant<int64_t, double, std::string>;

	struct Filter {
		std::string column;
		std::string op;    // = != < <= > >= contains starts null notnull
		std::string value;
	};

	struct RowQuery {
		std::string table;
		std::vector<Filter> filters;
		std::string search;       // any text column containing it, or any integer column equal to it
		std::string orderColumn;  // empty: table order
		bool ascending{ true };
		uint32_t start{};
		uint32_t length{ 50 };
	};

	constexpr uint32_t MAX_ROWS = 500;
	constexpr size_t MAX_FILTERS = 16;

	struct BuiltQuery {
		std::string select; // the page of rows
		std::string count;  // how many rows match
		std::vector<Value> params;       // for `select`: the WHERE parameters, then limit and offset
		std::vector<Value> countParams;  // for `count`
	};

	// "name" with any quote doubled
	inline std::string Quote(std::string_view identifier) {
		std::string quoted = "\"";
		for (const char c : identifier) {
			if (c == '"') quoted += '"';
			quoted += c;
		}
		return quoted + "\"";
	}

	// Text for LIKE with its wildcards escaped (ESCAPE '\')
	inline std::string LikeEscape(std::string_view text) {
		return GeneralUtils::LikeEscape(text, '\\');
	}

	// A filter value as the column's type; nullopt when it isn't one (e.g. "abc" for an integer column)
	inline std::optional<Value> Typed(const Column& column, const std::string& text) {
		if (column.valueType == eValueType::TEXT) return Value{ text };
		try {
			size_t used = 0;
			if (column.valueType == eValueType::INTEGER) {
				const auto value = std::stoll(text, &used);
				if (used == text.size()) return Value{ static_cast<int64_t>(value) };
			} else {
				const auto value = std::stod(text, &used);
				if (used == text.size()) return Value{ value };
			}
		} catch (const std::exception&) {}
		return std::nullopt;
	}

	/**
	 * The SQL for a page of a table's rows. Sets `error` and returns nullopt for an unknown table or column, an
	 * unknown operator or a value that doesn't fit its column.
	 */
	inline std::optional<BuiltQuery> BuildRowQuery(const Schema& schema, const RowQuery& query, std::string& error) {
		const auto* table = schema.Find(query.table);
		if (!table) {
			error = "Unknown table";
			return std::nullopt;
		}
		if (query.filters.size() > MAX_FILTERS) {
			error = "At most " + std::to_string(MAX_FILTERS) + " filters";
			return std::nullopt;
		}

		std::vector<std::string> conditions;
		std::vector<Value> params;
		for (const auto& filter : query.filters) {
			const auto* column = table->Find(filter.column);
			if (!column) {
				error = "Unknown column " + filter.column;
				return std::nullopt;
			}
			const auto quoted = Quote(column->name);
			if (filter.op == "null" || filter.op == "notnull") {
				conditions.push_back(quoted + (filter.op == "null" ? " IS NULL" : " IS NOT NULL"));
				continue;
			}
			if (filter.op == "contains" || filter.op == "starts") {
				conditions.push_back("CAST(" + quoted + " AS TEXT) LIKE ? ESCAPE '\\'");
				params.emplace_back((filter.op == "contains" ? "%" : "") + LikeEscape(filter.value) + "%");
				continue;
			}
			static const std::vector<std::string> COMPARISONS{ "=", "!=", "<", "<=", ">", ">=" };
			if (std::ranges::find(COMPARISONS, filter.op) == COMPARISONS.end()) {
				error = "Unknown operator " + filter.op;
				return std::nullopt;
			}
			const auto value = Typed(*column, filter.value);
			if (!value) {
				error = column->name + " holds numbers";
				return std::nullopt;
			}
			conditions.push_back(quoted + " " + filter.op + " ?");
			params.push_back(*value);
		}

		if (!query.search.empty()) {
			std::vector<std::string> any;
			for (const auto& column : table->columns) {
				if (column.valueType == eValueType::TEXT) {
					any.push_back(Quote(column.name) + " LIKE ? ESCAPE '\\'");
					params.emplace_back("%" + LikeEscape(query.search) + "%");
				} else if (const auto number = Typed(column, query.search)) {
					any.push_back(Quote(column.name) + " = ?");
					params.push_back(*number);
				}
			}
			conditions.push_back(any.empty() ? "0" : "(" + [&] {
				std::string joined;
				for (size_t i = 0; i < any.size(); i++) joined += (i ? " OR " : "") + any[i];
				return joined;
			}() + ")");
		}

		std::string where;
		for (size_t i = 0; i < conditions.size(); i++) where += (i ? " AND " : " WHERE ") + conditions[i];

		std::string order;
		if (!query.orderColumn.empty()) {
			const auto* column = table->Find(query.orderColumn);
			if (!column) {
				error = "Unknown column " + query.orderColumn;
				return std::nullopt;
			}
			order = " ORDER BY " + Quote(column->name) + (query.ascending ? " ASC" : " DESC");
		}

		BuiltQuery built;
		built.count = "SELECT COUNT(*) FROM " + Quote(table->name) + where + ";";
		built.countParams = params;
		built.select = "SELECT * FROM " + Quote(table->name) + where + order + " LIMIT ? OFFSET ?;";
		built.params = std::move(params);
		built.params.emplace_back(static_cast<int64_t>(std::clamp<uint32_t>(query.length, 1, MAX_ROWS)));
		built.params.emplace_back(static_cast<int64_t>(query.start));
		return built;
	}

	// ---- Behaviors ----

	/**
	 * Whether a behavior parameter holds a child behavior's ID. The server's behaviors read their children by these
	 * names (Behavior::GetAction): "action", "miss action", "on_success", "behavior 1", ... but not the "*faction*"
	 * parameters, which are faction IDs.
	 */
	inline bool IsChildBehaviorParameter(std::string_view parameter) {
		const auto name = Lower(parameter);
		if (name.find("faction") != std::string::npos) return false;
		return name.starts_with("behavior") || name.starts_with("on_") || name.find("action") != std::string::npos;
	}
}
