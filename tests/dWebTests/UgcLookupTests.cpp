#include <gtest/gtest.h>

#include <set>

#include <sqlite3.h>

#include "UgcAssemblies.h"
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
				"CREATE TABLE ugc (id INTEGER PRIMARY KEY, account_id INTEGER, character_id BIGINT, is_optimized INTEGER, filename TEXT, process_error TEXT DEFAULT '', "
				"process_attempts INTEGER DEFAULT 0, processed_at BIGINT DEFAULT 0, process_after BIGINT DEFAULT 0, bake_ao INTEGER DEFAULT 0, brick_count INTEGER DEFAULT 0, triangle_count INTEGER DEFAULT 0, process_ms INTEGER DEFAULT 0, process_cpu_ms INTEGER DEFAULT 0, process_memory_kb INTEGER DEFAULT 0, triangle_count_before INTEGER DEFAULT 0);"
				"CREATE TABLE ugc_modular_build (ugc_id BIGINT PRIMARY KEY, character_id BIGINT, ldf_config TEXT, is_optimized INTEGER DEFAULT 0, process_error TEXT DEFAULT '', "
				"process_attempts INTEGER DEFAULT 0, processed_at BIGINT DEFAULT 0, process_ms INTEGER DEFAULT 0, process_cpu_ms INTEGER DEFAULT 0, process_memory_kb INTEGER DEFAULT 0);"
				"CREATE TABLE properties_contents (id BIGINT PRIMARY KEY, property_id BIGINT, ugc_id BIGINT, lot INTEGER, model_name TEXT DEFAULT '', model_description TEXT DEFAULT '');"
				"INSERT INTO accounts VALUES (1, 'builder'), (2, 'racer');"
				"INSERT INTO charinfo VALUES (10, 1, 'Bricky'), (20, 2, 'Speedy');"
				"INSERT INTO properties VALUES (500, 10, 'Castle Hill', 1150);"
				"INSERT INTO ugc (id, account_id, character_id, is_optimized, filename, process_error, brick_count, triangle_count, process_ms, triangle_count_before) VALUES "
				"(1000, 1, 10, 1, 'tower.lxfml', '', 40, 900, 5200, 1500), (1001, 1, 10, 0, 'boat.lxfml', '', 0, 0, 0, 0);"
				"INSERT INTO ugc_modular_build (ugc_id, character_id, ldf_config, is_optimized, process_error) VALUES (2000, 20, '1:8129+1:8130+1:9330', 1, '');"
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

		// A list page's ids in order, and the count, as ListUgc runs them (all, or a search)
		std::pair<std::vector<int64_t>, int64_t> List(const IUgcLookup::UgcListQuery& query, bool modular) {
			const bool searching = UgcLookupSql::Searching(query.search);
			const auto where = (searching ? UgcLookupSql::Where(query.search, modular) : std::string("WHERE 1=1 ")) + UgcLookupSql::ListFilter(query, modular);
			const auto pattern = "%" + query.search.text + "%";
			const auto run = [&](const std::string& sql, bool page) {
				std::vector<int64_t> out;
				sqlite3_stmt* stmt = nullptr;
				EXPECT_EQ(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr), SQLITE_OK) << sqlite3_errmsg(db) << "\n" << sql;
				int i = 1;
				if (searching) for (; i <= UgcLookupSql::TEXT_BINDS; i++) sqlite3_bind_text(stmt, i, pattern.c_str(), -1, SQLITE_TRANSIENT);
				if (page) {
					sqlite3_bind_int(stmt, i++, static_cast<int>(query.limit));
					sqlite3_bind_int(stmt, i++, static_cast<int>(query.offset));
				}
				while (sqlite3_step(stmt) == SQLITE_ROW) out.push_back(sqlite3_column_int64(stmt, 0));
				sqlite3_finalize(stmt);
				return out;
			};
			const auto ids = run(UgcLookupSql::Select(modular) + where + UgcLookupSql::ListOrder(query, modular) + "LIMIT ? OFFSET ?;", true);
			const auto count = run("SELECT COUNT(*) " + UgcLookupSql::From(modular) + where + ";", false);
			return { ids, count.empty() ? -1 : count.front() };
		}
	};
}

