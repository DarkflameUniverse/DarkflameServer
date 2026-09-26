#include "CDClientBrowser.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

#include "BehaviorTemplate.h"
#include "CDClientDatabase.h"
#include "CDClientRules.h"
#include "CDClientSchema.h"
#include "ClientAssets.h"
#include "Database.h"
#include "GameLabels.h"
#include "GeneralUtils.h"
#include "InstanceLimits.h"
#include "Locale.h"
#include "RouteUtils.h"
#include "eHTTPMethod.h"
#include "eItemType.h"
#include "eMissionState.h"
#include "eMissionTaskType.h"
#include "eRacingTaskParam.h"

using namespace RouteUtils;
using namespace CDClientSchema;
using json = nlohmann::json;

namespace {
	constexpr uint32_t DEFAULT_PAGE = 50;
	constexpr size_t REFERENCE_ROWS = 25;   // rows shown per table in "used by" lists
	constexpr size_t MAX_OBJECTS = 100;     // objects listed in "dropped by" and similar lists
	constexpr size_t SEARCH_RESULTS = 25;
	constexpr size_t MAX_BEHAVIOR_NODES = 500;
	constexpr size_t MAX_BEHAVIOR_DEPTH = 32;
	constexpr size_t MAX_LOOT_ITEMS = 1000;  // items listed per vendor loot table (the biggest loot tables have ~650)
	constexpr size_t MAX_MATRICES = 100;    // loot matrices whose odds are worked out for "comes from" lists

	// Read from the database itself once: every table and its columns
	const Schema& GetSchema() {
		static const Schema schema = [] {
			Schema loaded;
			std::vector<std::string> names;
			auto tables = CDClientDatabase::ExecuteQuery("SELECT name FROM sqlite_master WHERE type = 'table' ORDER BY name;");
			while (!tables.eof()) {
				names.emplace_back(tables.getStringField(0, ""));
				tables.nextRow();
			}
			for (const auto& name : names) {
				Table table{ name, {} };
				auto columns = CDClientDatabase::ExecuteQuery("PRAGMA table_info(" + Quote(name) + ");");
				while (!columns.eof()) {
					table.columns.push_back({ columns.getStringField("name", ""), columns.getStringField("type", "") });
					columns.nextRow();
				}
				loaded.Add(std::move(table));
			}
			return loaded;
		}();
		return schema;
	}

	// A row's value, or the fallback when it is missing or NULL (the CDClient has many NULLs)
	int64_t Int(const json& row, const std::string& key, int64_t fallback = 0) {
		const auto it = row.find(key);
		return it != row.end() && it->is_number() ? it->get<int64_t>() : fallback;
	}

	double Real(const json& row, const std::string& key) {
		const auto it = row.find(key);
		return it != row.end() && it->is_number() ? it->get<double>() : 0.0;
	}

	std::string Text(const json& row, const std::string& key, const std::string& fallback = "") {
		const auto it = row.find(key);
		return it != row.end() && it->is_string() ? it->get<std::string>() : fallback;
	}

	void Bind(CppSQLite3Statement& statement, const std::vector<Value>& params) {
		for (size_t i = 0; i < params.size(); i++) {
			const int index = static_cast<int>(i + 1);
			std::visit([&](const auto& value) {
				using T = std::decay_t<decltype(value)>;
				if constexpr (std::is_same_v<T, std::string>) statement.bind(index, value.c_str());
				else if constexpr (std::is_same_v<T, int64_t>) statement.bind(index, static_cast<sqlite_int64>(value));
				else statement.bind(index, value);
			}, params[i]);
		}
	}

	json RowJson(CppSQLite3Query& query) {
		json row = json::object();
		for (int i = 0; i < query.numFields(); i++) {
			const std::string name = query.fieldName(i);
			switch (query.fieldDataType(i)) {
			case SQLITE_INTEGER: row[name] = query.getInt64Field(i); break;
			case SQLITE_FLOAT: row[name] = query.getFloatField(i); break;
			case SQLITE_NULL: row[name] = nullptr; break;
			default: row[name] = query.getStringField(i, ""); break;
			}
		}
		return row;
	}

	// Rows of a fixed query (only ever SQL written here, with values bound)
	json Rows(const std::string& sql, const std::vector<Value>& params, size_t max) {
		auto statement = CDClientDatabase::CreatePreppedStmt(sql);
		Bind(statement, params);
		auto query = statement.execQuery();
		json rows = json::array();
		while (!query.eof() && rows.size() < max) {
			rows.push_back(RowJson(query));
			query.nextRow();
		}
		return rows;
	}

	int64_t Count(const std::string& sql, const std::vector<Value>& params) {
		auto statement = CDClientDatabase::CreatePreppedStmt(sql);
		Bind(statement, params);
		auto query = statement.execQuery();
		return query.eof() ? 0 : query.getInt64Field(0);
	}

	// Rows of `table` whose `column` equals `value` (both from the schema)
	json RowsWhere(const Table& table, const Column& column, int64_t value, size_t max) {
		return Rows("SELECT * FROM " + Quote(table.name) + " WHERE " + Quote(column.name) + " = ? LIMIT ?;", { value, static_cast<int64_t>(max) }, max);
	}

	json RowsWhere(std::string_view tableName, std::string_view columnName, int64_t value, size_t max) {
		const auto* table = GetSchema().Find(tableName);
		const auto* column = table ? table->Find(columnName) : nullptr;
		return column ? RowsWhere(*table, *column, value, max) : json::array();
	}

	std::string Phrase(const std::string& key) {
		return Locale::GetPhrase(key);
	}

	// The locale's texts for a row: <Table>_<id>_<column> -> {column: text}
	json Localized(const std::string& table, int64_t id) {
		const auto prefix = table + "_" + std::to_string(id) + "_";
		json texts = json::object();
		for (const auto& key : Locale::GetPhraseIdsWithPrefix(prefix)) texts[key.substr(prefix.size())] = Locale::GetPhrase(key);
		return texts;
	}

	std::string Named(const std::string& phrase, const std::string& fallback) {
		return phrase.empty() ? fallback : phrase;
	}

	// A readable name for what a link points at
	std::string LinkLabel(eLink link, int64_t id) {
		const auto text = std::to_string(id);
		switch (link) {
		case eLink::OBJECT: return ClientAssets::ItemName(static_cast<LOT>(id));
		case eLink::MISSION: return Named(Phrase("Missions_" + text + "_name"), "Mission " + text);
		case eLink::SKILL: return Named(Phrase("SkillBehavior_" + text + "_name"), "Skill " + text);
		case eLink::ACTIVITY: return Named(Phrase("Activities_" + text + "_ActivityName"), "Activity " + text);
		case eLink::ZONE: return Named(Phrase("ZoneTable_" + text + "_DisplayDescription"), "Zone " + text);
		case eLink::EMOTE: {
			// Most emotes have no text in the locale, only an animation
			if (const auto& phrase = Phrase("Emotes_" + text + "_outputText"); !phrase.empty()) return phrase;
			const auto rows = RowsWhere("Emotes", "id", id, 1);
			return Named(rows.empty() ? "" : Text(rows[0], "animationName"), "Emote " + text);
		}
		default: return GameLabels::Words(magic_enum::enum_name(link)) + " " + text;
		}
	}

	// Whether a link's target row is in the CDClient
	bool Exists(eLink link, int64_t id) {
		const auto target = Target(link);
		return !RowsWhere(target.table, target.column, id, 1).empty();
	}

	json Ref(eLink link, int64_t id) {
		return { {"link", LinkName(link)}, {"id", id}, {"name", LinkLabel(link, id)} };
	}

	// Component table -> the component types stored in it
	const std::map<std::string, std::vector<int64_t>>& ComponentTypesByTable() {
		static const auto byTable = [] {
			std::map<std::string, std::vector<int64_t>> map;
			for (const auto type : magic_enum::enum_values<eReplicaComponentType>()) {
				if (const auto* table = ComponentTable(GetSchema(), type)) map[table->name].push_back(static_cast<int64_t>(type));
			}
			return map;
		}();
		return byTable;
	}

	std::string TypeList(const std::vector<int64_t>& types) {
		std::string list;
		for (const auto type : types) list += (list.empty() ? "" : ",") + std::to_string(type);
		return list;
	}

