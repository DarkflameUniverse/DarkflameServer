#include <gtest/gtest.h>

#include "CDClientRules.h"
#include "CDClientSchema.h"

using namespace CDClientSchema;

namespace {
	// A small CDClient-like schema
	Schema TestSchema() {
		Schema schema;
		schema.Add({ "Objects", { { "id", "int32" }, { "name", "text_4" }, { "placeable", "int_bool" }, { "interactionDistance", "real" } } });
		schema.Add({ "LootMatrix", { { "LootMatrixIndex", "int32" }, { "LootTableIndex", "int32" }, { "percent", "real" } } });
		schema.Add({ "LootTable", { { "itemid", "int32" }, { "LootTableIndex", "int32" } } });
		schema.Add({ "DestructibleComponent", { { "id", "int32" }, { "LootMatrixIndex", "int32" }, { "death_behavior", "int32" } } });
		schema.Add({ "Missions", { { "id", "int32" }, { "reward_item1", "int32" }, { "prereqMissionID", "text_4" } } });
		schema.Add({ "ActivityRewards", { { "objectTemplate", "int32" }, { "LootMatrixIndex", "int32" } } });
		schema.Add({ "Activities", { { "ActivityID", "int32" } } });
		schema.Add({ "ObjectSkills", { { "objectTemplate", "int32" }, { "skillID", "int32" } } });
		schema.Add({ "ObjectBehaviors", { { "BehaviorID", "int64" } } });
		schema.Add({ "VendorComponent", { { "id", "int32" } } });
		schema.Add({ "RenderComponent", { { "id", "int32" } } });
		schema.Add({ "PhysicsComponent", { { "id", "int32" } } });
		schema.Add({ "Weird\"Name", { { "a\"b", "text_4" } } });
		return schema;
	}

	const Table& T(const Schema& schema, const char* name) { return *schema.Find(name); }
	const Column& C(const Schema& schema, const char* table, const char* column) { return *schema.Find(table)->Find(column); }
}

TEST(CDClientSchemaTest, ValueTypes) {
	EXPECT_EQ(ValueType("int32"), eValueType::INTEGER);
	EXPECT_EQ(ValueType("int_bool"), eValueType::INTEGER);
	EXPECT_EQ(ValueType("INT64"), eValueType::INTEGER);
	EXPECT_EQ(ValueType("REAL"), eValueType::REAL);
	EXPECT_EQ(ValueType("text_4"), eValueType::TEXT);
}

TEST(CDClientSchemaTest, Quoting) {
	EXPECT_EQ(Quote("Objects"), "\"Objects\"");
	EXPECT_EQ(Quote("a\"b"), "\"a\"\"b\"");
	EXPECT_EQ(LikeEscape("50%_off\\"), "50\\%\\_off\\\\");
}

TEST(CDClientSchemaTest, Links) {
	const auto schema = TestSchema();
	EXPECT_EQ(LinkFor(schema, T(schema, "LootTable"), C(schema, "LootTable", "itemid")), eLink::OBJECT);
	EXPECT_EQ(LinkFor(schema, T(schema, "LootMatrix"), C(schema, "LootMatrix", "LootTableIndex")), eLink::LOOT_TABLE);
	EXPECT_EQ(LinkFor(schema, T(schema, "DestructibleComponent"), C(schema, "DestructibleComponent", "LootMatrixIndex")), eLink::LOOT_MATRIX);
	EXPECT_EQ(LinkFor(schema, T(schema, "Missions"), C(schema, "Missions", "reward_item1")), eLink::OBJECT);
	EXPECT_EQ(LinkFor(schema, T(schema, "ObjectSkills"), C(schema, "ObjectSkills", "objectTemplate")), eLink::OBJECT);
	// Table-specific rules win: ActivityRewards.objectTemplate is the activity
	EXPECT_EQ(LinkFor(schema, T(schema, "ActivityRewards"), C(schema, "ActivityRewards", "objectTemplate")), eLink::ACTIVITY);
	// A table's own key isn't a link
	EXPECT_FALSE(LinkFor(schema, T(schema, "LootMatrix"), C(schema, "LootMatrix", "LootMatrixIndex")));
	// Text and boolean columns never link
	EXPECT_FALSE(LinkFor(schema, T(schema, "Missions"), C(schema, "Missions", "prereqMissionID")));
	EXPECT_FALSE(LinkFor(schema, T(schema, "Objects"), C(schema, "Objects", "placeable")));
	// Model behaviors aren't skill behaviors
	EXPECT_FALSE(LinkFor(schema, T(schema, "ObjectBehaviors"), C(schema, "ObjectBehaviors", "BehaviorID")));
	// No target table in the schema: no link
	EXPECT_FALSE(LinkFor(schema, T(schema, "ObjectSkills"), C(schema, "ObjectSkills", "skillID")));

	const auto toMatrices = ColumnsLinkingTo(schema, eLink::LOOT_MATRIX);
	EXPECT_EQ(toMatrices.size(), 2u); // ActivityRewards and DestructibleComponent
}

