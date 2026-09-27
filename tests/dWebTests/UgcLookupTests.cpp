#include <gtest/gtest.h>

#include <set>

#include <sqlite3.h>

#include "UgcLookup.h"
#include "UgcLookupSql.h"

using namespace UgcLookup;
using eField = UgcSearch::eField;

TEST(UgcLookupTests, ParsesSearches) {
	auto s = ParseQuery("  1152921504606954530 ");
	EXPECT_EQ(s.field, eField::ANY);
	EXPECT_EQ(s.number, 1152921504606954530);
	EXPECT_EQ(s.text, "1152921504606954530"); // a name may be all digits

	s = ParseQuery("Bob");
	EXPECT_EQ(s.field, eField::ANY);
	EXPECT_FALSE(s.number);
	EXPECT_EQ(s.text, "Bob");

	s = ParseQuery("Owner: Bob");
	EXPECT_EQ(s.field, eField::OWNER);
	EXPECT_EQ(s.text, "Bob");

	s = ParseQuery("lot:8129");
	EXPECT_EQ(s.field, eField::LOT);
	EXPECT_EQ(s.number, 8129);
	EXPECT_TRUE(s.text.empty());

	// An id field with no number is searched as text; an unknown field is part of the text
	s = ParseQuery("id: abc");
	EXPECT_EQ(s.field, eField::ANY);
	EXPECT_EQ(s.text, "abc");
	s = ParseQuery("my: house");
	EXPECT_EQ(s.field, eField::ANY);
	EXPECT_EQ(s.text, "my: house");

	EXPECT_FALSE(ParseQuery("-5").number);
	EXPECT_TRUE(ParseQuery("   ").text.empty());
}

TEST(UgcLookupTests, LinksToTheViewer) {
	EXPECT_EQ(ViewerLink(eUgcKind::MODEL, 42), "/ugc?item=42&kind=model");
	EXPECT_EQ(ViewerLink(eUgcKind::MODULAR, 7), "/ugc?item=7&kind=modular");
	EXPECT_EQ(ParseKind("modular"), eUgcKind::MODULAR);
	EXPECT_FALSE(ParseKind("other"));
}

TEST(UgcLookupTests, FindsCreationsInAnInventory) {
	const std::string xml =
		"<obj><inv><items>"
		"<in t=\"5\">"
		"<i l=\"6662\" id=\"100\" s=\"0\" c=\"1\" sk=\"555\"><x bp=\"9:1000\" ui=\"9:555\"/></i>" // a model: its blueprint, its subkey is its UGID
		"<i l=\"8092\" id=\"101\" s=\"1\" c=\"1\" sk=\"2000\"><x ma=\"0:1:8129+1:8130\"/></i>"     // a rocket: its subkey
		"<i l=\"6416\" id=\"102\" s=\"2\" c=\"1\" sk=\"0\"><x ma=\"0:1:8337\"/></i>"               // an old car without a subkey
		"</in>"
		"<in t=\"0\"><i l=\"3254\" id=\"103\" s=\"0\" c=\"1\" sk=\"3000\"/><i l=\"1727\" id=\"104\" s=\"1\" c=\"5\"/></in>" // a pet; a plain item
		"</items></inv></obj>";
	const auto refs = InventoryRefs(xml);
	ASSERT_EQ(refs.size(), 3u);
	EXPECT_EQ(refs[0].blueprint, 1000);
	EXPECT_EQ(refs[0].inventory, 5u);
	EXPECT_EQ(refs[1].subkey, 2000);

	const auto candidates = Candidates(refs);
	EXPECT_EQ(std::set<LWOOBJID>(candidates.begin(), candidates.end()), (std::set<LWOOBJID>{ 555, 1000, 2000, 3000 }));

	// The pet's subkey (3000) is no creation; the model goes by its blueprint, not its UGID
	const auto links = LinkItems(refs, { 1000, 555 }, { 2000 });
	ASSERT_EQ(links.size(), 2u);
	EXPECT_EQ(links[0].item.itemId, 100);
	EXPECT_EQ(links[0].kind, eUgcKind::MODEL);
	EXPECT_EQ(links[0].ugcId, 1000);
	EXPECT_EQ(links[1].item.itemId, 101);
	EXPECT_EQ(links[1].kind, eUgcKind::MODULAR);
	EXPECT_EQ(links[1].ugcId, 2000);

	EXPECT_TRUE(InventoryRefs("").empty());
	EXPECT_TRUE(InventoryRefs("<obj><inv>").empty());
}

TEST(UgcLookupTests, FindsCreationsInTheMail) {
	IUgcLookup::UgcMail model{ .id = 1, .lot = MODEL_ITEM_LOT, .subkey = 555, .config = "blueprintid=9:1000\nuserModelID=9:555" };
	EXPECT_EQ(MailCreation(model, { 1000 }, {}), std::make_pair(eUgcKind::MODEL, LWOOBJID{ 1000 }));
	EXPECT_FALSE(MailCreation(model, { 999 }, {}));

	IUgcLookup::UgcMail rocket{ .id = 2, .lot = 8092, .subkey = 2000, .config = "assemblyPartLOTs=0:1:8129" };
	EXPECT_EQ(MailCreation(rocket, {}, { 2000 }), std::make_pair(eUgcKind::MODULAR, LWOOBJID{ 2000 }));
	EXPECT_FALSE(MailCreation(rocket, { 2000 }, {}));
}