	/**
	 * Objects with a component row in `table` matching `condition` (SQL about c, the component row; values bound).
	 * Only for component tables; others give nothing.
	 */
	json ObjectsWithComponent(const std::string& table, const std::string& condition, const std::vector<Value>& params) {
		const auto types = ComponentTypesByTable().find(table);
		if (types == ComponentTypesByTable().end()) return json::array();
		json objects = json::array();
		// The type list is made of numbers from the enum, so it can be part of the SQL
		const auto rows = Rows("SELECT DISTINCT cr.id AS lot FROM ComponentsRegistry cr JOIN " + Quote(table) + " c ON c.id = cr.component_id "
			"WHERE cr.component_type IN (" + TypeList(types->second) + ") AND " + condition + " LIMIT " + std::to_string(MAX_OBJECTS) + ";", params, MAX_OBJECTS);
		for (const auto& row : rows) objects.push_back(Ref(eLink::OBJECT, Int(row, "lot")));
		return objects;
	}

	/**
	 * Everything pointing at `id` through a `link` column: per (table, column) how many rows, the first few, and for
	 * component tables the objects that have them.
	 */
	json References(eLink link, int64_t id) {
		json references = json::array();
		for (const auto& [table, column] : ColumnsLinkingTo(GetSchema(), link)) {
			const auto where = " WHERE " + Quote(column->name) + " = ?";
			const auto count = Count("SELECT COUNT(*) FROM " + Quote(table->name) + where + ";", { id });
			if (count == 0) continue;
			references.push_back({
				{"table", table->name}, {"column", column->name}, {"count", count},
				{"rows", RowsWhere(*table, *column, id, REFERENCE_ROWS)},
				{"objects", ObjectsWithComponent(table->name, "c." + Quote(column->name) + " = ?", { id })}
			});
		}
		return references;
	}

	json Columns(const Table& table) {
		json columns = json::array();
		for (const auto& column : table.columns) {
			const auto link = LinkFor(GetSchema(), table, column);
			columns.push_back({ {"name", column.name}, {"type", column.type}, {"link", link ? json(LinkName(*link)) : json(nullptr)} });
		}
		return columns;
	}

	std::string ComponentTypeName(int64_t type) {
		const auto name = magic_enum::enum_name(static_cast<eReplicaComponentType>(type));
		return name.empty() ? "Type " + std::to_string(type) : GameLabels::Words(name);
	}

	// ---- Models ----

	bool PlainName(std::string_view name) {
		return !name.empty() && name.size() < 128 && std::ranges::all_of(name, [](char c) {
			return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.';
		});
	}

	/**
	 * LXFML to preview an object, and where it came from: brick-built objects have one in res/BrickModels named after
	 * their render asset (in the render component's LXFMLFolder, if any); a single LEGO brick (BrickIDTable) is drawn
	 * as that one brick.
	 */
	std::optional<std::pair<std::string, std::string>> ObjectLxfml(LOT lot) {
		const auto render = Rows("SELECT rc.render_asset, rc.LXFMLFolder FROM ComponentsRegistry cr JOIN RenderComponent rc ON rc.id = cr.component_id "
			"WHERE cr.component_type = ? AND cr.id = ? LIMIT 1;", { static_cast<int64_t>(eReplicaComponentType::RENDER), static_cast<int64_t>(lot) }, 1);
		if (!render.empty()) {
			std::string asset = Text(render[0], "render_asset");
			std::replace(asset.begin(), asset.end(), '\\', '/');
			asset = asset.substr(asset.rfind('/') + 1);
			asset = Lower(asset.substr(0, asset.rfind('.')));
			const std::string folder = Lower(Text(render[0], "LXFMLFolder"));
			if (PlainName(asset)) {
				std::vector<std::string> candidates;
				if (PlainName(folder)) candidates.push_back("BrickModels/" + folder + "/" + asset + ".lxfml");
				candidates.push_back("BrickModels/" + asset + ".lxfml");
				for (const auto& path : candidates) {
					if (auto data = ClientAssets::ReadResFile(path)) return std::make_pair(std::move(*data), path);
				}
				if (const auto found = ClientAssets::FindResFile("BrickModels", asset + ".lxfml")) {
					if (auto data = ClientAssets::ReadResFile(*found)) return std::make_pair(std::move(*data), *found);
				}
			}
		}
		const auto brick = Rows("SELECT LEGOBrickID FROM BrickIDTable WHERE NDObjectID = ? LIMIT 1;", { static_cast<int64_t>(lot) }, 1);
		if (brick.empty()) return std::nullopt;
		const auto design = std::to_string(Int(brick[0], "LEGOBrickID"));
		return std::make_pair(
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<LXFML versionMajor=\"5\" versionMinor=\"0\"><Bricks><Brick refID=\"0\" designID=\"" + design +
			"\"><Part refID=\"0\" designID=\"" + design + "\" materials=\"0\"><Bone refID=\"0\" transformation=\"1,0,0,0,1,0,0,0,1,0,0,0\"/></Part></Brick></Bricks></LXFML>\n",
			"LEGO brick " + design);
	}

	// ---- Loot, as the server rolls it (CDClientRules) ----

	constexpr auto ITEM_COMPONENT = static_cast<int64_t>(eReplicaComponentType::ITEM);

	/**
	 * Items' rarities as RollLootMatrix looks them up: the item component's rarity; an object without one gets item
	 * component 0's (what GetByIDAndType's default finds), or 0. Read once: the CDClient has no indexes.
	 */
	struct Rarities {
		std::map<int64_t, int32_t> byLot;
		int32_t withoutComponent{};
	};

	const Rarities& ItemRarities() {
		static const Rarities rarities = [] {
			Rarities loaded;
			for (const auto& row : Rows("SELECT cr.id, ic.rarity FROM ComponentsRegistry cr JOIN ItemComponent ic ON ic.id = cr.component_id WHERE cr.component_type = ?;",
				{ ITEM_COMPONENT }, SIZE_MAX)) {
				loaded.byLot.emplace(Int(row, "id"), static_cast<int32_t>(Int(row, "rarity")));
			}
			const auto none = Rows("SELECT rarity FROM ItemComponent WHERE id = 0 LIMIT 1;", {}, 1);
			if (!none.empty()) loaded.withoutComponent = static_cast<int32_t>(Int(none[0], "rarity"));
			return loaded;
		}();
		return rarities;
	}

	int32_t RollRarity(int64_t lot) {
		const auto& rarities = ItemRarities();
		const auto it = rarities.byLot.find(lot);
		return it == rarities.byLot.end() ? rarities.withoutComponent : it->second;
	}

	// An object as a link, with its item rarity when it is an item
	json ItemRef(int64_t lot) {
		auto ref = Ref(eLink::OBJECT, lot);
		const auto& rarities = ItemRarities();
		if (const auto it = rarities.byLot.find(lot); it != rarities.byLot.end()) ref["rarity"] = it->second;
		return ref;
	}

	/**
	 * A loot table's rows by one of its columns, all read once in table order: odds for an item used by hundreds of
	 * matrices would otherwise scan the unindexed tables hundreds of times.
	 */
	using Grouped = std::map<int64_t, std::vector<json>>;

	Grouped GroupRows(const std::string& sql, const std::string& key) {
		Grouped grouped;
		for (auto& row : Rows(sql, {}, SIZE_MAX)) grouped[Int(row, key)].push_back(std::move(row));
		return grouped;
	}

	const std::vector<json>& Group(const Grouped& grouped, int64_t index) {
		static const std::vector<json> none;
		const auto it = grouped.find(index);
		return it == grouped.end() ? none : it->second;
	}

	const std::vector<json>& LootMatrixRows(int64_t index) {
		static const auto grouped = GroupRows("SELECT * FROM LootMatrix;", "LootMatrixIndex");
		return Group(grouped, index);
	}

	const std::vector<json>& LootTableRows(int64_t index) {
		static const auto grouped = GroupRows("SELECT * FROM LootTable;", "LootTableIndex");
		return Group(grouped, index);
	}

	// The rarity rows in the order the server walks them (CDRarityTableTable: randmax descending)
	std::vector<CDClientRules::RarityRow> RarityRows(int64_t index) {
		static const auto grouped = GroupRows("SELECT RarityTableIndex, randmax, rarity FROM RarityTable ORDER BY randmax DESC;", "RarityTableIndex");
		std::vector<CDClientRules::RarityRow> rows;
		for (const auto& row : Group(grouped, index)) rows.push_back({ Real(row, "randmax"), static_cast<int32_t>(Int(row, "rarity")) });
		return rows;
	}