TEST_F(UgcLookupSqlTests, ListsPagesWithCounts) {
	IUgcLookup::UgcListQuery query;
	auto [ids, total] = List(query, false);
	EXPECT_EQ(ids, (std::vector<int64_t>{ 1001, 1000 })); // newest first
	EXPECT_EQ(total, 2);
	query.sort = IUgcLookup::eSort::BRICKS;
	EXPECT_EQ(List(query, false).first, (std::vector<int64_t>{ 1000, 1001 }));
	query.reverse = true; // the fewest bricks first
	EXPECT_EQ(List(query, false).first, (std::vector<int64_t>{ 1001, 1000 }));
	query.reverse = false;
	query.sort = IUgcLookup::eSort::NAME; // "Big tower" (its placed name) before boat.lxfml, whatever the case
	EXPECT_EQ(List(query, false).first, (std::vector<int64_t>{ 1000, 1001 }));
	query.reverse = true;
	EXPECT_EQ(List(query, false).first, (std::vector<int64_t>{ 1001, 1000 }));
	query.reverse = false;
	query.sort = IUgcLookup::eSort::NEWEST;
	query.reverse = true; // the oldest first
	EXPECT_EQ(List(query, false).first, (std::vector<int64_t>{ 1000, 1001 }));
	query.reverse = false;
	query.sort = IUgcLookup::eSort::SAVINGS; // the tower lost 40% of its triangles; the boat isn't known
	EXPECT_EQ(List(query, false).first, (std::vector<int64_t>{ 1000, 1001 }));
	query.sort = IUgcLookup::eSort::CPU; // none recorded: ties go newest first
	EXPECT_EQ(List(query, false).first, (std::vector<int64_t>{ 1001, 1000 }));
	query.sort = IUgcLookup::eSort::MEMORY;
	EXPECT_EQ(List(query, false).first, (std::vector<int64_t>{ 1001, 1000 }));
	query.sort = IUgcLookup::eSort::MADE; // neither has been made: ties go newest first
	EXPECT_EQ(List(query, false).first, (std::vector<int64_t>{ 1001, 1000 }));
	query.sort = IUgcLookup::eSort::SLOWEST; // the tower took 5.2 s; the boat isn't made
	EXPECT_EQ(List(query, false).first, (std::vector<int64_t>{ 1000, 1001 }));
	query.reverse = true;
	EXPECT_EQ(List(query, false).first, (std::vector<int64_t>{ 1001, 1000 }));
	query.reverse = false;
	query.sort = IUgcLookup::eSort::NAME;
	query.limit = 1;
	query.offset = 1;
	std::tie(ids, total) = List(query, false);
	EXPECT_EQ(ids, (std::vector<int64_t>{ 1001 }));
	EXPECT_EQ(total, 2); // the count is of every match, not the page
	query = {};
	query.state = IUgc::eProcessState::DONE;
	EXPECT_EQ(List(query, false), (std::pair<std::vector<int64_t>, int64_t>{ { 1000 }, 1 }));
	query.search = ParseQuery("boat"); // searched and in a state: none
	EXPECT_EQ(List(query, false).second, 0);
	query.state.reset();
	EXPECT_EQ(List(query, false), (std::pair<std::vector<int64_t>, int64_t>{ { 1001 }, 1 }));
	EXPECT_EQ(List({}, true), (std::pair<std::vector<int64_t>, int64_t>{ { 2000 }, 1 }));
}