TEST(CDClientSchemaTest, ComponentTables) {
	const auto schema = TestSchema();
	const auto name = [&](eReplicaComponentType type) {
		const auto* table = ComponentTable(schema, type);
		return table ? table->name : std::string("-");
	};
	EXPECT_EQ(name(eReplicaComponentType::VENDOR), "VendorComponent");
	EXPECT_EQ(name(eReplicaComponentType::RENDER), "RenderComponent");
	EXPECT_EQ(name(eReplicaComponentType::BUFF), "DestructibleComponent");
	EXPECT_EQ(name(eReplicaComponentType::SIMPLE_PHYSICS), "PhysicsComponent");
	EXPECT_EQ(name(eReplicaComponentType::PET), "-");
}

TEST(CDClientSchemaTest, RowQuery) {
	const auto schema = TestSchema();
	std::string error;
	RowQuery query;
	query.table = "Objects";
	query.filters = { { "id", ">=", "10" }, { "name", "contains", "50%" }, { "interactionDistance", "null", "" } };
	query.orderColumn = "name";
	query.ascending = false;
	query.start = 20;
	query.length = 10;
	const auto built = BuildRowQuery(schema, query, error);
	ASSERT_TRUE(built) << error;
	EXPECT_EQ(built->select, "SELECT * FROM \"Objects\" WHERE \"id\" >= ? AND CAST(\"name\" AS TEXT) LIKE ? ESCAPE '\\' AND \"interactionDistance\" IS NULL "
		"ORDER BY \"name\" DESC LIMIT ? OFFSET ?;");
	EXPECT_EQ(built->count, "SELECT COUNT(*) FROM \"Objects\" WHERE \"id\" >= ? AND CAST(\"name\" AS TEXT) LIKE ? ESCAPE '\\' AND \"interactionDistance\" IS NULL;");
	ASSERT_EQ(built->params.size(), 4u);
	EXPECT_EQ(std::get<int64_t>(built->params[0]), 10);
	EXPECT_EQ(std::get<std::string>(built->params[1]), "%50\\%%");
	EXPECT_EQ(std::get<int64_t>(built->params[2]), 10);
	EXPECT_EQ(std::get<int64_t>(built->params[3]), 20);
	EXPECT_EQ(built->countParams.size(), 2u);
}

TEST(CDClientSchemaTest, RowQuerySearchesEveryColumn) {
	const auto schema = TestSchema();
	std::string error;
	RowQuery query;
	query.table = "Objects";
	query.search = "12";
	const auto built = BuildRowQuery(schema, query, error);
	ASSERT_TRUE(built);
	// Integer and real columns compare as numbers, text columns contain it
	EXPECT_NE(built->select.find("(\"id\" = ? OR \"name\" LIKE ? ESCAPE '\\' OR \"placeable\" = ? OR \"interactionDistance\" = ?)"), std::string::npos);

	query.search = "abc";
	const auto text = BuildRowQuery(schema, query, error);
	ASSERT_TRUE(text);
	EXPECT_NE(text->select.find("(\"name\" LIKE ? ESCAPE '\\')"), std::string::npos);
}

TEST(CDClientSchemaTest, RowQueryRejectsWhatIsNotInTheSchema) {
	const auto schema = TestSchema();
	std::string error;
	RowQuery query;
	query.table = "sqlite_master";
	EXPECT_FALSE(BuildRowQuery(schema, query, error));
	EXPECT_EQ(error, "Unknown table");

	query.table = "Objects";
	query.filters = { { "id; DROP TABLE Objects", "=", "1" } };
	EXPECT_FALSE(BuildRowQuery(schema, query, error));

	query.filters = { { "id", "OR 1=1 --", "1" } };
	EXPECT_FALSE(BuildRowQuery(schema, query, error));

	query.filters = { { "id", "=", "1 OR 1=1" } };
	EXPECT_FALSE(BuildRowQuery(schema, query, error)); // not a number

	query.filters = {};
	query.orderColumn = "nope";
	EXPECT_FALSE(BuildRowQuery(schema, query, error));

	query.orderColumn = "";
	query.filters.assign(MAX_FILTERS + 1, { "id", "=", "1" });
	EXPECT_FALSE(BuildRowQuery(schema, query, error));
}