	// One loot matrix entry as a drop rolls it: the loot table's items with their rarity and chances
	struct EntryOdds {
		std::vector<int32_t> rarities;
		std::vector<double> perDrop;
		std::vector<CDClientRules::DropOdds> odds;
	};

	EntryOdds RollEntry(const json& row) {
		EntryOdds entry;
		const auto& lootRows = LootTableRows(Int(row, "LootTableIndex"));
		for (const auto& loot : lootRows) entry.rarities.push_back(RollRarity(Int(loot, "itemid")));
		entry.perDrop = CDClientRules::ChancePerDrop(entry.rarities, RarityRows(Int(row, "RarityTableIndex")));
		for (const auto chance : entry.perDrop) {
			entry.odds.push_back(CDClientRules::Odds(Real(row, "percent"), static_cast<int32_t>(Int(row, "minToDrop")), static_cast<int32_t>(Int(row, "maxToDrop")), chance));
		}
		return entry;
	}

	// Per item of a loot matrix, the chance of getting it at least once and how many on average (entries roll independently)
	std::map<int64_t, CDClientRules::DropOdds> MatrixOdds(int64_t index) {
		std::map<int64_t, std::pair<double, double>> totals; // lot: chance of none, expected count
		for (const auto& row : LootMatrixRows(index)) {
			const auto entry = RollEntry(row);
			const auto& lootRows = LootTableRows(Int(row, "LootTableIndex"));
			for (size_t i = 0; i < lootRows.size(); i++) {
				auto& total = totals.try_emplace(Int(lootRows[i], "itemid"), 1.0, 0.0).first->second;
				total.first *= 1.0 - entry.odds[i].atLeastOne;
				total.second += entry.odds[i].expected;
			}
		}
		std::map<int64_t, CDClientRules::DropOdds> odds;
		for (const auto& [lot, total] : totals) odds[lot] = { 1.0 - total.first, total.second };
		return odds;
	}

	/**
	 * A loot matrix as a drop rolls it (RollLootMatrix, for smashables, activities and packages): per entry its chance,
	 * how many drops, the rarity each drop rolls and each item's chances; and per item the chance of getting it at
	 * least once from the whole matrix and how many on average.
	 */
	json LootMatrix(int64_t index) {
		json entries = json::array();
		for (const auto& row : LootMatrixRows(index)) {
			const auto entry = RollEntry(row);
			const auto& lootRows = LootTableRows(Int(row, "LootTableIndex"));
			json rolled = json::array();
			for (const auto& [rarity, chance] : CDClientRules::RarityChances(RarityRows(Int(row, "RarityTableIndex")))) rolled.push_back({ {"rarity", rarity}, {"chance", chance} });
			json items = json::array();
			for (size_t i = 0; i < lootRows.size(); i++) {
				items.push_back({ {"item", ItemRef(Int(lootRows[i], "itemid"))}, {"rarity", entry.rarities[i]}, {"missionDrop", Int(lootRows[i], "MissionDrop") == 1},
					{"perDrop", entry.perDrop[i]}, {"chance", entry.odds[i].atLeastOne}, {"expected", entry.odds[i].expected} });
			}
			std::stable_sort(items.begin(), items.end(), [](const json& a, const json& b) { return a["rarity"].get<int32_t>() > b["rarity"].get<int32_t>(); });
			entries.push_back({ {"lootTable", row["LootTableIndex"]}, {"rarityTable", row["RarityTableIndex"]}, {"percent", row["percent"]},
				{"minToDrop", row["minToDrop"]}, {"maxToDrop", row["maxToDrop"]}, {"flagID", row.contains("flagID") ? row["flagID"] : json(nullptr)},
				{"rolledRarity", rolled}, {"items", items} });
		}
		json items = json::array();
		for (const auto& [lot, odds] : MatrixOdds(index)) items.push_back({ {"item", ItemRef(lot)}, {"chance", odds.atLeastOne}, {"expected", odds.expected} });
		std::stable_sort(items.begin(), items.end(), [](const json& a, const json& b) { return a["chance"].get<double>() > b["chance"].get<double>(); });
		return { {"entries", entries}, {"items", items} };
	}

	// What an item costs at a vendor with this buy scalar (VendorComponent::Buy); coins are null when the scalar is 0,
	// which means the world's vendor_buy_multiplier
	json VendorPrice(const json& item, double buyScalar) {
		json price{ {"baseValue", item.value("baseValue", json(nullptr))}, {"coins", nullptr}, {"altCurrency", nullptr}, {"costs", json::array()} };
		const auto baseValue = Int(item, "baseValue", -1);
		if (buyScalar != 0.0 && baseValue >= 0) price["coins"] = static_cast<int64_t>(std::floor(baseValue * buyScalar));
		const auto currency = Int(item, "currencyLOT");
		if (currency > 0 && ItemRarities().byLot.contains(currency)) {
			price["altCurrency"] = { {"item", ItemRef(currency)}, {"base", Int(item, "altCurrencyCost")},
				{"count", buyScalar != 0.0 ? json(static_cast<int64_t>(std::floor(Int(item, "altCurrencyCost") * buyScalar))) : json(nullptr)} };
		}
		// currencyCosts "lot:count,lot:count" (CDItemComponentTable::ParseCraftingCurrencies), taken as they are
		for (const auto& part : GeneralUtils::SplitString(Text(item, "currencyCosts"), ',')) {
			const auto pair = GeneralUtils::SplitString(part, ':');
			if (pair.size() != 2) continue;
			const auto lot = GeneralUtils::TryParse<int64_t>(pair[0]);
			if (lot) price["costs"].push_back({ {"item", ItemRef(*lot)}, {"count", GeneralUtils::TryParse<int64_t>(pair[1]).value_or(0)} });
		}
		return price;
	}

	/**
	 * What a vendor sells (VendorComponent::RefreshInventory): per loot matrix entry the whole loot table when its
	 * minToDrop or maxToDrop is 0, otherwise that many of its items at random, picked again every refresh. Items
	 * without an item component are never stocked.
	 */
	json VendorStock(int64_t matrix, double buyScalar) {
		json entries = json::array();
		for (const auto& row : LootMatrixRows(matrix)) {
			const auto tableIndex = Int(row, "LootTableIndex");
			const auto minDrops = static_cast<int32_t>(Int(row, "minToDrop"));
			const auto maxDrops = static_cast<int32_t>(Int(row, "maxToDrop"));
			const auto items = Rows("SELECT lt.itemid, lt.sortPriority, cr.component_id AS itemComponent, ic.baseValue, ic.currencyLOT, ic.altCurrencyCost, ic.currencyCosts "
				"FROM LootTable lt LEFT JOIN ComponentsRegistry cr ON cr.id = lt.itemid AND cr.component_type = ? LEFT JOIN ItemComponent ic ON ic.id = cr.component_id "
				"WHERE lt.LootTableIndex = ? ORDER BY lt.sortPriority;", { ITEM_COMPONENT, tableIndex }, MAX_LOOT_ITEMS);
			const auto chance = CDClientRules::VendorStockChance(minDrops, maxDrops, items.size());
			json list = json::array();
			for (const auto& item : items) {
				const bool sold = !item["itemComponent"].is_null();
				list.push_back({ {"item", ItemRef(Int(item, "itemid"))}, {"chance", sold ? chance : 0.0}, {"sold", sold}, {"price", VendorPrice(item, buyScalar)} });
			}
			entries.push_back({ {"lootTable", tableIndex}, {"all", minDrops == 0 || maxDrops == 0}, {"minToDrop", minDrops}, {"maxToDrop", maxDrops}, {"items", list} });
		}
		return entries;
	}

	// Coins a destructible drops (Entity: CurrencyTable at its currency index and level) or an activity gives (level 1)
	json Coins(int64_t currencyIndex, int64_t level) {
		const auto rows = Rows("SELECT minvalue, maxvalue FROM CurrencyTable WHERE currencyIndex = ? AND npcminlevel = ? LIMIT 1;", { currencyIndex, level }, 1);
		if (rows.empty()) return nullptr;
		return { {"min", Int(rows[0], "minvalue")}, {"max", Int(rows[0], "maxvalue")} };
	}

