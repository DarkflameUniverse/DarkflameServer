#include <gtest/gtest.h>
#include "RouteUtils.h"

using namespace RouteUtils;

TEST(RouteUtilsTest, PathSegmentSplitsOnSlashes) {
	EXPECT_EQ(PathSegment("/api/accounts/42/ban", 0), "api");
	EXPECT_EQ(PathSegment("/api/accounts/42/ban", 2), "42");
	EXPECT_EQ(PathSegment("/api/accounts/42/ban", 3), "ban");
	EXPECT_EQ(PathSegment("/api/accounts/42/ban", 4), "");
	EXPECT_EQ(PathSegment("//accounts//7", 1), "7");
}

TEST(RouteUtilsTest, PathIdRejectsNonNumericSegments) {
	EXPECT_EQ(PathId<uint32_t>("/api/accounts/42/ban", 2), 42u);
	EXPECT_FALSE(PathId<uint32_t>("/api/accounts/42abc/ban", 2).has_value());
	EXPECT_FALSE(PathId<uint32_t>("/api/accounts/-1/ban", 2).has_value());
	EXPECT_FALSE(PathId<uint32_t>("/api/accounts//ban", 2).has_value());
	EXPECT_FALSE(PathId<int32_t>("/api/icon/1\";rm -rf ~;\"", 2).has_value());
	EXPECT_EQ(PathId<int64_t>("/characters/1152921510794154770", 1), 1152921510794154770LL);
}

TEST(RouteUtilsTest, ParseDataTablesRequestDefaultsAndClamps) {
	auto request = ParseDataTablesRequest("");
	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->start, 0u);
	EXPECT_EQ(request->length, 10u);
	EXPECT_TRUE(request->orderAsc);

	request = ParseDataTablesRequest(R"({"draw":3,"start":50,"length":100000,"search":{"value":"bob"},"order":[{"column":2,"dir":"desc"}]})");
	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->draw, 3u);
	EXPECT_EQ(request->start, 50u);
	EXPECT_EQ(request->length, MAX_PAGE_LENGTH);
	EXPECT_EQ(request->search, "bob");
	EXPECT_EQ(request->orderColumn, 2u);
	EXPECT_FALSE(request->orderAsc);
}

TEST(RouteUtilsTest, ParseDataTablesRequestHandlesBadInput) {
	EXPECT_FALSE(ParseDataTablesRequest("not json").has_value());
	EXPECT_FALSE(ParseDataTablesRequest("[1,2]").has_value());

	// Wrong types fall back to defaults instead of throwing
	auto request = ParseDataTablesRequest(R"({"start":-5,"length":"lots","search":42,"order":[{"column":"x"}]})");
	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->start, 0u);
	EXPECT_EQ(request->length, 10u);
	EXPECT_EQ(request->search, "");
	EXPECT_EQ(request->orderColumn, 0u);
}

TEST(RouteUtilsTest, EscapeHtmlEscapesSpecialCharacters) {
	EXPECT_EQ(EscapeHtml("<script>alert('x')</script>"), "&lt;script&gt;alert(&#39;x&#39;)&lt;/script&gt;");
	EXPECT_EQ(EscapeHtml("a & \"b\""), "a &amp; &quot;b&quot;");
	EXPECT_EQ(EscapeHtml("plain"), "plain");
}

TEST(RouteUtilsTest, EscapeHtmlStringsWalksNestedJson) {
	nlohmann::json data = {
		{"name", "<b>"},
		{"level", 9},
		{"characters", {{{"name", "x'); alert(1)//"}}}}
	};
	EscapeHtmlStrings(data);
	EXPECT_EQ(data["name"], "&lt;b&gt;");
	EXPECT_EQ(data["level"], 9);
	EXPECT_EQ(data["characters"][0]["name"], "x&#39;); alert(1)//");
}

TEST(RouteUtilsTest, CanManageAccountEnforcesHierarchy) {
	// Lower level targets are fine, whatever the permissions
	EXPECT_TRUE(CanManageAccount(4, 1, 0, 2, false, false));
	EXPECT_TRUE(CanManageAccount(8, 1, 7, 2, false, false));
	// The same level needs manage_equal_rank
	EXPECT_EQ(ManageDenial(4, 1, 4, 2, false, false), eManageDenial::EQUAL_RANK);
	EXPECT_TRUE(CanManageAccount(4, 1, 4, 2, false, true));
	// A higher level never, whatever the permissions say
	EXPECT_EQ(ManageDenial(4, 1, 9, 2, true, true), eManageDenial::HIGHER_RANK);
	EXPECT_EQ(ManageDenial(8, 1, 9, 2, true, true), eManageDenial::HIGHER_RANK);
	EXPECT_EQ(ManageDenial(4, 1, 5, 2, true, true), eManageDenial::HIGHER_RANK);
}

TEST(RouteUtilsTest, OperatorsMayActOnAnyoneIncludingThemselves) {
	// GM 9: other GM 9s and their own account, with no permission needed
	EXPECT_TRUE(CanManageAccount(9, 1, 9, 2, false, false));
	EXPECT_TRUE(CanManageAccount(9, 5, 9, 5, false, false));
	EXPECT_TRUE(CanManageAccount(9, 5, 0, 5, false, false));
}