TEST(CDClientSchemaTest, RowQueryQuotesOddNamesAndCapsLength) {
	const auto schema = TestSchema();
	std::string error;
	RowQuery query;
	query.table = "Weird\"Name";
	query.filters = { { "a\"b", "=", "x" } };
	query.length = 100000;
	const auto built = BuildRowQuery(schema, query, error);
	ASSERT_TRUE(built);
	EXPECT_EQ(built->select, "SELECT * FROM \"Weird\"\"Name\" WHERE \"a\"\"b\" = ? LIMIT ? OFFSET ?;");
	EXPECT_EQ(std::get<int64_t>(built->params[1]), MAX_ROWS);
}

TEST(CDClientSchemaTest, ChildBehaviorParameters) {
	EXPECT_TRUE(IsChildBehaviorParameter("action"));
	EXPECT_TRUE(IsChildBehaviorParameter("miss action"));
	EXPECT_TRUE(IsChildBehaviorParameter("behavior 3"));
	EXPECT_TRUE(IsChildBehaviorParameter("on_fail_armor"));
	EXPECT_TRUE(IsChildBehaviorParameter("action_true"));
	EXPECT_TRUE(IsChildBehaviorParameter("ground_action"));
	EXPECT_FALSE(IsChildBehaviorParameter("include_faction"));
	EXPECT_FALSE(IsChildBehaviorParameter("isEnemyFaction"));
	EXPECT_FALSE(IsChildBehaviorParameter("radius"));
}

TEST(CDClientSchemaTest, PrerequisiteMissions) {
	EXPECT_EQ(PrerequisiteMissions("815|812|813"), (std::vector<int32_t>{ 815, 812, 813 }));
	EXPECT_EQ(PrerequisiteMissions("(509|229)&(812|813)"), (std::vector<int32_t>{ 509, 229, 812, 813 }));
	EXPECT_EQ(PrerequisiteMissions("1296, (1684 | 1680)"), (std::vector<int32_t>{ 1296, 1684, 1680 }));
	// ":2" is a mission state, not a mission
	EXPECT_EQ(PrerequisiteMissions("236:2"), (std::vector<int32_t>{ 236 }));
	EXPECT_TRUE(PrerequisiteMissions("").empty());
}

TEST(CDClientRulesTest, RarityChances) {
	using namespace CDClientRules;
	// RarityTable 2 as the server loads it (randmax descending): a roll takes the lowest row at or above it
	const std::vector<RarityRow> rows{ { 1.0, 4 }, { 0.95, 3 }, { 0.75, 2 }, { 0.5, 1 } };
	const auto chances = RarityChances(rows);
	ASSERT_EQ(chances.size(), 4u);
	EXPECT_EQ(chances[0].first, 4);
	EXPECT_NEAR(chances[0].second, 0.05, 1e-9);
	EXPECT_NEAR(chances[1].second, 0.2, 1e-9);
	EXPECT_NEAR(chances[2].second, 0.25, 1e-9);
	EXPECT_EQ(chances[3].first, 1);
	EXPECT_NEAR(chances[3].second, 0.5, 1e-9);

	// A roll above every row keeps rarity 1
	const auto partial = RarityChances({ { 0.9, 2 }, { 0.4, 1 } });
	ASSERT_EQ(partial.size(), 2u);
	EXPECT_EQ(partial[0].first, 1);
	EXPECT_NEAR(partial[0].second, 0.1 + 0.4, 1e-9);
	EXPECT_NEAR(partial[1].second, 0.5, 1e-9);
}

TEST(CDClientRulesTest, Candidates) {
	using namespace CDClientRules;
	// Items of the rolled rarity
	EXPECT_EQ(Candidates({ 1, 3, 4, 3 }, 3), (std::vector<size_t>{ 1, 3 }));
	// None of it: the next lower rarity
	EXPECT_EQ(Candidates({ 4, 3, 3, 1 }, 2), (std::vector<size_t>{ 3 }));
	// A lower rarity found alone doesn't stop the search: the server keeps taking lower ones until two match
	EXPECT_EQ(Candidates({ 3, 1, 1 }, 4), (std::vector<size_t>{ 0, 1, 2 }));
	EXPECT_TRUE(Candidates({ 4 }, 2).empty());
}