	// ---- Views ----

	// A number that can be any of these kinds, as the first that has it; just the number when none does
	json Resolve(const std::vector<eLink>& kinds, int64_t id) {
		for (const auto kind : kinds) {
			if (Exists(kind, id)) return kind == eLink::OBJECT ? ItemRef(id) : Ref(kind, id);
		}
		return { {"id", id} };
	}

	json LootTableView(int64_t index) {
		json items = json::array();
		for (const auto& row : LootTableRows(index)) {
			const auto lot = Int(row, "itemid");
			items.push_back({ {"item", ItemRef(lot)}, {"rarity", RollRarity(lot)}, {"missionDrop", Int(row, "MissionDrop") == 1}, {"sortPriority", row["sortPriority"]} });
		}
		if (items.empty()) return nullptr;
		std::stable_sort(items.begin(), items.end(), [](const json& a, const json& b) { return a["rarity"].get<int32_t>() > b["rarity"].get<int32_t>(); });
		return { {"id", index}, {"items", items}, {"usedBy", References(eLink::LOOT_TABLE, index)} };
	}

	std::string EnumWords(std::string_view name, int64_t value) {
		return name.empty() ? std::to_string(value) : GameLabels::Words(name);
	}

	// The first row of an object's component of this type, if it has one
	std::optional<json> ComponentRow(LOT lot, eReplicaComponentType type) {
		const auto* table = ComponentTable(GetSchema(), type);
		if (!table || !table->Find("id")) return std::nullopt;
		const auto rows = Rows("SELECT c.* FROM ComponentsRegistry cr JOIN " + Quote(table->name) + " c ON c.id = cr.component_id WHERE cr.id = ? AND cr.component_type = ? LIMIT 1;",
			{ static_cast<int64_t>(lot), static_cast<int64_t>(type) }, 1);
		if (rows.empty()) return std::nullopt;
		return rows[0];
	}

	// An item's own details: what kind, where it goes, what's in it, what it needs
	json ItemDetails(LOT lot, const json& item) {
		const auto type = Int(item, "itemType", -1);
		json details{ {"rarity", item["rarity"]}, {"itemType", type}, {"itemTypeName", EnumWords(magic_enum::enum_name(static_cast<eItemType>(type)), type)},
			{"stackSize", item["stackSize"]}, {"baseValue", item["baseValue"]}, {"equipLocation", item["equipLocation"]}, {"subItems", json::array()},
			{"price", VendorPrice(item, 1.0)}, {"commendation", nullptr}, {"preconditions", json::array()} };
		// Proxy items equipped with it, read as InventoryComponent does: spaces dropped, then split at commas
		auto subItems = Text(item, "subItems");
		std::erase_if(subItems, [](unsigned char c) { return std::isspace(c); });
		for (const auto& sub : GeneralUtils::SplitString(subItems, ',')) {
			if (const auto subLot = GeneralUtils::TryParse<int64_t>(sub)) details["subItems"].push_back(ItemRef(*subLot));
		}
		if (Int(item, "commendationLOT") > 0) details["commendation"] = { {"item", ItemRef(Int(item, "commendationLOT"))}, {"count", Int(item, "commendationCost")} };
		// reqPrecondition lists Preconditions rows; show each with the reason the client gives when it isn't met
		for (const auto id : PrerequisiteMissions(Text(item, "reqPrecondition"))) {
			details["preconditions"].push_back({ {"id", id}, {"reason", Phrase("Preconditions_" + std::to_string(id) + "_FailureReason")} });
		}
		const auto info = ClientAssets::ItemInfo(lot);
		details["stats"] = info.value("stats", json(nullptr));
		details["set"] = info.value("set", json(nullptr));
		return details;
	}

	// The skills on an object (ObjectSkills) with what they cost and do
	json ObjectSkills(LOT lot) {
		json skills = json::array();
		for (const auto& row : Rows("SELECT os.skillID, os.castOnType, os.AICombatWeight, sb.behaviorID, sb.imaginationcost, sb.cooldown, sb.cooldowngroup, sb.skillIcon "
			"FROM ObjectSkills os LEFT JOIN SkillBehavior sb ON sb.skillID = os.skillID WHERE os.objectTemplate = ?;", { static_cast<int64_t>(lot) }, 100)) {
			auto skill = row;
			skill["skill"] = Ref(eLink::SKILL, Int(row, "skillID"));
			skill["description"] = Phrase("SkillBehavior_" + std::to_string(Int(row, "skillID")) + "_descriptionUI");
			skills.push_back(std::move(skill));
		}
		return skills;
	}

	/**
	 * Mission tasks that count an object, mission, skill... : tasks whose targets (as MissionTask::Progress compares
	 * them, see CDClientRules::MeaningOf) include `id` as that kind.
	 */
	json TasksCounting(eLink link, int64_t id) {
		json tasks = json::array();
		const auto text = std::to_string(id);
		for (const auto& task : Rows("SELECT * FROM MissionTasks WHERE target = ? OR targetGroup LIKE ? ESCAPE '\\';", { id, "%" + text + "%" }, 500)) {
			const auto type = static_cast<eMissionTaskType>(Int(task, "taskType", -1));
			const auto meaning = CDClientRules::MeaningOf(type);
			if (std::ranges::find(meaning.target, link) == meaning.target.end()) continue;
			// Numbers that can be more than one kind count as the first kind that has them
			if (meaning.target.size() > 1 && Resolve(meaning.target, id).value("link", "") != LinkName(link)) continue;
			bool counts = meaning.targetUsed && Int(task, "target") == id;
			if (!counts && meaning.targetGroupIds) {
				const auto group = CDClientRules::NumberList(Text(task, "targetGroup"));
				counts = std::ranges::find(group, id) != group.end();
			}
			if (!counts) continue;
			tasks.push_back({ {"mission", Ref(eLink::MISSION, Int(task, "id"))}, {"uid", task["uid"]}, {"taskType", EnumWords(magic_enum::enum_name(type), Int(task, "taskType", -1))},
				{"targetValue", task["targetValue"]} });
		}
		return tasks;
	}

	// Missions giving an item as a reward, first time or on repeats (Mission::YieldRewards)
	json MissionsRewarding(LOT lot) {
		json missions = json::array();
		std::string condition;
		for (int i = 1; i <= 4; i++) {
			const auto slot = std::to_string(i);
			condition += (i > 1 ? " OR " : "") + std::string("reward_item") + slot + " = ?1 OR reward_item" + slot + "_repeatable = ?1";
		}
		for (const auto& row : Rows("SELECT * FROM Missions WHERE " + condition + " LIMIT 200;", { static_cast<int64_t>(lot) }, 200)) {
			for (int i = 1; i <= 4; i++) {
				const auto slot = std::to_string(i);
				if (Int(row, "reward_item" + slot) == lot) missions.push_back({ {"mission", Ref(eLink::MISSION, Int(row, "id"))}, {"count", std::max<int64_t>(Int(row, "reward_item" + slot + "_count"), 1)}, {"repeat", false}, {"choice", Int(row, "isChoiceReward") == 1} });
				if (Int(row, "reward_item" + slot + "_repeatable") == lot) missions.push_back({ {"mission", Ref(eLink::MISSION, Int(row, "id"))}, {"count", std::max<int64_t>(Int(row, "reward_item" + slot + "_repeat_count"), 1)}, {"repeat", true}, {"choice", Int(row, "isChoiceReward") == 1} });
			}
		}
		return missions;
	}

