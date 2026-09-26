#include <gtest/gtest.h>

#include <algorithm>

#include "BackupFiles.h"
#include "GameLabels.h"
#include "LoginThrottle.h"
#include "PlayerAction.h"
#include "ServerState.h"

// ---- Failed sign-in throttling (DashboardAuthService) ----

TEST(LoginThrottleTests, BlocksOnlyTheGuessingAddressForThatAccount) {
	LoginThrottle throttle(3, 900, 900);
	EXPECT_FALSE(throttle.RecordFailure("1.1.1.1", 7, 100));
	EXPECT_FALSE(throttle.RecordFailure("1.1.1.1", 7, 101));
	EXPECT_TRUE(throttle.RecordFailure("1.1.1.1", 7, 102));
	EXPECT_GT(throttle.BlockedFor("1.1.1.1", 7, 103), 0);
	// The owner, from their own address, is not locked out by a stranger's guesses
	EXPECT_EQ(throttle.BlockedFor("2.2.2.2", 7, 103), 0);
	// Nor is the stranger blocked from other accounts
	EXPECT_EQ(throttle.BlockedFor("1.1.1.1", 8, 103), 0);
}

TEST(LoginThrottleTests, BlockEndsAndOldFailuresExpire) {
	LoginThrottle throttle(3, 60, 300);
	throttle.RecordFailure("a", 1, 0);
	throttle.RecordFailure("a", 1, 10);
	// The first two are outside the 60 s window by now, so this is only the first in the window
	EXPECT_FALSE(throttle.RecordFailure("a", 1, 200));
	throttle.RecordFailure("a", 1, 201);
	EXPECT_TRUE(throttle.RecordFailure("a", 1, 202));
	EXPECT_EQ(throttle.BlockedFor("a", 1, 202), 300);
	EXPECT_EQ(throttle.BlockedFor("a", 1, 502), 0); // time-limited, never permanent
}

TEST(LoginThrottleTests, SuccessAndUnlockForget) {
	LoginThrottle throttle(2, 900, 900);
	throttle.RecordFailure("a", 1, 0);
	throttle.Clear("a", 1);
	EXPECT_FALSE(throttle.RecordFailure("a", 1, 1));
	EXPECT_TRUE(throttle.RecordFailure("a", 1, 2));
	throttle.RecordFailure("b", 1, 2);
	throttle.ClearAccount(1);
	EXPECT_EQ(throttle.BlockedFor("a", 1, 3), 0);
	EXPECT_EQ(throttle.Size(), 0u);
}

// ---- What players see of running worlds (/api/status, dashboard_update) ----

TEST(ServerStatePlayerSafeTests, PropertiesAreMergedAndPrivateDetailsDropped) {
	nlohmann::json state{ {"worlds", nlohmann::json::array({
		{ {"mapID", 1100}, {"instanceID", 1}, {"cloneID", 0}, {"players", 4}, {"isPrivate", false}, {"zoneName", "Avant Gardens"} },
		{ {"mapID", 1150}, {"instanceID", 2}, {"cloneID", 12345}, {"players", 1}, {"isPrivate", true}, {"zoneName", "Block Yard"} },
		{ {"mapID", 1150}, {"instanceID", 3}, {"cloneID", 67890}, {"players", 2}, {"isPrivate", false}, {"zoneName", "Block Yard"} },
	}) }, {"stats", { {"onlinePlayers", 7}, {"worlds", 3} }} };

	const auto safe = ServerState::PlayerSafe(state);
	ASSERT_EQ(safe["worlds"].size(), 2u);
	for (const auto& world : safe["worlds"]) {
		EXPECT_FALSE(world.contains("cloneID"));
		EXPECT_FALSE(world.contains("isPrivate"));
	}
	EXPECT_EQ(safe["worlds"][0]["players"], 4);
	EXPECT_EQ(safe["worlds"][1]["mapID"], 1150);
	EXPECT_EQ(safe["worlds"][1]["players"], 3);
	EXPECT_EQ(safe["worlds"][1]["properties"], 2);
	EXPECT_EQ(safe["worlds"][1]["instanceID"], 0);
	// Totals are unchanged
	EXPECT_EQ(safe["stats"]["worlds"], 3);
	EXPECT_EQ(safe["stats"]["onlinePlayers"], 7);
}

// ---- Player action text is cut on a character boundary ----

