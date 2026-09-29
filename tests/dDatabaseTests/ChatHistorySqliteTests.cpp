/**
 * The chat history and chat flag tables on SQLite, with no other database needed: a fresh SQLite file gets every
 * migration (MigrationRunner::RunMigrations, as the servers run them), then the IChatLog and IChatFlags methods are
 * checked against what was written. DatabaseParityTests compares the same methods with MySQL when one is set up.
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <unistd.h>

#include "Database.h"
#include "dConfig.h"
#include "Game.h"
#include "Logger.h"
#include "MigrationRunner.h"
#include "SQLiteDatabase.h"

namespace Game {
	Logger* logger = nullptr;
	dConfig* config = nullptr;
}

namespace {
	constexpr LWOOBJID ALICE = 1152921510000000001;
	constexpr LWOOBJID BOB = 1152921510000000002;
	constexpr LWOOBJID CAROL = 1152921510000000003;
	constexpr LWOOBJID TEAM = 1700000000000001;

	std::filesystem::path g_Path;
	std::filesystem::path g_Log;
	std::unique_ptr<SQLiteDatabase> g_Db;

	class SqliteEnvironment : public ::testing::Environment {
	public:
		void SetUp() override {
			const auto base = std::filesystem::temp_directory_path() / ("dDatabaseSqliteTests_" + std::to_string(getpid()));
			g_Path = base.string() + ".sqlite";
			g_Log = base.string() + ".log";
			for (const auto* suffix : { "", "-wal", "-shm" }) std::filesystem::remove(g_Path.string() + suffix);
			Game::logger = new Logger(g_Log.string(), false, false);
			setenv("DATABASE_TYPE", "sqlite", 1);
			setenv("SQLITE_DATABASE_PATH", g_Path.string().c_str(), 1);
			Game::config = new dConfig("dDatabaseSqliteTests.ini");
			Database::Connect();
			MigrationRunner::RunMigrations();
			Database::Destroy("dDatabaseSqliteTests migrations");
			g_Db = std::make_unique<SQLiteDatabase>();
			g_Db->Connect();
		}

		void TearDown() override {
			if (g_Db) g_Db->Destroy("dDatabaseSqliteTests");
			g_Db.reset();
			for (const auto* suffix : { "", "-wal", "-shm" }) std::filesystem::remove(g_Path.string() + suffix);
			std::filesystem::remove(g_Log);
			delete Game::config;
			Game::config = nullptr;
			delete Game::logger;
			Game::logger = nullptr;
		}
	};

	const auto* const g_Environment = ::testing::AddGlobalTestEnvironment(new SqliteEnvironment());

	IChatLog::ChatMessage Message(int64_t time, const std::string& channel, LWOOBJID sender, const std::string& senderName, uint32_t account, const std::string& text) {
		IChatLog::ChatMessage m;
		m.time = time;
		m.channel = channel;
		m.senderId = sender;
		m.senderName = senderName;
		m.accountId = account;
		m.zoneId = 1100;
		m.instanceId = 2;
		m.message = text;
		return m;
	}

	IChatLog::ChatMessage Whisper(int64_t time, LWOOBJID from, const std::string& fromName, LWOOBJID to, const std::string& toName, const std::string& text) {
		auto m = Message(time, "whisper", from, fromName, from == ALICE ? 1 : from == BOB ? 2 : 3, text);
		m.recipientId = to;
		m.recipientName = toName;
		return m;
	}

	std::vector<std::string> Texts(const std::vector<IChatLog::ChatMessage>& messages) {
		std::vector<std::string> out;
		for (const auto& m : messages) out.push_back(m.message);
		return out;
	}

	// Every test gets the same chat: zone chat, a guild, a team and whispers between three characters
	class ChatHistorySqlite : public ::testing::Test {
	protected:
		static inline std::vector<uint64_t> ids;

		static void SetUpTestSuite() {
			ASSERT_TRUE(g_Db);
			auto& db = *g_Db;
			ids.push_back(db.InsertChatMessage(Message(1000, "zone", ALICE, "Alice", 1, "hello zone")));
			auto guild = Message(1010, "guild", ALICE, "Alice", 1, "guild hi");
			guild.guildId = 7;
			ids.push_back(db.InsertChatMessage(guild));
			auto team = Message(1020, "team", BOB, "Bob", 2, "team darn");
			team.teamId = TEAM;
			team.filtered = true;
			ids.push_back(db.InsertChatMessage(team));
			ids.push_back(db.InsertChatMessage(Whisper(1030, ALICE, "Alice", BOB, "Bob", "psst bob")));
			ids.push_back(db.InsertChatMessage(Whisper(1040, BOB, "Bob", ALICE, "Alice", "yes alice")));
			ids.push_back(db.InsertChatMessage(Whisper(1050, ALICE, "Alice", CAROL, "Carol", "hi carol")));
			auto blocked = Message(1060, "zone", BOB, "Bob", 2, "bad words");
			blocked.blocked = true;
			ids.push_back(db.InsertChatMessage(blocked));
			team.senderId = ALICE; team.senderName = "Alice"; team.accountId = 1; team.time = 1070; team.message = "team reply"; team.filtered = false;
			ids.push_back(db.InsertChatMessage(team));
		}
	};
}

TEST_F(ChatHistorySqlite, StoresWhereEachMessageWent) {
	IChatLog::ChatQuery all;
	all.includePrivate = true;
	all.includeWhispers = true;
	const auto messages = g_Db->GetChatMessages(all);
	ASSERT_EQ(messages.size(), 8u);
	EXPECT_EQ(messages[1].guildId, 7);
	EXPECT_EQ(messages[2].teamId, TEAM);
	EXPECT_TRUE(messages[2].filtered);
	EXPECT_FALSE(messages[2].blocked);
	EXPECT_EQ(messages[3].recipientId, BOB);
	// Stopped by the filter is always filtered too
	EXPECT_TRUE(messages[6].blocked);
	EXPECT_TRUE(messages[6].filtered);
}

TEST_F(ChatHistorySqlite, WhispersAndGroupChatNeedTheirOwnPermission) {
	IChatLog::ChatQuery zoneOnly;
	EXPECT_EQ(Texts(g_Db->GetChatMessages(zoneOnly)), (std::vector<std::string>{ "hello zone", "bad words" }));
	IChatLog::ChatQuery group;
	group.includePrivate = true;
	EXPECT_EQ(g_Db->CountChatMessages(group), 5u); // no whispers
	IChatLog::ChatQuery whispers;
	whispers.includeWhispers = true;
	EXPECT_EQ(g_Db->CountChatMessages(whispers), 5u); // no team or guild chat
	// Asking for a channel doesn't get round it
	whispers.channel = "team";
	EXPECT_EQ(g_Db->CountChatMessages(whispers), 0u);
}

TEST_F(ChatHistorySqlite, ConversationBetweenTwoCharacters) {
	IChatLog::ChatQuery q;
	q.includeWhispers = true;
	q.channel = "whisper";
	q.characterId = ALICE;
	q.otherCharacterId = BOB;
	EXPECT_EQ(Texts(g_Db->GetChatMessages(q)), (std::vector<std::string>{ "psst bob", "yes alice" }));
	q.otherCharacterId = CAROL;
	EXPECT_EQ(Texts(g_Db->GetChatMessages(q)), (std::vector<std::string>{ "hi carol" }));
}

TEST_F(ChatHistorySqlite, GuildTeamTimeAndPaging) {
	IChatLog::ChatQuery guild;
	guild.includePrivate = true;
	guild.guildId = 7;
	EXPECT_EQ(Texts(g_Db->GetChatMessages(guild)), (std::vector<std::string>{ "guild hi" }));
	IChatLog::ChatQuery team;
	team.includePrivate = true;
	team.teamId = TEAM;
	EXPECT_EQ(Texts(g_Db->GetChatMessages(team)), (std::vector<std::string>{ "team darn", "team reply" }));

	IChatLog::ChatQuery range;
	range.includePrivate = true;
	range.includeWhispers = true;
	range.since = 1020;
	range.until = 1050;
	EXPECT_EQ(Texts(g_Db->GetChatMessages(range)), (std::vector<std::string>{ "team darn", "psst bob", "yes alice" }));

	// Paging back through a conversation: the newest page, then the one before it
	IChatLog::ChatQuery page;
	page.includePrivate = true;
	page.includeWhispers = true;
	page.newestFirst = true;
	page.limit = 3;
	const auto newest = g_Db->GetChatMessages(page);
	ASSERT_EQ(newest.size(), 3u);
	EXPECT_EQ(newest.front().message, "team reply");
	page.beforeId = newest.back().id;
	EXPECT_EQ(Texts(g_Db->GetChatMessages(page)), (std::vector<std::string>{ "yes alice", "psst bob", "team darn" }));

	IChatLog::ChatQuery account;
	account.includeWhispers = true;
	account.includePrivate = true;
	account.accountId = 2;
	EXPECT_EQ(g_Db->CountChatMessages(account), 3u);
}

TEST_F(ChatHistorySqlite, WhisperPartnersNewestFirst) {
	const auto partners = g_Db->GetWhisperPartners(ALICE, 0, 10);
	ASSERT_EQ(partners.size(), 2u);
	EXPECT_EQ(partners[0].characterId, CAROL);
	EXPECT_EQ(partners[0].name, "Carol");
	EXPECT_EQ(partners[0].messages, 1u);
	EXPECT_EQ(partners[1].characterId, BOB);
	EXPECT_EQ(partners[1].name, "Bob");
	EXPECT_EQ(partners[1].messages, 2u);
	EXPECT_EQ(partners[1].firstTime, 1030);
	EXPECT_EQ(partners[1].lastTime, 1040);
	EXPECT_EQ(g_Db->CountWhisperPartners(ALICE), 2u);
	EXPECT_EQ(g_Db->CountWhisperPartners(BOB), 1u);
	EXPECT_EQ(g_Db->GetWhisperPartners(ALICE, 1, 10).size(), 1u);
}

TEST_F(ChatHistorySqlite, TeamsWithWhoTalked) {
	const auto teams = g_Db->GetChatTeams(0, 0, 10);
	ASSERT_EQ(teams.size(), 1u);
	EXPECT_EQ(teams[0].teamId, TEAM);
	EXPECT_EQ(teams[0].messages, 2u);
	EXPECT_EQ(teams[0].senders, "Alice,Bob");
	EXPECT_EQ(g_Db->CountChatTeams(ALICE), 1u);
	EXPECT_EQ(g_Db->CountChatTeams(CAROL), 0u);
	EXPECT_TRUE(g_Db->GetChatTeams(CAROL, 0, 10).empty());
}

TEST_F(ChatHistorySqlite, FlagsKeepTheirMessagesAndHistory) {
	IChatFlags::ChatFlag flag;
	flag.createdAt = 2000;
	flag.createdById = 3;
	flag.createdBy = "gm";
	flag.channel = "whisper";
	flag.characterId = ALICE;
	flag.characterName = "Alice";
	flag.accountId = 1;
	flag.firstMessageId = ids[3];
	flag.lastMessageId = ids[4];
	flag.excerpt = "Alice: psst bob / Bob: yes alice";
	flag.note = "rude";
	flag.messages = R"([{"id":4,"message":"psst bob","flagged":true}])";
	const auto id = g_Db->InsertChatFlag(flag, { ids[3], ids[4] });
	ASSERT_GT(id, 0u);

	auto zoneFlag = flag;
	zoneFlag.channel = "zone";
	zoneFlag.characterId = BOB;
	zoneFlag.accountId = 2;
	const auto zoneId = g_Db->InsertChatFlag(zoneFlag, { ids[6] });

	const auto stored = g_Db->GetChatFlag(id);
	ASSERT_TRUE(stored.has_value());
	EXPECT_EQ(stored->status, "open");
	EXPECT_EQ(stored->messages, flag.messages);
	EXPECT_EQ(stored->characterId, ALICE);
	EXPECT_FALSE(g_Db->GetChatFlag(id + 100).has_value());

	// Which shown messages are flagged, with the newest flag on each
	const auto flagged = g_Db->GetFlaggedMessages({ ids[0], ids[3], ids[4], ids[6] });
	EXPECT_EQ(flagged, (std::vector<std::pair<uint64_t, uint64_t>>{ { ids[3], id }, { ids[4], id }, { ids[6], zoneId } }));
	EXPECT_TRUE(g_Db->GetFlaggedMessages({}).empty());

	// A viewer without chat_dms doesn't get flags on whispers in the queue
	IChatFlags::ChatFlagQuery q;
	EXPECT_EQ(g_Db->CountChatFlags(q), 1u);
	q.includeWhispers = true;
	EXPECT_EQ(g_Db->CountChatFlags(q), 2u);
	const auto list = g_Db->GetChatFlags(q);
	ASSERT_EQ(list.size(), 2u);
	EXPECT_EQ(list[0].id, zoneId); // newest first
	EXPECT_TRUE(list[0].messages.empty()); // the list leaves the copy of the chat out
	q.characterId = ALICE;
	EXPECT_EQ(g_Db->CountChatFlags(q), 1u);
	q.characterId = 0;
	q.accountId = 2;
	EXPECT_EQ(g_Db->CountChatFlags(q), 1u);

	EXPECT_TRUE(g_Db->UpdateChatFlag(id, "actioned", "muted 3 days", 42, 2100, "gm"));
	EXPECT_FALSE(g_Db->UpdateChatFlag(id + 100, "actioned", "", 0, 2100, "gm"));
	const auto updated = g_Db->GetChatFlag(id);
	EXPECT_EQ(updated->status, "actioned");
	EXPECT_EQ(updated->note, "muted 3 days");
	EXPECT_EQ(updated->playerReportId, 42u);
	EXPECT_EQ(updated->updatedBy, "gm");
	IChatFlags::ChatFlagQuery open;
	open.includeWhispers = true;
	open.status = "open";
	EXPECT_EQ(g_Db->CountChatFlags(open), 1u);

	g_Db->InsertChatFlagEvent({ 0, id, 2000, 3, "gm", "created", "2 message(s)" });
	g_Db->InsertChatFlagEvent({ 0, id, 2100, 3, "gm", "actioned", "muted" });
	const auto events = g_Db->GetChatFlagEvents(id);
	ASSERT_EQ(events.size(), 2u);
	EXPECT_EQ(events[0].action, "created");
	EXPECT_EQ(events[1].action, "actioned");
	EXPECT_EQ(events[1].detail, "muted");
	EXPECT_TRUE(g_Db->GetChatFlagEvents(zoneId).empty());

	// The badge count
	EXPECT_EQ(g_Db->GetDashboardSnapshot().openChatFlags, 1u);
}

TEST_F(ChatHistorySqlite, PruningOldChatKeepsFlags) {
	IChatLog::ChatQuery all;
	all.includePrivate = true;
	all.includeWhispers = true;
	const auto before = g_Db->CountChatMessages(all);
	const auto flags = g_Db->CountChatFlags({ .includePrivate = true, .includeWhispers = true });
	EXPECT_EQ(g_Db->PruneLog(IDashboardAdmin::eLog::CHAT, 1035), 4u);
	EXPECT_EQ(g_Db->CountChatMessages(all), before - 4);
	EXPECT_EQ(g_Db->CountChatFlags({ .includePrivate = true, .includeWhispers = true }), flags);
}