	/**
	 * Where an object comes from through loot matrices: per table and column pointing at matrices that have it, each
	 * matrix with its chance (a drop's, or a vendor's chance of stocking it) and the objects or rows using it.
	 */
	json Sources(LOT lot) {
		const auto& schema = GetSchema();
		const auto* vendorTable = ComponentTable(schema, eReplicaComponentType::VENDOR);
		const auto matrices = Rows("SELECT DISTINCT lm.LootMatrixIndex, lm.LootTableIndex, lm.minToDrop, lm.maxToDrop FROM LootMatrix lm "
			"JOIN LootTable lt ON lt.LootTableIndex = lm.LootTableIndex WHERE lt.itemid = ?;", { static_cast<int64_t>(lot) }, SIZE_MAX);
		std::string withItem; // the matrices as an SQL list: numbers read from the database
		for (const auto& row : matrices) withItem += (withItem.empty() ? "" : ",") + std::to_string(Int(row, "LootMatrixIndex"));
		if (withItem.empty()) return json::array();

		// The chance per matrix, worked out once however many tables use it (up to MAX_MATRICES of them)
		std::map<int64_t, json> drops;
		const auto dropChance = [&](int64_t matrix) -> const json& {
			if (const auto it = drops.find(matrix); it != drops.end()) return it->second;
			json chance = json::object();
			if (drops.size() < MAX_MATRICES) {
				const auto odds = MatrixOdds(matrix);
				if (const auto it = odds.find(lot); it != odds.end()) chance = { {"chance", it->second.atLeastOne}, {"expected", it->second.expected} };
			}
			return drops[matrix] = chance;
		};
		// A vendor's chance of having it in stock, from the entries whose table has it
		const auto stockChance = [&](int64_t matrix) {
			double missing = 1.0;
			for (const auto& row : matrices) {
				if (Int(row, "LootMatrixIndex") != matrix) continue;
				const auto size = LootTableRows(Int(row, "LootTableIndex")).size();
				missing *= 1.0 - CDClientRules::VendorStockChance(static_cast<int32_t>(Int(row, "minToDrop")), static_cast<int32_t>(Int(row, "maxToDrop")), size);
			}
			return 1.0 - missing;
		};

		json sources = json::array();
		for (const auto& [table, column] : ColumnsLinkingTo(schema, eLink::LOOT_MATRIX)) {
			if (table->name == "LootMatrix" || table->name == "LootMatrixIndex") continue;
			const bool vendor = vendorTable && table == vendorTable;
			const auto types = ComponentTypesByTable().find(table->name);
			// Per matrix, the objects whose component uses it: one query for the table, not one per matrix
			std::map<int64_t, json> users;
			if (types != ComponentTypesByTable().end()) {
				for (const auto& row : Rows("SELECT DISTINCT c." + Quote(column->name) + " AS matrix, cr.id AS lot FROM ComponentsRegistry cr JOIN " + Quote(table->name) +
					" c ON c.id = cr.component_id WHERE cr.component_type IN (" + TypeList(types->second) + ") AND c." + Quote(column->name) + " IN (" + withItem + ");", {}, SIZE_MAX)) {
					auto& objects = users.try_emplace(Int(row, "matrix"), json::array()).first->second;
					if (objects.size() < MAX_OBJECTS) objects.push_back(ItemRef(Int(row, "lot")));
				}
			} else {
				for (const auto& row : Rows("SELECT * FROM " + Quote(table->name) + " WHERE " + Quote(column->name) + " IN (" + withItem + ");", {}, SIZE_MAX)) {
					auto& rows = users.try_emplace(Int(row, column->name), json::array()).first->second;
					if (rows.size() < REFERENCE_ROWS) rows.push_back(row);
				}
			}
			json list = json::array();
			for (const auto& [matrix, used] : users) {
				json entry{ {"matrix", matrix}, {"objects", types != ComponentTypesByTable().end() ? used : json::array()}, {"rows", types != ComponentTypesByTable().end() ? json::array() : used} };
				if (vendor) entry["chance"] = stockChance(matrix);
				else entry.update(dropChance(matrix));
				list.push_back(std::move(entry));
			}
			if (list.empty()) continue;
			std::stable_sort(list.begin(), list.end(), [](const json& a, const json& b) { return a.value("chance", 0.0) > b.value("chance", 0.0); });
			sources.push_back({ {"table", table->name}, {"column", column->name}, {"vendor", vendor}, {"matrices", list} });
		}
		return sources;
	}

	json ObjectView(LOT lot) {
		const auto object = RowsWhere("Objects", "id", lot, 1);
		if (object.empty()) return nullptr;
		const auto& schema = GetSchema();
		using enum eReplicaComponentType;

		json components = json::array();
		json loot = json::array();
		for (const auto& entry : RowsWhere("ComponentsRegistry", "id", lot, 200)) {
			const auto type = Int(entry, "component_type");
			const auto componentId = Int(entry, "component_id");
			const auto* table = ComponentTable(schema, static_cast<eReplicaComponentType>(type));
			json component{ {"type", type}, {"typeName", ComponentTypeName(type)}, {"componentId", componentId}, {"table", table ? json(table->name) : json(nullptr)}, {"rows", json::array()} };
			if (table && table->Find("id")) {
				component["rows"] = RowsWhere(*table, *table->Find("id"), componentId, 50);
				component["columns"] = Columns(*table);
				// What this object drops or contains: loot matrices its component rows point at (a vendor's are its stock, below)
				if (static_cast<eReplicaComponentType>(type) != VENDOR) {
					for (const auto& column : table->columns) {
						if (LinkFor(schema, *table, column) != eLink::LOOT_MATRIX) continue;
						for (const auto& row : component["rows"]) {
							const auto matrix = Int(row, column.name);
							if (matrix > 0) loot.push_back({ {"table", table->name}, {"column", column.name}, {"matrix", matrix}, {"odds", LootMatrix(matrix)} });
						}
					}
				}
			}
			components.push_back(std::move(component));
		}

		json view{
			{"lot", lot},
			{"name", ClientAssets::ItemName(lot)},
			{"description", Phrase("Objects_" + std::to_string(lot) + "_description")},
			{"row", object[0]},
			{"columns", Columns(*schema.Find("Objects"))},
			{"localized", Localized("Objects", lot)},
			{"icon", !ClientAssets::IconPathForLot(lot).empty()},
			{"components", components},
			{"loot", loot},
			{"skills", ObjectSkills(lot)},
			{"sources", Sources(lot)},
			{"rewardedBy", MissionsRewarding(lot)},
			{"countedBy", TasksCounting(eLink::OBJECT, lot)},
		};

		if (const auto item = ComponentRow(lot, ITEM)) view["item"] = ItemDetails(lot, *item);
		if (const auto destructible = ComponentRow(lot, BUFF)) {
			view["destructible"] = { {"life", (*destructible)["life"]}, {"armor", (*destructible)["armor"]}, {"imagination", (*destructible)["imagination"]},
				{"level", (*destructible)["level"]}, {"faction", (*destructible)["faction"]}, {"isSmashable", (*destructible)["isSmashable"]},
				{"coins", Coins(Int(*destructible, "CurrencyIndex"), Int(*destructible, "level"))} };
		}
		if (const auto vendor = ComponentRow(lot, VENDOR)) {
			const auto buyScalar = Real(*vendor, "buyScalar");
			view["vendor"] = { {"buyScalar", buyScalar}, {"sellScalar", (*vendor)["sellScalar"]}, {"refreshTimeSeconds", (*vendor)["refreshTimeSeconds"]},
				{"matrix", (*vendor)["LootMatrixIndex"]}, {"stock", VendorStock(Int(*vendor, "LootMatrixIndex"), buyScalar)} };
		}
		if (const auto collectible = ComponentRow(lot, COLLECTIBLE); collectible && Int(*collectible, "requirement_mission") > 0) {
			view["collectibleMission"] = Ref(eLink::MISSION, Int(*collectible, "requirement_mission"));
		}
		// What it starts with (InventoryComponent) and the missions it gives or takes (MissionNPCComponent)
		if (const auto* table = ComponentTable(schema, INVENTORY); table && table->Find("itemid")) {
			json inventory = json::array();
			for (const auto& row : Rows("SELECT c.itemid, c.count, c.equip FROM ComponentsRegistry cr JOIN " + Quote(table->name) + " c ON c.id = cr.component_id "
				"WHERE cr.id = ? AND cr.component_type = ?;", { static_cast<int64_t>(lot), static_cast<int64_t>(INVENTORY) }, 200)) {
				inventory.push_back({ {"item", ItemRef(Int(row, "itemid"))}, {"count", row["count"]}, {"equip", Int(row, "equip") == 1} });
			}
			if (!inventory.empty()) view["inventory"] = inventory;
		}
		if (const auto* table = ComponentTable(schema, MISSION_OFFER); table && table->Find("missionID")) {
			json missions = json::array();
			for (const auto& row : Rows("SELECT c.missionID, c.offersMission, c.acceptsMission FROM ComponentsRegistry cr JOIN " + Quote(table->name) + " c ON c.id = cr.component_id "
				"WHERE cr.id = ? AND cr.component_type = ?;", { static_cast<int64_t>(lot), static_cast<int64_t>(MISSION_OFFER) }, 500)) {
				missions.push_back({ {"mission", Ref(eLink::MISSION, Int(row, "missionID"))}, {"offers", Int(row, "offersMission") == 1}, {"accepts", Int(row, "acceptsMission") == 1} });
			}
			if (!missions.empty()) view["missions"] = missions;
		}

		// Its own ComponentsRegistry rows are listed above as its components
		auto usedBy = References(eLink::OBJECT, lot);
		usedBy.erase(std::remove_if(usedBy.begin(), usedBy.end(), [](const json& r) { return r["table"] == "ComponentsRegistry"; }), usedBy.end());
		view["usedBy"] = usedBy;
		const auto model = ObjectLxfml(lot);
		view["model"] = model ? json(model->second) : json(nullptr);
		return view;
	}