TEST(PlayerActionTextTests, TruncationKeepsUtf8Whole) {
	EXPECT_EQ(PlayerActionRequest::Utf8Length("abc", 10), 3u);
	// "é" is 2 bytes; cutting at 2 would split it
	const std::string text = "a\xC3\xA9" "b";
	EXPECT_EQ(PlayerActionRequest::Utf8Length(text, 2), 1u);
	EXPECT_EQ(PlayerActionRequest::Utf8Length(text, 3), 3u);
	// A 4-byte emoji at the limit is dropped whole
	const std::string emoji = std::string(PlayerActionRequest::MAX_TEXT - 2, 'x') + "\xF0\x9F\x98\x80";
	EXPECT_EQ(PlayerActionRequest::Utf8Length(emoji, PlayerActionRequest::MAX_TEXT), PlayerActionRequest::MAX_TEXT - 2u);

	PlayerActionRequest request;
	request.text = emoji;
	RakNet::BitStream stream;
	request.Serialize(stream);
	PlayerActionRequest read;
	ASSERT_TRUE(read.Deserialize(stream));
	EXPECT_EQ(read.text, std::string(PlayerActionRequest::MAX_TEXT - 2, 'x'));
}

// ---- Labels the pages used to keep their own copies of ----

TEST(GameLabelsTests, TransferFlagAndTaskNamesComeFromTheEnums) {
	const auto& labels = GameLabels::Json();
	const auto name = [&](const char* kind, int value) {
		for (const auto& entry : labels[kind]) if (entry["value"] == value) return entry["name"].get<std::string>();
		return std::string();
	};
	EXPECT_EQ(name("transferMethods", 1), "Trade");
	EXPECT_EQ(name("transferMethods", 3), "Mail Claimed");
	EXPECT_EQ(name("flagKinds", 3), "Duplicate");
	EXPECT_EQ(name("flagStatus", 0), "Open");
	EXPECT_EQ(name("taskStatus", 3), "Timed Out");
}

// ---- mysqldump runs without a shell ----

TEST(BackupFilesTests, MysqldumpArgumentsAreAVector) {
	const auto arguments = BackupFiles::MysqldumpArguments({ "/usr/bin/mysqldump", "/tmp/opt", "db.local:3307", "dlu$(touch x)", "/b/dlu.sql.partial", "/tmp/err" });
	ASSERT_FALSE(arguments.empty());
	EXPECT_EQ(arguments.front(), "/usr/bin/mysqldump");
	EXPECT_EQ(arguments[1], "--defaults-extra-file=/tmp/opt"); // must be first
	EXPECT_NE(std::find(arguments.begin(), arguments.end(), "--host=db.local"), arguments.end());
	EXPECT_NE(std::find(arguments.begin(), arguments.end(), "--port=3307"), arguments.end());
	EXPECT_NE(std::find(arguments.begin(), arguments.end(), "--log-error=/tmp/err"), arguments.end());
	EXPECT_NE(std::find(arguments.begin(), arguments.end(), "--result-file=/b/dlu.sql.partial"), arguments.end());
	// Passed as one literal argument, never interpreted
	EXPECT_EQ(arguments.back(), "dlu$(touch x)");
}

// mysql_host in every form the servers connect with
TEST(BackupFilesTests, MysqldumpArgumentsReadEveryHostForm) {
	const auto has = [](const std::vector<std::string>& arguments, const std::string& argument) {
		return std::find(arguments.begin(), arguments.end(), argument) != arguments.end();
	};
	const auto dump = [](const std::string& host) { return BackupFiles::MysqldumpArguments({ "mysqldump", "/tmp/opt", host, "dlu", "/b/x", "" }); };

	auto arguments = dump("tcp://10.0.0.5:3307");
	EXPECT_TRUE(has(arguments, "--host=10.0.0.5"));
	EXPECT_TRUE(has(arguments, "--port=3307"));

	arguments = dump("tcp://db.local:3306/dlu");
	EXPECT_TRUE(has(arguments, "--host=db.local"));
	EXPECT_TRUE(has(arguments, "--port=3306"));

	arguments = dump("db.local");
	EXPECT_TRUE(has(arguments, "--host=db.local"));
	EXPECT_FALSE(std::any_of(arguments.begin(), arguments.end(), [](const std::string& a) { return a.starts_with("--port="); }));

	arguments = dump("unix:///run/mysqld/mysqld.sock");
	EXPECT_TRUE(has(arguments, "--socket=/run/mysqld/mysqld.sock"));
	EXPECT_FALSE(std::any_of(arguments.begin(), arguments.end(), [](const std::string& a) { return a.starts_with("--host=") || a.starts_with("--port="); }));

	arguments = dump("pipe://MySQL");
	EXPECT_TRUE(has(arguments, "--protocol=PIPE"));
	EXPECT_TRUE(has(arguments, "--socket=MySQL"));
	EXPECT_EQ(arguments.back(), "dlu");
}