namespace {
	// The lookup's SQL against a small database with the game's tables (the columns it reads)
	class UgcLookupSqlTests : public ::testing::Test {
	protected:
		sqlite3* db = nullptr;

		void SetUp() override {
			ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
			Exec("CREATE TABLE accounts (id INTEGER PRIMARY KEY, name TEXT);"
				"CREATE TABLE charinfo (id BIGINT PRIMARY KEY, account_id INTEGER, name TEXT);"
				"CREATE TABLE properties (id BIGINT PRIMARY KEY, owner_id BIGINT, name TEXT, zone_id INTEGER);"
				"CREATE TABLE ugc (id INTEGER PRIMARY KEY, account_id INTEGER, character_id BIGINT, is_optimized INTEGER, filename TEXT, process_error TEXT DEFAULT '');"
				"CREATE TABLE ugc_modular_build (ugc_id BIGINT PRIMARY KEY, character_id BIGINT, ldf_config TEXT, is_optimized INTEGER DEFAULT 0, process_error TEXT DEFAULT '');"
				"CREATE TABLE properties_contents (id BIGINT PRIMARY KEY, property_id BIGINT, ugc_id BIGINT, lot INTEGER, model_name TEXT DEFAULT '', model_description TEXT DEFAULT '');"
				"INSERT INTO accounts VALUES (1, 'builder'), (2, 'racer');"
				"INSERT INTO charinfo VALUES (10, 1, 'Bricky'), (20, 2, 'Speedy');"
				"INSERT INTO properties VALUES (500, 10, 'Castle Hill', 1150);"
				"INSERT INTO ugc VALUES (1000, 1, 10, 1, 'tower.lxfml', ''), (1001, 1, 10, 0, 'boat.lxfml', '');"
				"INSERT INTO ugc_modular_build VALUES (2000, 20, '1:8129+1:8130+1:9330', 1, '');"
				"INSERT INTO properties_contents VALUES (7000, 500, 1000, 14, 'Big tower', 'the tallest');");
		}

		void TearDown() override { sqlite3_close(db); }

		void Exec(const char* sql) { ASSERT_EQ(sqlite3_exec(db, sql, nullptr, nullptr, nullptr), SQLITE_OK) << sqlite3_errmsg(db); }

		// The ids a search finds, of models and modular builds
		std::set<int64_t> Find(const std::string& query, bool modular) {
			const auto search = ParseQuery(query);
			const auto sql = UgcLookupSql::Select(modular) + UgcLookupSql::Where(search, modular) + "LIMIT 50;";
			sqlite3_stmt* stmt = nullptr;
			EXPECT_EQ(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr), SQLITE_OK) << sqlite3_errmsg(db) << "\n" << sql;
			EXPECT_EQ(sqlite3_bind_parameter_count(stmt), UgcLookupSql::TEXT_BINDS);
			const auto pattern = "%" + search.text + "%";
			for (int i = 1; i <= UgcLookupSql::TEXT_BINDS; i++) sqlite3_bind_text(stmt, i, pattern.c_str(), -1, SQLITE_TRANSIENT);
			std::set<int64_t> ids;
			while (sqlite3_step(stmt) == SQLITE_ROW) ids.insert(sqlite3_column_int64(stmt, 0));
			sqlite3_finalize(stmt);
			return ids;
		}
	};
}

TEST_F(UgcLookupSqlTests, FindsByIds) {
	EXPECT_EQ(Find("1000", false), (std::set<int64_t>{ 1000 }));        // the blueprint id
	EXPECT_EQ(Find("7000", false), (std::set<int64_t>{ 1000 }));        // the placed model's object id
	EXPECT_EQ(Find("500", false), (std::set<int64_t>{ 1000 }));         // the property
	EXPECT_EQ(Find("10", false), (std::set<int64_t>{ 1000, 1001 }));    // the creator
	EXPECT_EQ(Find("id: 2", true), (std::set<int64_t>{ 2000 }));        // the creator's account
	EXPECT_EQ(Find("2000", true), (std::set<int64_t>{ 2000 }));
	EXPECT_TRUE(Find("2000", false).empty());
}

TEST_F(UgcLookupSqlTests, FindsByNames) {
	EXPECT_EQ(Find("brick", false), (std::set<int64_t>{ 1000, 1001 }));   // the character's name
	EXPECT_EQ(Find("owner: racer", true), (std::set<int64_t>{ 2000 }));   // the account's name
	EXPECT_TRUE(Find("owner: racer", false).empty());
	EXPECT_EQ(Find("castle", false), (std::set<int64_t>{ 1000 }));        // a property it is placed on
	EXPECT_EQ(Find("property: 500", false), (std::set<int64_t>{ 1000 }));
	EXPECT_EQ(Find("tallest", false), (std::set<int64_t>{ 1000 }));       // the description given to it
	EXPECT_EQ(Find("model: boat", false), (std::set<int64_t>{ 1001 }));   // the upload's file name
	EXPECT_TRUE(Find("property: tower", false).empty());                  // a model name is not a property name
}

TEST_F(UgcLookupSqlTests, FindsByLot) {
	EXPECT_EQ(Find("lot: 8130", true), (std::set<int64_t>{ 2000 }));      // a module
	EXPECT_EQ(Find("lot: 9330", true), (std::set<int64_t>{ 2000 }));      // the last module
	EXPECT_TRUE(Find("lot: 813", true).empty());                          // not part of a module's LOT
	EXPECT_EQ(Find("lot: 14", false), (std::set<int64_t>{ 1000 }));       // placed as LOT 14
}