	json MissionView(int64_t id) {
		const auto mission = RowsWhere("Missions", "id", id, 1);
		if (mission.empty()) return nullptr;
		const auto& row = mission[0];

		json tasks = json::array();
		for (const auto& task : RowsWhere("MissionTasks", "id", id, 50)) {
			const auto type = Int(task, "taskType", -1);
			const auto meaning = CDClientRules::MeaningOf(static_cast<eMissionTaskType>(type));
			json entry{ {"uid", task["uid"]}, {"taskType", type}, {"taskTypeName", EnumWords(magic_enum::enum_name(static_cast<eMissionTaskType>(type)), type)},
				{"description", Phrase("MissionTasks_" + std::to_string(Int(task, "uid")) + "_description")},
				{"targetValue", task["targetValue"]}, {"progressed", meaning.progressed}, {"iconID", task["IconID"]},
				{"targets", json::array()}, {"targetText", nullptr}, {"parameters", json::array()}, {"racingParameter", nullptr}, {"row", task} };
			std::vector<int64_t> targets;
			if (meaning.targetUsed) targets.push_back(Int(task, "target"));
			if (meaning.targetGroupIds) for (const auto number : CDClientRules::NumberList(Text(task, "targetGroup"))) targets.push_back(number);
			for (const auto target : targets) {
				// MissionTask skips a target it can't read and -1 never matches; 0 is only ever progressed by the "value 0" tasks
				if (target > 0 || (target == 0 && meaning.target.empty())) entry["targets"].push_back(Resolve(meaning.target, target));
			}
			if (meaning.targetGroupText && !Text(task, "targetGroup").empty()) entry["targetText"] = Text(task, "targetGroup");
			const auto parameters = CDClientRules::NumberList(Text(task, "taskParam1"));
			if (meaning.racingParameter && !parameters.empty()) {
				entry["racingParameter"] = EnumWords(magic_enum::enum_name(static_cast<eRacingTaskParam>(parameters[0])), parameters[0]);
			} else if (!meaning.parameters.empty()) {
				for (const auto parameter : parameters) entry["parameters"].push_back(Resolve(meaning.parameters, parameter));
			}
			tasks.push_back(std::move(entry));
		}

		// Mission::YieldRewards: the first completion gives the items (a count of 0 gives 1) and the stat rewards; later
		// ones only the repeat items and coins. A choice reward gives only the item picked.
		json rewards{ {"items", json::array()}, {"repeatItems", json::array()}, {"emotes", json::array()}, {"stats", json::array()} };
		for (int i = 1; i <= 4; i++) {
			const auto slot = std::to_string(i);
			const auto item = Int(row, "reward_item" + slot);
			const auto count = Int(row, "reward_item" + slot + "_count");
			if (item > 0 && count >= 0) rewards["items"].push_back({ {"item", ItemRef(item)}, {"count", std::max<int64_t>(count, 1)} });
			const auto repeatItem = Int(row, "reward_item" + slot + "_repeatable");
			const auto repeatCount = Int(row, "reward_item" + slot + "_repeat_count");
			if (repeatItem > 0 && repeatCount >= 0) rewards["repeatItems"].push_back({ {"item", ItemRef(repeatItem)}, {"count", std::max<int64_t>(repeatCount, 1)} });
			const auto emote = Int(row, i == 1 ? "reward_emote" : "reward_emote" + slot);
			if (emote > 0) rewards["emotes"].push_back(Ref(eLink::EMOTE, emote));
		}
		// The other reward_ columns the server gives the first time (inventory space, health, imagination, ...), as named
		for (const auto& [key, value] : row.items()) {
			if (!key.starts_with("reward_max") && key != "reward_bankinventory") continue;
			if (value.is_number() && value.get<int64_t>() > 0) rewards["stats"].push_back({ {"column", key}, {"value", value} });
		}

		json prerequisites = json::array();
		for (const auto& term : CDClientRules::ParsePrerequisites(Text(row, "prereqMissionID"))) {
			prerequisites.push_back({ {"mission", term.mission ? Ref(eLink::MISSION, term.mission) : json(nullptr)}, {"state", term.state},
				{"stateName", term.state ? json(EnumWords(magic_enum::enum_name(static_cast<eMissionState>(term.state)), term.state)) : json(nullptr)}, {"or", term.orRest} });
		}

		// Missions that need this one: prereqMissionID is text, so find candidates and check each properly
		json unlocks = json::array();
		for (const auto& candidate : Rows("SELECT id, prereqMissionID FROM Missions WHERE prereqMissionID LIKE ? ESCAPE '\\';", { "%" + std::to_string(id) + "%" }, 500)) {
			const auto terms = CDClientRules::ParsePrerequisites(Text(candidate, "prereqMissionID"));
			if (std::ranges::any_of(terms, [&](const auto& term) { return static_cast<int64_t>(term.mission) == id; })) unlocks.push_back(Ref(eLink::MISSION, Int(candidate, "id")));
		}

		return {
			{"id", id},
			{"name", LinkLabel(eLink::MISSION, id)},
			{"row", row},
			{"columns", Columns(*GetSchema().Find("Missions"))},
			{"localized", Localized("Missions", id)},
			{"text", Localized("MissionText", id)},
			{"tasks", tasks},
			{"rewards", rewards},
			{"prerequisites", prerequisites},
			{"unlocks", unlocks},
			{"countedBy", TasksCounting(eLink::MISSION, id)},
			{"offeredBy", ObjectsWithComponent("MissionNPCComponent", "c.missionID = ? AND c.offersMission = 1", { id })},
			{"acceptedBy", ObjectsWithComponent("MissionNPCComponent", "c.missionID = ? AND c.acceptsMission = 1", { id })},
			{"usedBy", References(eLink::MISSION, id)}
		};
	}

	// A template's name from the server's enum (BehaviorTemplate.h), or the CDClient's own name for one it doesn't know
	std::string TemplateName(int64_t templateId, const std::string& clientName) {
		const auto name = magic_enum::enum_name(static_cast<BehaviorTemplate>(templateId));
		if (!name.empty()) return GameLabels::Words(name);
		return clientName.empty() ? "Template " + std::to_string(templateId) : clientName;
	}

	/**
	 * A behavior and everything under it, read a level at a time: {root, nodes: {id: {template, effect, parameters:
	 * [{name, value, child}]}}, truncated}. A behavior used twice is listed once.
	 */
	json BehaviorTree(int64_t root) {
		json nodes = json::object();
		std::set<int64_t> seen{ root };
		std::vector<int64_t> level{ root };
		bool truncated = false;
		for (size_t depth = 0; !level.empty(); depth++) {
			if (depth >= MAX_BEHAVIOR_DEPTH || seen.size() > MAX_BEHAVIOR_NODES) {
				truncated = true;
				break;
			}
			std::string placeholders;
			std::vector<Value> ids;
			for (const auto id : level) {
				placeholders += placeholders.empty() ? "?" : ",?";
				ids.emplace_back(id);
			}
			for (const auto& row : Rows("SELECT bt.behaviorID, bt.templateID, btn.name AS clientName, bt.effectID, bt.effectHandle FROM BehaviorTemplate bt "
				"LEFT JOIN BehaviorTemplateName btn ON btn.templateID = bt.templateID WHERE bt.behaviorID IN (" + placeholders + ");", ids, level.size())) {
				auto node = row;
				node["templateName"] = TemplateName(Int(row, "templateID"), Text(row, "clientName"));
				node["parameters"] = json::array();
				nodes[std::to_string(Int(row, "behaviorID"))] = std::move(node);
			}

			std::vector<int64_t> next;
			for (const auto& row : Rows("SELECT behaviorID, parameterID, value FROM BehaviorParameter WHERE behaviorID IN (" + placeholders + ") ORDER BY behaviorID, parameterID;",
				ids, MAX_BEHAVIOR_NODES * 40)) {
				const auto key = std::to_string(Int(row, "behaviorID"));
				if (!nodes.contains(key)) continue;
				const std::string name = Text(row, "parameterID");
				const double value = Real(row, "value");
				json parameter{ {"name", name}, {"value", value}, {"child", nullptr} };
				if (IsChildBehaviorParameter(name) && value > 0 && std::floor(value) == value) {
					const auto child = static_cast<int64_t>(value);
					parameter["child"] = child;
					if (seen.insert(child).second) next.push_back(child);
				}
				nodes[key]["parameters"].push_back(std::move(parameter));
			}
			level = std::move(next);
		}
		return { {"root", root}, {"nodes", nodes}, {"truncated", truncated} };
	}