TEST(CDClientRulesTest, ChancesAndOdds) {
	using namespace CDClientRules;
	const std::vector<RarityRow> rows{ { 1.0, 4 }, { 0.95, 3 }, { 0.75, 2 }, { 0.5, 1 } };
	const auto perDrop = ChancePerDrop({ 1, 1, 2, 3, 4 }, rows);
	EXPECT_NEAR(perDrop[0], 0.25, 1e-9);
	EXPECT_NEAR(perDrop[2], 0.25, 1e-9);
	EXPECT_NEAR(perDrop[3], 0.2, 1e-9);
	EXPECT_NEAR(perDrop[4], 0.05, 1e-9);
	double total = 0;
	for (const auto chance : perDrop) total += chance;
	EXPECT_NEAR(total, 1.0, 1e-9);

	const auto once = Odds(0.5, 1, 1, 0.25);
	EXPECT_NEAR(once.atLeastOne, 0.125, 1e-9);
	EXPECT_NEAR(once.expected, 0.125, 1e-9);
	const auto twice = Odds(1.0, 2, 2, 0.5);
	EXPECT_NEAR(twice.atLeastOne, 0.75, 1e-9);
	EXPECT_NEAR(twice.expected, 1.0, 1e-9);
	// 1 to 3 drops: 1 - (0.5 + 0.25 + 0.125) / 3
	EXPECT_NEAR(Odds(1.0, 1, 3, 0.5).atLeastOne, 1.0 - 0.875 / 3.0, 1e-9);

	EXPECT_EQ(VendorStockChance(0, 3, 10), 1.0);
	EXPECT_NEAR(VendorStockChance(2, 4, 10), 0.3, 1e-9);
	EXPECT_EQ(VendorStockChance(5, 5, 2), 1.0);
}

TEST(CDClientRulesTest, Prerequisites) {
	using namespace CDClientRules;
	const auto terms = [](std::string_view text) {
		std::string out;
		for (const auto& term : ParsePrerequisites(text)) {
			out += std::to_string(term.mission) + (term.state ? ":" + std::to_string(term.state) : "") + (term.orRest ? " or " : " and ");
		}
		return out;
	};
	// Right to left, brackets don't group: 509 or (229 and (812 or 813))
	EXPECT_EQ(terms("(509|229)&(812|813)"), "509 or 229 and 812 or 813 and ");
	EXPECT_EQ(terms("1|2,3"), "1 or 2 and 3 and ");
	EXPECT_EQ(terms("1170:2"), "1170:2 and ");
	EXPECT_EQ(terms("1882:10 | 1882:2"), "1882:10 or 1882:2 and ");
	// Nothing: always met; a trailing "|" makes the whole thing always met, a trailing "," changes nothing
	EXPECT_EQ(terms(""), "0 and ");
	EXPECT_EQ(terms("5|"), "5 or 0 and ");
	EXPECT_EQ(terms("5,"), "5 and ");
}

TEST(CDClientRulesTest, TaskMeanings) {
	using namespace CDClientRules;
	EXPECT_EQ(MeaningOf(eMissionTaskType::SMASH).target, (std::vector<eLink>{ eLink::OBJECT }));
	EXPECT_TRUE(MeaningOf(eMissionTaskType::SMASH).targetGroupIds);
	EXPECT_EQ(MeaningOf(eMissionTaskType::META).target, (std::vector<eLink>{ eLink::MISSION }));
	EXPECT_FALSE(MeaningOf(eMissionTaskType::TALK_TO_NPC).targetGroupIds);
	EXPECT_EQ(MeaningOf(eMissionTaskType::USE_SKILL).parameters, (std::vector<eLink>{ eLink::SKILL }));
	EXPECT_TRUE(MeaningOf(eMissionTaskType::RACING).racingParameter);
	EXPECT_FALSE(MeaningOf(eMissionTaskType::BUY).progressed);

	// As MissionTask reads targetGroup: leading spaces are fine, anything else that isn't a number is skipped
	EXPECT_EQ(NumberList("1,2, 3,-1,x, 4 "), (std::vector<int64_t>{ 1, 2, 3 }));
	EXPECT_TRUE(NumberList("").empty());
}