TEST(RouteUtilsTest, OwnAccountNeedsTheSelfPermission) {
	EXPECT_EQ(ManageDenial(4, 5, 4, 5, false, false), eManageDenial::SELF);
	EXPECT_TRUE(CanManageAccount(4, 5, 4, 5, true, false));
	// Being allowed on yourself is not being allowed on your peers, and the other way round
	EXPECT_FALSE(CanManageAccount(4, 5, 4, 6, true, false));
	EXPECT_EQ(ManageDenial(4, 5, 4, 5, false, true), eManageDenial::SELF);
	// Account 0 (no account) is never "yourself"
	EXPECT_TRUE(CanManageAccount(4, 0, 0, 0, false, false));
}

TEST(RouteUtilsTest, SelfPermissionFollowsTheKindOfAction) {
	EXPECT_STREQ(SelfPermission(eAccountAction::TOOLS), "self_tools");
	EXPECT_STREQ(SelfPermission(eAccountAction::ITEMS), "self_items");
	EXPECT_STREQ(SelfPermission(eAccountAction::MODERATION), "self_moderation");
}

TEST(RouteUtilsTest, LastOperatorIsKept) {
	// The last GM 9 that can sign in can't be demoted, banned, locked or deleted, even by themselves
	EXPECT_TRUE(RemovesLastOperator(9, 0));
	EXPECT_FALSE(RemovesLastOperator(9, 1));
	// Anyone below GM 9 isn't covered by the rail
	EXPECT_FALSE(RemovesLastOperator(8, 0));
}

TEST(RouteUtilsTest, CanGrantGmLevelNeverExceedsOwnLevel) {
	EXPECT_TRUE(CanGrantGmLevel(8, 0));
	EXPECT_TRUE(CanGrantGmLevel(8, 7));
	EXPECT_FALSE(CanGrantGmLevel(8, 8));
	EXPECT_FALSE(CanGrantGmLevel(8, 9));
	EXPECT_TRUE(CanGrantGmLevel(9, 9));
	EXPECT_FALSE(CanGrantGmLevel(9, 10));
}

TEST(RouteUtilsTest, RateLimiterAllowsUpToLimitPerWindow) {
	RateLimiter limiter(3, std::chrono::seconds(60));
	const auto t0 = RateLimiter::Clock::now();
	EXPECT_TRUE(limiter.Allow("1.2.3.4", t0));
	EXPECT_TRUE(limiter.Allow("1.2.3.4", t0));
	EXPECT_TRUE(limiter.Allow("1.2.3.4", t0));
	EXPECT_FALSE(limiter.Allow("1.2.3.4", t0 + std::chrono::seconds(30)));
	// Other clients are unaffected
	EXPECT_TRUE(limiter.Allow("5.6.7.8", t0));
	// Old attempts fall out of the window
	EXPECT_TRUE(limiter.Allow("1.2.3.4", t0 + std::chrono::seconds(61)));
}

TEST(RouteUtilsTest, ValidateUsername) {
	EXPECT_TRUE(ValidateUsername("ab").has_value());
	EXPECT_FALSE(ValidateUsername("valid_name-1.x").has_value());
	EXPECT_TRUE(ValidateUsername("has space").has_value());
	EXPECT_TRUE(ValidateUsername("x\",\"gmLevel\":9").has_value());
	EXPECT_TRUE(ValidateUsername(std::string(33, 'a')).has_value());
}

TEST(RouteUtilsTest, ValidatePassword) {
	EXPECT_TRUE(ValidatePassword("12345").has_value());
	EXPECT_FALSE(ValidatePassword("123456").has_value());
	EXPECT_TRUE(ValidatePassword(std::string(41, 'a')).has_value());
}

TEST(CsvTests, CellsAreQuotedAndFormulasNeutralized) {
	using RouteUtils::CsvCell;
	EXPECT_EQ(CsvCell("plain"), "plain");
	EXPECT_EQ(CsvCell("a,b"), "\"a,b\"");
	EXPECT_EQ(CsvCell("say \"hi\""), "\"say \"\"hi\"\"\"");
	EXPECT_EQ(CsvCell("two\nlines"), "\"two\nlines\"");
	EXPECT_EQ(CsvCell("=HYPERLINK(\"x\")"), "\"'=HYPERLINK(\"\"x\"\")\"");
	EXPECT_EQ(CsvCell("+1"), "'+1");
	EXPECT_EQ(CsvCell("@SUM(A1)"), "'@SUM(A1)");
	EXPECT_EQ(CsvCell("-42"), "-42");      // negative numbers stay numbers
	EXPECT_EQ(CsvCell("-1+2"), "'-1+2");
	EXPECT_EQ(CsvCell(""), "");
}

TEST(CsvTests, RowsFollowColumns) {
	const nlohmann::json rows = nlohmann::json::array({
		{ {"name", "Bob"}, {"count", 5}, {"ok", true} },
		{ {"name", "=cmd"}, {"count", nullptr} }
	});
	EXPECT_EQ(RouteUtils::ToCsv(rows, { {"name", "Name"}, {"count", "Count"}, {"ok", "OK"} }),
		"Name,Count,OK\r\nBob,5,true\r\n'=cmd,,\r\n");
}