	json SkillView(int64_t id) {
		const auto skill = RowsWhere("SkillBehavior", "skillID", id, 1);
		if (skill.empty()) return nullptr;
		json objects = json::array();
		for (const auto& row : Rows("SELECT objectTemplate, castOnType FROM ObjectSkills WHERE skillID = ? LIMIT ?;", { id, static_cast<int64_t>(MAX_OBJECTS) }, MAX_OBJECTS)) {
			objects.push_back({ {"object", ItemRef(Int(row, "objectTemplate"))}, {"castOnType", row["castOnType"]} });
		}
		const auto behavior = Int(skill[0], "behaviorID");
		return {
			{"id", id},
			{"name", LinkLabel(eLink::SKILL, id)},
			{"description", Phrase("SkillBehavior_" + std::to_string(id) + "_descriptionUI")},
			{"row", skill[0]},
			{"columns", Columns(*GetSchema().Find("SkillBehavior"))},
			{"localized", Localized("SkillBehavior", id)},
			{"objects", objects},
			{"countedBy", TasksCounting(eLink::SKILL, id)},
			{"tree", behavior > 0 && Exists(eLink::BEHAVIOR, behavior) ? BehaviorTree(behavior) : json(nullptr)},
			{"usedBy", References(eLink::SKILL, id)}
		};
	}

	json ActivityView(int64_t id) {
		const auto activity = RowsWhere("Activities", "ActivityID", id, 1);
		if (activity.empty()) return nullptr;
		const auto& row = activity[0];
		// ActivityRewards in table order: an activity instance rewards the first row, scripts that rate a result
		// (Loot::DropActivityLoot) the one with the highest activityRating at or below it; coins at level 1
		json rewards = json::array();
		for (const auto& reward : RowsWhere("ActivityRewards", "objectTemplate", id, 50)) {
			const auto matrix = Int(reward, "LootMatrixIndex");
			rewards.push_back({ {"rating", reward["activityRating"]}, {"challengeRating", reward["ChallengeRating"]}, {"description", reward["description"]},
				{"matrix", matrix}, {"coins", Coins(Int(reward, "CurrencyIndex"), 1)}, {"odds", matrix > 0 ? LootMatrix(matrix) : json(nullptr)} });
		}
		const auto zone = Int(row, "instanceMapID");
		const auto cost = Int(row, "optionalCostLOT");
		return {
			{"id", id},
			{"name", LinkLabel(eLink::ACTIVITY, id)},
			{"row", row},
			{"columns", Columns(*GetSchema().Find("Activities"))},
			{"localized", Localized("Activities", id)},
			{"zone", zone > 0 ? Ref(eLink::ZONE, zone) : json(nullptr)},
			{"cost", cost > 0 && Int(row, "optionalCostCount") > 0 ? json{ {"item", ItemRef(cost)}, {"count", row["optionalCostCount"]} } : json(nullptr)},
			{"rewards", rewards},
			{"countedBy", TasksCounting(eLink::ACTIVITY, id)},
			{"usedBy", References(eLink::ACTIVITY, id)}
		};
	}

	/**
	 * A zone: its row, names, the player caps new instances get (InstanceManager::GetSoftCap/GetHardCap: an override
	 * set on the Instances page, else the ZoneTable's), and what is played or launched there.
	 */
	json ZoneView(int64_t id, bool withOverrides) {
		const auto zone = RowsWhere("ZoneTable", "zoneID", id, 1);
		if (zone.empty()) return nullptr;
		const auto& row = zone[0];
		// As InstanceLoad reads them: NULL caps count as 8 and 12
		const InstanceLimits::Caps client{ static_cast<uint32_t>(Int(row, "population_soft_cap", 8)), static_cast<uint32_t>(Int(row, "population_hard_cap", 12)) };
		std::optional<uint32_t> softOverride, hardOverride;
		if (withOverrides) {
			for (const auto& limit : Database::Get()->GetZoneLimits()) {
				if (limit.zoneId != id) continue;
				softOverride = limit.softCap;
				hardOverride = limit.hardCap;
			}
		}
		const auto effective = InstanceLimits::Effective(softOverride, hardOverride, client);
		const json caps{ {"clientSoft", client.soft}, {"clientHard", client.hard}, {"soft", effective.soft}, {"hard", effective.hard}, {"overridesShown", withOverrides},
			{"softOverride", softOverride ? json(*softOverride) : json(nullptr)}, {"hardOverride", hardOverride ? json(*hardOverride) : json(nullptr)} };
		const auto control = Int(row, "zoneControlTemplate");
		return {
			{"id", id},
			{"name", LinkLabel(eLink::ZONE, id)},
			{"row", row},
			{"columns", Columns(*GetSchema().Find("ZoneTable"))},
			{"localized", Localized("ZoneTable", id)},
			{"caps", caps},
			{"control", control > 0 ? ItemRef(control) : json(nullptr)},
			{"usedBy", References(eLink::ZONE, id)}
		};
	}

	// ---- Search ----

	// id -> localized name for the kinds whose names are only in the locale
	const std::vector<std::pair<int64_t, std::string>>& LocaleNames(const std::string& prefix, const std::string& suffix) {
		static std::map<std::string, std::vector<std::pair<int64_t, std::string>>> cache;
		auto& names = cache[prefix + suffix];
		if (!names.empty()) return names;
		for (const auto& key : Locale::GetPhraseIdsWithPrefix(prefix)) {
			if (!key.ends_with(suffix) || key.size() <= prefix.size() + suffix.size()) continue;
			const auto id = GeneralUtils::TryParse<int64_t>(key.substr(prefix.size(), key.size() - prefix.size() - suffix.size()));
			if (id) names.emplace_back(*id, Locale::GetPhrase(key));
		}
		std::ranges::sort(names);
		return names;
	}

	json SearchLocale(eLink link, const std::string& prefix, const std::string& suffix, const std::string& text) {
		json results = json::array();
		const auto wanted = Lower(text);
		for (const auto& [id, name] : LocaleNames(prefix, suffix)) {
			if (Lower(name).find(wanted) == std::string::npos) continue;
			results.push_back({ {"link", LinkName(link)}, {"id", id}, {"name", name} });
			if (results.size() >= SEARCH_RESULTS) break;
		}
		return results;
	}

