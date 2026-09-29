#include <gtest/gtest.h>

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
}

TEST(CDClientSchemaTest, ComponentTables) {
	const auto schema = TestSchema();
	const auto name = [&](eReplicaComponentType type) {
		const auto* table = ComponentTable(schema, type);
		return table ? table->name : std::string("-");
	};
	EXPECT_EQ(name(eReplicaComponentType::VENDOR), "VendorComponent");
	EXPECT_EQ(name(eReplicaComponentType::RENDER), "RenderComponent");
	EXPECT_EQ(name(eReplicaComponentType::DESTROYABLE), "DestructibleComponent");
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
