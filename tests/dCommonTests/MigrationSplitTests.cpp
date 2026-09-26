#include <gtest/gtest.h>

#include "MigrationRunner.h"

using MigrationRunner::SplitStatements;

TEST(MigrationSplitTests, SplitsAtStatementEnds) {
	const auto statements = SplitStatements("CREATE TABLE a (x INT);\nINSERT INTO a VALUES (1);\n\n", false);
	ASSERT_EQ(statements.size(), 2u);
	EXPECT_EQ(statements[0], "CREATE TABLE a (x INT)");
	EXPECT_EQ(statements[1], "\nINSERT INTO a VALUES (1)");
}

TEST(MigrationSplitTests, IgnoresSemicolonsInComments) {
	const auto statements = SplitStatements(
		"/* Dashboard data; see the other file. */\nCREATE TABLE a (x INT); -- trailing; note\n-- whole line; comment\nCREATE TABLE b (y INT);", false);
	ASSERT_EQ(statements.size(), 2u);
	EXPECT_NE(statements[0].find("CREATE TABLE a"), std::string::npos);
	EXPECT_EQ(statements[0].find("Dashboard"), std::string::npos); // comments are dropped
	EXPECT_NE(statements[1].find("CREATE TABLE b"), std::string::npos);
}

TEST(MigrationSplitTests, IgnoresSemicolonsInQuotes) {
	const auto statements = SplitStatements("INSERT INTO t VALUES ('a;b', \"c;d\", `e;f`, 'it''s; fine');SELECT 1;", false);
	ASSERT_EQ(statements.size(), 2u);
	EXPECT_EQ(statements[0], "INSERT INTO t VALUES ('a;b', \"c;d\", `e;f`, 'it''s; fine')");
	// A -- or /* inside a string is text, not a comment
	EXPECT_EQ(SplitStatements("SELECT '--;/*';", false)[0], "SELECT '--;/*'");
}

TEST(MigrationSplitTests, BackslashesDependOnTheDatabase) {
	// SQLite: a backslash is an ordinary character, so the quote after it closes the string
	const auto sqlite = SplitStatements("INSERT INTO t VALUES ('scripts\\');SELECT 1;", false);
	EXPECT_EQ(sqlite.size(), 2u);
	// MySQL: \' is an escaped quote inside the string
	const auto mysql = SplitStatements("INSERT INTO t VALUES ('it\\'s; here');SELECT 1;", true);
	ASSERT_EQ(mysql.size(), 2u);
	EXPECT_EQ(mysql[0], "INSERT INTO t VALUES ('it\\'s; here')");
}

TEST(MigrationSplitTests, UnterminatedPartsDoNotLoop) {
	EXPECT_EQ(SplitStatements("SELECT 1; /* never closed", false).size(), 1u);
	EXPECT_EQ(SplitStatements("SELECT 'never closed", false).size(), 1u);
	EXPECT_TRUE(SplitStatements("", false).empty());
	EXPECT_TRUE(SplitStatements(" ;; \n", false).empty());
}