	json Search(const std::string& text) {
		json results{ {"exact", json::array()}, {"objects", json::array()}, {"missions", json::array()}, {"skills", json::array()}, {"activities", json::array()},
			{"zones", json::array()} };
		if (const auto id = GeneralUtils::TryParse<int64_t>(text)) {
			for (const auto link : magic_enum::enum_values<eLink>()) {
				if (Exists(link, *id)) results["exact"].push_back(Ref(link, *id));
			}
			return results;
		}
		// Objects by the name players see (the locale), then by their names in the CDClient
		std::set<int64_t> found;
		for (const auto& ref : SearchLocale(eLink::OBJECT, "Objects_", "_name", text)) {
			found.insert(ref["id"].get<int64_t>());
			results["objects"].push_back(ItemRef(ref["id"].get<int64_t>()));
		}
		for (const auto& row : Rows("SELECT id FROM Objects WHERE name LIKE ? ESCAPE '\\' OR displayName LIKE ? ESCAPE '\\' ORDER BY id LIMIT ?;",
			{ "%" + LikeEscape(text) + "%", "%" + LikeEscape(text) + "%", static_cast<int64_t>(SEARCH_RESULTS) }, SEARCH_RESULTS)) {
			if (results["objects"].size() >= SEARCH_RESULTS) break;
			if (found.insert(Int(row, "id")).second) results["objects"].push_back(ItemRef(Int(row, "id")));
		}
		results["missions"] = SearchLocale(eLink::MISSION, "Missions_", "_name", text);
		results["skills"] = SearchLocale(eLink::SKILL, "SkillBehavior_", "_name", text);
		results["activities"] = SearchLocale(eLink::ACTIVITY, "Activities_", "_ActivityName", text);
		results["zones"] = SearchLocale(eLink::ZONE, "ZoneTable_", "_DisplayDescription", text);
		return results;
	}

	// ---- Routes ----

	template<typename View>
	void ViewRoute(const std::string& path, const std::string& description, View view) {
		Route(eHTTPMethod::GET, path, Perm("dev_cdclient"), description, [view](HTTPReply& reply, const HTTPContext& context) {
			const auto id = PathId<int64_t>(context.path, 3);
			if (!id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Give a number");
			auto result = view(*id);
			if (result.is_null()) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Not in the CDClient");
			JsonReply(reply, eHTTPStatusCode::OK, result);
		});
	}
}

void RegisterCDClientBrowserRoutes() {
	Route(eHTTPMethod::GET, "/cdclient", Perm("dev_cdclient"), "The CDClient browser",
		[](HTTPReply& reply, const HTTPContext& context) {
			RenderPage(reply, context, "cdclient.jinja2", "cdclient");
		});

	Route(eHTTPMethod::GET, "/api/cdclient/schema", Perm("dev_cdclient"), "Every CDClient table with its columns and what each column links to",
		[](HTTPReply& reply, const HTTPContext&) {
			static const auto schema = [] {
				json tables = json::array();
				for (const auto& [name, table] : GetSchema().Tables()) tables.push_back({ {"name", name}, {"columns", Columns(table)} });
				json links = json::object();
				for (const auto link : magic_enum::enum_values<eLink>()) {
					const auto target = Target(link);
					links[LinkName(link)] = { {"table", target.table}, {"column", target.column} };
				}
				json componentTypes = json::array();
				for (const auto type : magic_enum::enum_values<eReplicaComponentType>()) {
					const auto* table = ComponentTable(GetSchema(), type);
					componentTypes.push_back({ {"type", static_cast<int64_t>(type)}, {"name", ComponentTypeName(static_cast<int64_t>(type))}, {"table", table ? json(table->name) : json(nullptr)} });
				}
				return json{ {"tables", tables}, {"links", links}, {"componentTypes", componentTypes} };
			}();
			JsonReply(reply, eHTTPStatusCode::OK, schema);
		});

	Route(eHTTPMethod::GET, "/api/cdclient/tables/:name/rows", Perm("dev_cdclient"),
		"A page of a CDClient table. Query: start, length (max 500), order (column), dir (asc/desc), q (search), filters (JSON [{column, op, value}]; op is = != < <= > >= contains starts null notnull)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto& query = context.queryString;
			RowQuery request;
			request.table = std::string(PathSegment(context.originalPath, 3)); // table names mix cases
			request.start = GeneralUtils::TryParse<uint32_t>(QueryValue(query, "start")).value_or(0);
			request.length = GeneralUtils::TryParse<uint32_t>(QueryValue(query, "length")).value_or(DEFAULT_PAGE);
			request.orderColumn = QueryValue(query, "order");
			request.ascending = QueryValue(query, "dir") != "desc";
			request.search = QueryValue(query, "q");
			const auto filters = QueryValue(query, "filters");
			if (!filters.empty()) {
				const auto parsed = json::parse(filters, nullptr, false);
				if (!parsed.is_array()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "filters must be a JSON array");
				for (const auto& filter : parsed) {
					if (!filter.is_object()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Each filter is {column, op, value}");
					const auto& value = filter.contains("value") ? filter["value"] : json("");
					request.filters.push_back({ Text(filter, "column"), Text(filter, "op", "="), value.is_string() ? value.get<std::string>() : value.dump() });
				}
			}

			std::string error;
			const auto built = BuildRowQuery(GetSchema(), request, error);
			if (!built) return JsonError(reply, error == "Unknown table" ? eHTTPStatusCode::NOT_FOUND : eHTTPStatusCode::BAD_REQUEST, error);
			const auto* table = GetSchema().Find(request.table);
			JsonReply(reply, eHTTPStatusCode::OK, {
				{"table", table->name},
				{"columns", Columns(*table)},
				{"total", Count(built->count, built->countParams)},
				{"start", request.start},
				{"rows", Rows(built->select, built->params, MAX_ROWS)}
			});
		});

	Route(eHTTPMethod::GET, "/api/cdclient/search", Perm("dev_cdclient"), "Find objects, missions, skills, activities and zones by name, or anything by ID. Query: q",
		[](HTTPReply& reply, const HTTPContext& context) {
			auto text = QueryValue(context.queryString, "q");
			text.erase(0, text.find_first_not_of(" \t"));
			text.erase(text.find_last_not_of(" \t") + 1);
			if (text.size() < 2 && !GeneralUtils::TryParse<int64_t>(text)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Type at least 2 letters, or an ID");
			JsonReply(reply, eHTTPStatusCode::OK, Search(text));
		});

	ViewRoute("/api/cdclient/objects/:id", "An object as the game uses it: its components (item, health and drops, skills, vendor stock, "
		"starting inventory, missions), where it comes from with the chances, the missions that reward or count it, and what else uses it",
		[](int64_t id) { return ObjectView(static_cast<LOT>(id)); });
	ViewRoute("/api/cdclient/missions/:id", "A mission: tasks with their targets, rewards, texts, prerequisites as the server reads them, what it unlocks and who offers it", MissionView);
	ViewRoute("/api/cdclient/skills/:id", "A skill: its costs, behavior tree, the objects that have it and what uses it", SkillView);
	ViewRoute("/api/cdclient/behaviors/:id", "A behavior tree with every behavior's template, effect and parameters", [](int64_t id) {
		return Exists(eLink::BEHAVIOR, id) ? json{ {"tree", BehaviorTree(id)}, {"usedBy", References(eLink::BEHAVIOR, id)} } : json(nullptr);
	});
	ViewRoute("/api/cdclient/activities/:id", "An activity: where it is played, what it costs, its rewards per rating with drop chances, and what uses it", ActivityView);
	ViewRoute("/api/cdclient/loot_matrix/:id", "A loot matrix with the chance of each item dropping, as the server rolls it, and what uses it", [](int64_t id) {
		auto odds = LootMatrix(id);
		return odds["entries"].empty() ? json(nullptr) : json{ {"id", id}, {"odds", odds}, {"usedBy", References(eLink::LOOT_MATRIX, id)} };
	});
	ViewRoute("/api/cdclient/loot_table/:id", "A loot table's items with their rarity and the loot matrices that use it", LootTableView);

	Route(eHTTPMethod::GET, "/api/cdclient/zones/:id", Perm("dev_cdclient"), "A zone: its names, the player caps new instances get (with any override set on the Instances page) and what uses it",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto id = PathId<int64_t>(context.path, 3);
			if (!id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Give a number");
			// The overrides are the Instances page's, so only for those who can see it
			auto result = ZoneView(*id, Can(context, "health_view"));
			if (result.is_null()) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Not in the CDClient");
			JsonReply(reply, eHTTPStatusCode::OK, result);
		});

	Route(eHTTPMethod::GET, "/api/cdclient/objects/:id/lxfml", Perm("dev_cdclient"), "An object's model as LXFML for the 3D preview, when the client has one",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto lot = PathId<LOT>(context.path, 3);
			const auto model = lot ? ObjectLxfml(*lot) : std::nullopt;
			if (!model) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No brick model for this object (is client_location set?)");
			reply.status = eHTTPStatusCode::OK;
			reply.message = model->first;
			reply.contentType = eContentType::APPLICATION_OCTET_STREAM;
			reply.headers.push_back("Cache-Control: private, max-age=86400");
		});
}