TEST(UgcAssemblies, GroupsBuildsByTheirModules) {
	using State = IUgc::eProcessState;
	const auto build = [](LWOOBJID id, LWOOBJID owner, const std::string& ldf, State state) {
		IUgcLookup::UgcEntry e;
		e.kind = IUgcLookup::eUgcKind::MODULAR;
		e.id = id;
		e.characterId = owner;
		e.detail = ldf;
		e.state = state;
		if (state == State::FAILED) e.error = "broken";
		return e;
	};
	const std::map<uint32_t, UgcAssemblies::ModuleInfo> modules = {
		{ 4713, { 3, "Classic Rocket Nose Cone" } }, { 4714, { 3, "Classic Rocket Cockpit" } }, { 4715, { 3, "Classic Rocket Engine" } },
		{ 8129, { 6, "Racing Car Chassis" } }, { 8130, { 6, "Racing Car Engine" } },
	};
	const std::vector<IUgcLookup::UgcEntry> builds = {
		build(10, 1, "1:4713+1:4714+1:4715", State::DONE),
		build(11, 2, "1:4715+1:4714+1:4713", State::PENDING), // the same modules written differently
		build(12, 2, "1:8129+1:8130", State::FAILED),
		build(13, 3, "", State::PENDING),                     // no modules: left out
		build(14, 1, "1:4713+1:4714+1:4715", State::FAILED),
	};
	auto list = UgcAssemblies::Group(builds, modules);
	ASSERT_EQ(list.size(), 2u);
	const auto rocket = std::find_if(list.begin(), list.end(), [](const auto& a) { return a.key == "4713-4714-4715"; });
	ASSERT_NE(rocket, list.end());
	EXPECT_EQ(rocket->builds, (std::vector<LWOOBJID>{ 14, 11, 10 }));
	EXPECT_EQ(rocket->owners.size(), 2u);
	EXPECT_EQ(rocket->buildType, 3);
	EXPECT_EQ(rocket->state, State::DONE);  // one build's icon is made: the combination's is
	EXPECT_EQ(rocket->iconBuild, 10);
	EXPECT_TRUE(rocket->error.empty());
	const auto car = std::find_if(list.begin(), list.end(), [](const auto& a) { return a.key == "8129-8130"; });
	EXPECT_EQ(car->state, State::FAILED);
	EXPECT_EQ(car->error, "broken");

	UgcAssemblies::Filter filter;
	filter.buildType = 6;
	EXPECT_FALSE(UgcAssemblies::Matches(*rocket, filter, modules));
	EXPECT_TRUE(UgcAssemblies::Matches(*car, filter, modules));
	filter = {};
	filter.moduleText = "cockpit";
	EXPECT_TRUE(UgcAssemblies::Matches(*rocket, filter, modules));
	EXPECT_FALSE(UgcAssemblies::Matches(*car, filter, modules));
	filter = {};
	filter.moduleLot = 8130;
	EXPECT_TRUE(UgcAssemblies::Matches(*car, filter, modules));
	filter = {};
	filter.builds = std::set<LWOOBJID>{ 11 }; // an owner's build
	EXPECT_TRUE(UgcAssemblies::Matches(*rocket, filter, modules));
	EXPECT_FALSE(UgcAssemblies::Matches(*car, filter, modules));
	filter = {};
	filter.state = State::FAILED;
	EXPECT_FALSE(UgcAssemblies::Matches(*rocket, filter, modules));

	UgcAssemblies::Sort(list, UgcAssemblies::eSort::REFERENCES, modules);
	EXPECT_EQ(list.front().key, "4713-4714-4715");
	UgcAssemblies::Sort(list, UgcAssemblies::eSort::NEWEST, modules);
	EXPECT_EQ(list.front().key, "4713-4714-4715"); // build 14
	UgcAssemblies::Sort(list, UgcAssemblies::eSort::NAME, modules);
	EXPECT_EQ(list.front().key, "4713-4714-4715"); // Classic before Racing
	UgcAssemblies::Sort(list, UgcAssemblies::eSort::OLDEST, modules);
	EXPECT_EQ(list.front().key, "4713-4714-4715"); // build 10
	UgcAssemblies::Sort(list, UgcAssemblies::eSort::REFERENCES, modules, true);
	EXPECT_EQ(list.back().key, "4713-4714-4715"); // the most builds last
	UgcAssemblies::Sort(list, UgcAssemblies::eSort::NAME, modules, true);
	EXPECT_EQ(list.back().key, "4713-4714-4715"); // Racing before Classic
	EXPECT_EQ(UgcAssemblies::ParseSort("uses"), UgcAssemblies::eSort::REFERENCES);
	EXPECT_FALSE(UgcAssemblies::ParseSort("nonsense"));
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
