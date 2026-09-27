/**
 * MySQL / SQLite parity tests for the game database.
 *
 * Creates a fresh MySQL (MariaDB) database and a fresh SQLite file, runs every migration on both the way the servers do
 * (MigrationRunner::RunMigrations), then calls the IDatabase methods with the same data on both and compares the
 * results. Needs a MariaDB/MySQL server, so every test is skipped unless DLU_TEST_MYSQL_HOST is set:
 *
 *   DLU_TEST_MYSQL_HOST      host for the connector, e.g. tcp://127.0.0.1:3307 or unix:///run/mysqld/mysqld.sock
 *   DLU_TEST_MYSQL_USER      default root
 *   DLU_TEST_MYSQL_PASSWORD  default empty
 *   DLU_TEST_MYSQL_DATABASE  default dlu_parity_test; it is DROPPED and recreated, so its name must contain "test"
 */
#include <gtest/gtest.h>

#include <conncpp.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <unistd.h>

#include "BinaryPathFinder.h"
#include "CppSQLite3.h"
#include "Database.h"
#include "dConfig.h"
#include "eGameMasterLevel.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "MigrationRunner.h"
#include "MySQLDatabase.h"
#include "SQLiteDatabase.h"
#include "json.hpp"

namespace Game {
	Logger* logger = nullptr;
	dConfig* config = nullptr;
}

using json = nlohmann::json;

namespace nlohmann {
	template<typename T>
	struct adl_serializer<std::optional<T>> {
		static void to_json(json& j, const std::optional<T>& value) {
			if (value) j = *value;
			else j = nullptr;
		}
		static void from_json(const json& j, std::optional<T>& value) {
			if (j.is_null()) value = std::nullopt;
			else value = j.get<T>();
		}
	};
}

// ---- JSON for the structs the interfaces return, so the two backends' results can be compared ----
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IAccounts::Info, bcryptPassword, id, playKeyId, muteExpire, banned, locked, banExpires, banReason, maxGmLevel);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IAccountEmails::EmailInfo, email, confirmed);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IAccountEmails::AccountToken, accountId, data);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IAccountNotes::AccountNote, id, accountId, kind, text, actor, createdAt);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IAccountStrikes::Strike, id, accountId, characterId, source, subject, reason, givenById, givenBy, createdAt, revokedAt, revokedBy, revokeReason);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IApiKeys::ApiKey, id, accountId, name, note, keyHash, keyPrefix, permissions, readOnly, allowedIps, allowedPaths, rateLimit, dailyQuota, createdAt, createdBy, issuedAt, expiresAt, revokedAt, revokedBy, lastUsedAt, lastIp, requestCount, quotaDay, dayCount);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ICharacterSnapshots::CharacterSnapshot, id, characterId, takenAt, reason, actor, size, hash, compressed);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IChatLog::ChatMessage, id, time, channel, senderId, senderName, accountId, recipientId, recipientName, zoneId, instanceId, cloneId, message, blocked);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IDashboardAdmin::Webhook, id, name, url, format, events, secret, enabled, createdAt, lastSentAt, lastStatus, lastError);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IDashboardAdmin::Totp, encryptedSecret, enabledAt, lastStep);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IDashboardStats::Snapshot, accounts, accountsMaxId, characters, pendingNames, properties, pendingProperties, playKeys, bugReports, unresolvedBugReports, petNames, pendingPetNames, activityLogMaxId, chatLogMaxId, commandLogMaxId, auditLogMaxId, mailMaxId, openEconomyFlags, economyFlagsMaxId);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IEconomyLedger::MailAttachment, mailId, receiverId, itemId, lot, count);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IModeration::AppliedStrikeStep, step, count, time);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IModeration::PlayerReport, id, createdAt, kind, reporterId, reporterAccountId, objectId, objectLot, targetCharacterId, targetAccountId, propertyId, zoneId, instanceId, cloneId, body, status, handledBy, handledAt, resolution);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IModeration::ChatFilterWord, word, allowed, addedBy, addedAt);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IModeration::LinkedAccount, accountId, name, gmLevel, banned, link, shared, lastSeen);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IScheduledTasks::TaskSettings, name, schedule, enabled, lastScheduledAt, updatedAt, updatedBy);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IScheduledTasks::TaskRun, id, task, trigger, actor, startedAt, finishedAt, status, summary, log);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IServerConfig::Setting, file, name, fileValue, fileSource, webValue, webWins, secret, description, seenAt, updatedAt, updatedBy);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IServerConfig::SettingChange, id, file, name, oldValue, oldWebWins, newValue, newWebWins, fileValue, secret, removed, revertOf, changedAt, accountId, changedBy);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IServerHealth::HealthSample, time, players, worlds, authOnline, chatOnline, memoryKb, ugcEnabled, ugcOnline);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IServerHealth::InstanceSample, time, zoneId, instanceId, cloneId, players);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IPlayerPositions::PositionSample, time, characterId, zoneId, instanceId, cloneId, x, y, z);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IPlayerPositions::PositionInstance, zoneId, instanceId, cloneId, first, last, players);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IServerOperations::ScheduledAnnouncement, id, title, message, zones, schedule, startsAt, endsAt, enabled, lastSentAt, sentCount, createdAt, createdBy, updatedAt, updatedBy);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IServerOperations::ScheduledEvent, id, name, note, mode, schedule, startsAt, endsAt, priority, parts, state, status, createdAt, createdBy, updatedAt, updatedBy);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IServerOperations::ZoneLimit, zoneId, softCap, hardCap, spareInstances, updatedAt, updatedBy);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ISlashCommands::SlashCommand, name, aliases, help, info, defaultLevel, minLevel, fixed, clientHandled, note, dashboardPermission);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ICharInfo::Info, name, pendingName, id, accountId, needsRename, cloneId, permissionMap);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IProperty::Info, name, description, rejectionReason, id, ownerId, cloneId, privacyOption, modApproved, lastUpdatedTime, claimedTime, reputation, performanceCost, zoneId);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IProperty::ShowcaseEntry, info, ownerName, modelCount);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IProperty::ShowcaseResult, total, entries);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IPetNames::Info, petName, approvalStatus, ownerId, petLot);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ILeaderboard::Entry, charId, lastPlayedTimestamp, primaryScore, secondaryScore, tertiaryScore, numWins, numTimesPlayed, ranking, name);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ILeaderboard::Score, primaryScore, secondaryScore, tertiaryScore);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ILiveOps::LiveEvent, id, type, title, message, zones, instanceId, config, startsAt, endsAt, state, endedAt, endReason, createdBy, endedBy);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ILiveOps::LiveEventInstance, eventId, zoneId, instanceId, status, updatedAt);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ILiveOps::LiveOpsScore, id, characterId, name, amount);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ILiveOps::Challenge, id, title, description, metricKind, metric, lot, zones, target, startsAt, endsAt, includeStaff, isPublic, rewardCoins, rewardItems, rewardMin, state, milestone, completedAt, rewardedAt, rewardedCount, createdAt, createdBy, updatedAt, updatedBy);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ILiveOps::ChallengeTotal, total, contributors);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ILiveOps::Reward, challengeId, characterId, amount, coins, rewardedAt, claimedAt);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IFeaturedProperties::FeaturedSlot, templateId, mode, propertyId, updatedAt, updatedBy, zoneId);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IPropertyRent::RentRate, mapId, price, periodDays, updatedBy, updatedAt);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IPropertyRent::OwnedProperty, id, zoneId, privacyOption, rentDue, name);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IContraband::ContrabandItem, lot, reason, action, addedBy, addedAt);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IFeaturedProperties::FeaturedSettings, fullAuto, updatedAt, updatedBy);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IMessageCaptures::MessageCaptureSession, id, characterId, characterName, accountId, accountName, startedById, startedBy, startedAt, endsAt, endedAt, endReason, toServer, toClient, onlyMessages, skipMessages, zoneId, instanceId, cloneId, zones, messageCount, byteCount, dropped);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IMessageCaptures::MessageCaptureRecord, sessionId, seq, timeMs, direction, messageId, objectId, bits, droppedBefore, zoneId, instanceId, cloneId, payload, decoded);

void to_json(json& j, const MailInfo& mail) {
	j = json{ {"senderUsername", mail.senderUsername}, {"subject", mail.subject}, {"body", mail.body}, {"id", mail.id},
		{"senderId", mail.senderId}, {"receiverId", mail.receiverId}, {"timeSent", mail.timeSent}, {"wasRead", mail.wasRead},
		{"itemConfig", mail.itemConfig}, {"itemID", mail.itemID}, {"itemCount", mail.itemCount}, {"itemLOT", mail.itemLOT},
		{"itemSubkey", mail.itemSubkey} };
}

namespace {
	struct Backend {
		std::string name;
		std::unique_ptr<GameDatabase> db;
	};

	// Filled by the environment; empty when the tests are skipped
	std::vector<Backend> g_Backends;
	std::string g_SkipReason;
	std::string g_SetupError;
	std::filesystem::path g_LogPath;
	std::filesystem::path g_SqlitePath;
	std::string g_MysqlDatabase;
	std::map<std::string, std::vector<std::string>> g_MigrationErrors; // backend -> errors logged while migrating

	sql::Properties MysqlProperties() {
		sql::Properties properties;
		const std::string host = std::getenv("DLU_TEST_MYSQL_HOST");
		if (host.starts_with("unix://")) {
			properties["hostName"] = "unix://localhost";
			properties["localSocket"] = host.substr(7).c_str();
		} else {
			properties["hostName"] = host.c_str();
		}
		const char* user = std::getenv("DLU_TEST_MYSQL_USER");
		const char* password = std::getenv("DLU_TEST_MYSQL_PASSWORD");
		properties["user"] = user ? user : "root";
		properties["password"] = password ? password : "";
		return properties;
	}

	std::unique_ptr<sql::Connection> RawMysql(bool withSchema = true) {
		auto properties = MysqlProperties();
		std::unique_ptr<sql::Connection> con(sql::mariadb::get_driver_instance()->connect(properties));
		if (withSchema) con->setSchema(g_MysqlDatabase.c_str());
		return con;
	}

	std::vector<std::string> ReadLog() {
		Game::logger->Flush();
		std::ifstream in(g_LogPath);
		std::vector<std::string> lines;
		for (std::string line; std::getline(in, line);) lines.push_back(line);
		return lines;
	}

	void Migrate(const std::string& type) {
		setenv("DATABASE_TYPE", type.c_str(), 1);
		const auto before = ReadLog().size();
		Database::Connect();
		MigrationRunner::RunMigrations();
		Database::Destroy("dDatabaseTests migrations");
		const auto lines = ReadLog();
		for (size_t i = before; i < lines.size(); i++) {
			if (lines[i].find("Encountered error") == std::string::npos) continue;
			// mysql 10_Security_updates.sql has a foreign key MariaDB rejects (TEXT column); 11 drops and recreates the table
			if (type == "mysql" && lines[i].find("player_cheat_detections") != std::string::npos && lines[i].find("errno: 150") != std::string::npos) continue;
			g_MigrationErrors[type].push_back(lines[i]);
		}
	}

	class ParityEnvironment : public ::testing::Environment {
	public:
		void SetUp() override {
			g_LogPath = std::filesystem::temp_directory_path() / ("dDatabaseTests_" + std::to_string(getpid()) + ".log");
			std::filesystem::remove(g_LogPath);
			Game::logger = new Logger(g_LogPath.string(), false, false);

			if (!std::getenv("DLU_TEST_MYSQL_HOST")) {
				g_SkipReason = "DLU_TEST_MYSQL_HOST is not set; no MariaDB/MySQL server to test against";
				return;
			}
			const char* database = std::getenv("DLU_TEST_MYSQL_DATABASE");
			// ctest runs every test in its own process, several at once, so each process gets its own databases
			g_MysqlDatabase = std::string(database ? database : "dlu_parity_test") + "_" + std::to_string(getpid());
			if (g_MysqlDatabase.find("test") == std::string::npos || g_MysqlDatabase.find('`') != std::string::npos) {
				g_SetupError = "DLU_TEST_MYSQL_DATABASE must contain \"test\": it is dropped and recreated";
				return;
			}

			try {
				auto raw = RawMysql(false);
				std::unique_ptr<sql::Statement> statement(raw->createStatement());
				statement->execute(("DROP DATABASE IF EXISTS `" + g_MysqlDatabase + "`;").c_str());
				statement->execute(("CREATE DATABASE `" + g_MysqlDatabase + "`;").c_str());
			} catch (const std::exception& ex) {
				g_SetupError = std::string("Could not recreate the MySQL test database: ") + ex.what();
				return;
			}

			g_SqlitePath = std::filesystem::temp_directory_path() / ("dDatabaseTests_" + std::to_string(getpid()) + ".sqlite");
			for (const auto* suffix : { "", "-wal", "-shm" }) std::filesystem::remove(g_SqlitePath.string() + suffix);

			// The servers' connection code reads these through the config; environment values win over the ini files
			setenv("MYSQL_HOST", std::getenv("DLU_TEST_MYSQL_HOST"), 1);
			setenv("MYSQL_USERNAME", MysqlProperties()["user"].c_str(), 1);
			setenv("MYSQL_PASSWORD", MysqlProperties()["password"].c_str(), 1);
			setenv("MYSQL_DATABASE", g_MysqlDatabase.c_str(), 1);
			setenv("SQLITE_DATABASE_PATH", g_SqlitePath.string().c_str(), 1);
			Game::config = new dConfig("dDatabaseTests.ini");

			try {
				Migrate("sqlite");
				Migrate("mysql");

				setenv("DATABASE_TYPE", "sqlite", 1);
				g_Backends.push_back({ "sqlite", std::make_unique<SQLiteDatabase>() });
				g_Backends.back().db->Connect();
				setenv("DATABASE_TYPE", "mysql", 1);
				g_Backends.push_back({ "mysql", std::make_unique<MySQLDatabase>() });
				g_Backends.back().db->Connect();
			} catch (const std::exception& ex) {
				g_SetupError = std::string("Setting up the databases failed: ") + ex.what();
				g_Backends.clear();
			}
		}

		void TearDown() override {
			for (auto& backend : g_Backends) backend.db->Destroy("dDatabaseTests");
			g_Backends.clear();
			if (!g_MysqlDatabase.empty() && g_SetupError.empty() && g_SkipReason.empty()) {
				try {
					auto raw = RawMysql(false);
					std::unique_ptr<sql::Statement>(raw->createStatement())->execute(("DROP DATABASE IF EXISTS `" + g_MysqlDatabase + "`;").c_str());
				} catch (const std::exception&) {}
			}
			if (!g_SqlitePath.empty()) {
				for (const auto* suffix : { "", "-wal", "-shm" }) std::filesystem::remove(g_SqlitePath.string() + suffix);
			}
			delete Game::config;
			Game::config = nullptr;
			delete Game::logger;
			Game::logger = nullptr;
		}
	};

	const auto* const g_Environment = ::testing::AddGlobalTestEnvironment(new ParityEnvironment());

	int64_t Now() {
		return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	/**
	 * Values the database fills in with the current time can't be compared exactly: unix times within a few minutes of
	 * now become "<now>" and date-time strings keep only their shape. A time zone mistake (hours off) still shows.
	 */
	json Normalize(const json& value) {
		static const std::regex dateTime(R"(^\d{4}-\d{2}-\d{2}[ T]\d{2}:\d{2}:\d{2})");
		if (value.is_object()) {
			json out = json::object();
			for (const auto& [key, item] : value.items()) out[key] = Normalize(item);
			return out;
		}
		if (value.is_array()) {
			json out = json::array();
			for (const auto& item : value) out.push_back(Normalize(item));
			return out;
		}
		if (value.is_number_integer()) {
			const auto number = value.get<int64_t>();
			if (std::llabs(number - Now()) < 300) return "<now>";
			return value;
		}
		if (value.is_string()) {
			const auto& text = value.get_ref<const std::string&>();
			if (std::regex_search(text, dateTime)) return std::regex_replace(text, std::regex(R"(\d)"), "N");
			return value;
		}
		return value;
	}

	template<typename T>
	json ToJson(const T& value) {
		if constexpr (std::is_same_v<T, std::string>) {
			// DataTables results are JSON text
			if (!value.empty() && (value.front() == '{' || value.front() == '[')) {
				auto parsed = json::parse(value, nullptr, false);
				if (!parsed.is_discarded()) return parsed;
			}
			return value;
		} else {
			return json(value);
		}
	}

	/**
	 * Run the same call on every backend and expect the same (normalized) result. Returns the SQLite result.
	 */
	template<typename F>
	json Both(const std::string& what, F&& call) {
		std::vector<json> results;
		for (auto& backend : g_Backends) {
			json result;
			try {
				if constexpr (std::is_void_v<decltype(call(*backend.db))>) {
					call(*backend.db);
					result = "done";
				} else {
					result = ToJson(call(*backend.db));
				}
			} catch (const std::exception& ex) {
				ADD_FAILURE() << what << " threw on " << backend.name << ": " << ex.what();
				result = json{ {"exception", ex.what()} };
			}
			results.push_back(Normalize(result));
		}
		for (size_t i = 1; i < results.size(); i++) {
			EXPECT_EQ(results[0], results[i]) << what << "\n  " << g_Backends[0].name << ": " << results[0].dump()
				<< "\n  " << g_Backends[i].name << ": " << results[i].dump();
		}
		return results.empty() ? json() : results[0];
	}
}

class Parity : public ::testing::Test {
protected:
	void SetUp() override {
		if (!g_SkipReason.empty()) GTEST_SKIP() << g_SkipReason;
		ASSERT_TRUE(g_SetupError.empty()) << g_SetupError;
		ASSERT_EQ(g_Backends.size(), 2u);
	}
};

// ---- Migrations ----

TEST_F(Parity, MigrationsRunWithoutErrors) {
	for (const auto& type : { "sqlite", "mysql" }) {
		for (const auto& error : g_MigrationErrors[type]) ADD_FAILURE() << type << ": " << error;
	}
}

TEST_F(Parity, EveryMigrationIsRecorded) {
	for (const auto& type : { "sqlite", "mysql" }) {
		const auto files = GeneralUtils::GetSqlFileNamesFromFolder((BinaryPathFinder::GetBinaryDir() / "migrations/dlu" / type).string());
		ASSERT_FALSE(files.empty());
		for (const auto& file : files) {
			EXPECT_TRUE((type == std::string("sqlite") ? g_Backends[0] : g_Backends[1]).db->IsMigrationRun(file)) << type << " " << file;
		}
	}
}

namespace {
	std::string Lower(std::string text) {
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
		return text;
	}

	std::map<std::string, std::set<std::string>> SqliteSchema() {
		std::map<std::string, std::set<std::string>> schema;
		CppSQLite3DB db;
		db.open(g_SqlitePath.string().c_str());
		auto tables = db.execQuery("SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%';");
		std::vector<std::string> names;
		for (; !tables.eof(); tables.nextRow()) names.push_back(tables.getStringField(0));
		tables.finalize();
		for (const auto& table : names) {
			auto columns = db.execQuery(("PRAGMA table_info(" + table + ");").c_str());
			for (; !columns.eof(); columns.nextRow()) schema[table].insert(Lower(columns.getStringField("name")));
		}
		db.close();
		return schema;
	}

	std::map<std::string, std::set<std::string>> MysqlSchema() {
		std::map<std::string, std::set<std::string>> schema;
		auto con = RawMysql();
		std::unique_ptr<sql::PreparedStatement> statement(con->prepareStatement(
			"SELECT TABLE_NAME, COLUMN_NAME FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = ?;"));
		statement->setString(1, g_MysqlDatabase.c_str());
		std::unique_ptr<sql::ResultSet> result(statement->executeQuery());
		while (result->next()) schema[result->getString(1).c_str()].insert(Lower(result->getString(2).c_str()));
		return schema;
	}
}

TEST_F(Parity, SchemasHaveTheSameTablesAndColumns) {
	const auto sqlite = SqliteSchema();
	const auto mysql = MysqlSchema();
	for (const auto& [table, columns] : sqlite) {
		const auto it = mysql.find(table);
		if (it == mysql.end()) {
			ADD_FAILURE() << "table " << table << " is only in SQLite";
			continue;
		}
		for (const auto& column : columns) EXPECT_TRUE(it->second.contains(column)) << table << "." << column << " is only in SQLite";
		for (const auto& column : it->second) EXPECT_TRUE(columns.contains(column)) << table << "." << column << " is only in MySQL";
	}
	for (const auto& [table, columns] : mysql) EXPECT_TRUE(sqlite.contains(table)) << "table " << table << " is only in MySQL";
}

// ---- Accounts, play keys, characters ----

namespace {
	constexpr LWOOBJID CHAR_ALICE = 1152921510000000001LL;
	constexpr LWOOBJID CHAR_ALICE2 = 1152921510000000002LL;
	constexpr LWOOBJID CHAR_BOB = 1152921510000000003LL;
	constexpr LWOOBJID CHAR_GM = 1152921510000000004LL;
	constexpr LWOOBJID PET1 = 1152921510000200001LL, PET2 = 1152921510000200002LL, PET3 = 1152921510000200003LL;
	constexpr LWOOBJID PROP1 = 1152921510000300001LL, PROP2 = 1152921510000300002LL, PROP3 = 1152921510000300003LL;

	// Accounts 1 alice, 2 bob, 3 gm (level 9); characters as above. Every test after this one can rely on them.
	void SeedAccounts() {
		static bool seeded = false;
		if (seeded) return;
		seeded = true;
		Both("InsertNewAccount alice", [](GameDatabase& db) { db.InsertNewAccount("alice", "$2a$12$hashalice", eGameMasterLevel::CIVILIAN); });
		Both("InsertNewAccount bob", [](GameDatabase& db) { db.InsertNewAccount("bob", "$2a$12$hashbob", eGameMasterLevel::CIVILIAN); });
		Both("InsertNewAccount gm", [](GameDatabase& db) { db.InsertNewAccount("gm", "$2a$12$hashgm", eGameMasterLevel::OPERATOR); });
		const auto character = [](const std::string& name, const std::string& pending, LWOOBJID id, uint32_t account) {
			return ICharInfo::Info{ name, pending, id, account, false, 0, static_cast<ePermissionMap>(0) };
		};
		Both("InsertNewCharacter", [&](GameDatabase& db) {
			db.InsertNewCharacter(character("Alice", "", CHAR_ALICE, 1));
			db.InsertNewCharacter(character("AliceTwo", "AliceRenamed", CHAR_ALICE2, 1));
			db.InsertNewCharacter(character("Bob", "", CHAR_BOB, 2));
			db.InsertNewCharacter(character("GameMaster", "", CHAR_GM, 3));
		});
		Both("InsertCharacterXml", [](GameDatabase& db) {
			db.InsertCharacterXml(CHAR_ALICE, "<obj v=\"1\"><char cc=\"100\"/><inv><items><in t=\"0\"><i l=\"6086\" id=\"1152921510000100001\" s=\"0\"/></in></items></inv></obj>");
			db.InsertCharacterXml(CHAR_ALICE2, "<obj v=\"1\"><char cc=\"5\"/></obj>");
			db.InsertCharacterXml(CHAR_BOB, "<obj v=\"1\"><char cc=\"42\"/><pet id=\"1152921510000200001\"/></obj>");
			db.InsertCharacterXml(CHAR_GM, "<obj v=\"1\"><char cc=\"0\"/></obj>");
		});
	}
}

class ParitySeeded : public Parity {
protected:
	void SetUp() override {
		Parity::SetUp();
		if (IsSkipped() || HasFatalFailure()) return;
		SeedAccounts();
	}
};

TEST_F(ParitySeeded, Accounts) {
	Both("GetAccountInfo", [](GameDatabase& db) { return db.GetAccountInfo("alice"); });
	Both("GetAccountInfo missing", [](GameDatabase& db) { return db.GetAccountInfo("nobody"); });
	Both("GetAccountCount", [](GameDatabase& db) { return db.GetAccountCount(); });
	Both("UpdateAccountUnmuteTime", [](GameDatabase& db) { db.UpdateAccountUnmuteTime(2, 1900000000ULL); });
	Both("UpdateAccountBan", [](GameDatabase& db) { db.UpdateAccountBan(2, true); });
	Both("GetAccountInfo bob", [](GameDatabase& db) { return db.GetAccountInfo("bob"); });
	Both("UpdateAccountBan off", [](GameDatabase& db) { db.UpdateAccountBan(2, false); });
	Both("UpdateAccountPassword", [](GameDatabase& db) { db.UpdateAccountPassword(2, "$2a$12$newbob"); });
	Both("RecordFailedAttempt", [](GameDatabase& db) { db.RecordFailedAttempt(1); db.RecordFailedAttempt(1); });
	Both("GetFailedAttempts", [](GameDatabase& db) { return db.GetFailedAttempts(1); });
	Both("SetLockout", [](GameDatabase& db) { db.SetLockout(1, Now() + 600); });
	Both("IsLockedOut", [](GameDatabase& db) { return db.IsLockedOut(1); });
	Both("SetLockout past", [](GameDatabase& db) { db.SetLockout(1, Now() - 600); });
	Both("IsLockedOut past", [](GameDatabase& db) { return db.IsLockedOut(1); });
	Both("ClearFailedAttempts", [](GameDatabase& db) { db.ClearFailedAttempts(1); return db.GetFailedAttempts(1); });
	Both("GetAccountsTable", [](GameDatabase& db) { return db.GetAccountsTable(0, 10); });
	Both("GetAccountsTable by character name", [](GameDatabase& db) { return db.GetAccountsTable(0, 10, "Bob"); });
	Both("GetAccountsTable by id", [](GameDatabase& db) { return db.GetAccountsTable(0, 10, "3"); });
	for (uint32_t column = 0; column < 7; column++) {
		Both("GetAccountsTable order " + std::to_string(column), [&](GameDatabase& db) { return db.GetAccountsTable(0, 2, "", column, false); });
	}
	Both("GetAccountById", [](GameDatabase& db) { return db.GetAccountById(1); });
	Both("GetAccountById missing", [](GameDatabase& db) { return db.GetAccountById(999); });
	Both("GetAccountCharacters", [](GameDatabase& db) { return db.GetAccountCharacters(1); });
	Both("UpdateAccountGmLevel", [](GameDatabase& db) { db.UpdateAccountGmLevel(2, eGameMasterLevel::JUNIOR_MODERATOR); return db.GetAccountById(2); });
	Both("UpdateAccountGmLevel back", [](GameDatabase& db) { db.UpdateAccountGmLevel(2, eGameMasterLevel::CIVILIAN); });
	Both("CountActiveAccountsAtGmLevel", [](GameDatabase& db) {
		db.UpdateAccountGmLevel(2, eGameMasterLevel::OPERATOR);
		const auto withOther = db.CountActiveAccountsAtGmLevel(9, 1);
		const auto excluded = db.CountActiveAccountsAtGmLevel(9, 2);
		db.SetAccountLocked(2, true);
		const auto locked = db.CountActiveAccountsAtGmLevel(9, 1);
		db.SetAccountLocked(2, false);
		db.UpdateAccountGmLevel(2, eGameMasterLevel::CIVILIAN);
		return json{ withOther, excluded, locked };
	});
}

TEST_F(ParitySeeded, AccountEmailsAndTokens) {
	Both("GetAccountEmail empty", [](GameDatabase& db) { return db.GetAccountEmail(1); });
	Both("GetAccountEmail missing", [](GameDatabase& db) { return db.GetAccountEmail(999); });
	Both("SetAccountEmail", [](GameDatabase& db) { db.SetAccountEmail(1, "Alice@Example.com", true); db.SetAccountEmail(2, "bob@example.com", false); });
	Both("GetAccountEmail", [](GameDatabase& db) { return json{ db.GetAccountEmail(1), db.GetAccountEmail(2) }; });
	Both("GetAccountIdByConfirmedEmail", [](GameDatabase& db) { return json{ db.GetAccountIdByConfirmedEmail("alice@example.COM"), db.GetAccountIdByConfirmedEmail("bob@example.com") }; });
	Both("InsertAccountToken", [](GameDatabase& db) {
		db.InsertAccountToken("aaaa", 1, "reset", "payload", Now() + 3600);
		db.InsertAccountToken("bbbb", 1, "verify", "", Now() - 10);
		db.InsertAccountToken("cccc", 2, "reset", "x", Now() + 3600);
	});
	Both("ConsumeAccountToken wrong purpose", [](GameDatabase& db) { return db.ConsumeAccountToken("aaaa", "verify"); });
	Both("ConsumeAccountToken again", [](GameDatabase& db) { return db.ConsumeAccountToken("aaaa", "reset"); }); // consumed by the wrong-purpose call
	Both("ConsumeAccountToken expired", [](GameDatabase& db) { return db.ConsumeAccountToken("bbbb", "verify"); });
	Both("InsertAccountToken 2", [](GameDatabase& db) { db.InsertAccountToken("dddd", 1, "reset", "p", Now() + 3600); db.InsertAccountToken("eeee", 1, "verify", "p", Now() - 3600); });
	Both("ConsumeAccountToken", [](GameDatabase& db) { return db.ConsumeAccountToken("dddd", "reset"); });
	Both("DeleteAccountTokens", [](GameDatabase& db) { db.DeleteAccountTokens(2, "reset"); return db.ConsumeAccountToken("cccc", "reset"); });
	Both("DeleteExpiredAccountTokens", [](GameDatabase& db) { db.DeleteExpiredAccountTokens(); });
	Both("SessionsValidAfter", [](GameDatabase& db) { const auto before = db.GetSessionsValidAfter(1); db.SetSessionsValidAfter(1, 1700000000); return json{ before, db.GetSessionsValidAfter(1), db.GetSessionsValidAfter(999) }; });
}

TEST_F(ParitySeeded, PlayKeys) {
	Both("CreatePlayKey", [](GameDatabase& db) { db.CreatePlayKey("KEY-AAAA-1111", 2, "for friends"); db.CreatePlayKey("KEY-BBBB-2222", 1); });
	Both("GetPlayKeyCount", [](GameDatabase& db) { return db.GetPlayKeyCount(); });
	Both("IsPlaykeyActive", [](GameDatabase& db) { return json{ db.IsPlaykeyActive(1), db.IsPlaykeyActive(99) }; });
	Both("GetRedeemablePlayKeyId", [](GameDatabase& db) { return db.GetRedeemablePlayKeyId("KEY-AAAA-1111"); });
	Both("SetAccountPlayKey", [](GameDatabase& db) { db.SetAccountPlayKey(1, 1); db.SetAccountPlayKey(2, 1); db.SetAccountPlayKey(3, 2); });
	Both("GetRedeemablePlayKeyId used up", [](GameDatabase& db) { return json{ db.GetRedeemablePlayKeyId("KEY-AAAA-1111"), db.GetRedeemablePlayKeyId("KEY-BBBB-2222") }; });
	Both("GetPlayKey", [](GameDatabase& db) { return db.GetPlayKey(1); });
	Both("GetPlayKey missing", [](GameDatabase& db) { return db.GetPlayKey(77); });
	Both("GetPlayKeysTable", [](GameDatabase& db) { return db.GetPlayKeysTable(0, 10); });
	Both("GetPlayKeysTable search account", [](GameDatabase& db) { return db.GetPlayKeysTable(0, 10, "bob"); });
	Both("GetPlayKeysTable search id", [](GameDatabase& db) { return db.GetPlayKeysTable(0, 10, "2"); });
	for (uint32_t column = 0; column < 6; column++) {
		Both("GetPlayKeysTable order " + std::to_string(column), [&](GameDatabase& db) { return db.GetPlayKeysTable(0, 10, "", column, false); });
	}
	Both("SetPlayKeyActive", [](GameDatabase& db) { db.SetPlayKeyActive(2, false); return db.IsPlaykeyActive(2); });
	Both("UpdatePlayKey", [](GameDatabase& db) { db.UpdatePlayKey(2, 5, "more uses", true); return db.GetPlayKey(2); });
	Both("CreatePlayKey 3", [](GameDatabase& db) { db.CreatePlayKey("KEY-CCCC-3333", 1, "to delete"); db.SetAccountPlayKey(3, 3); });
	Both("DeletePlayKey", [](GameDatabase& db) { db.DeletePlayKey(3); return json{ db.GetPlayKey(3), db.GetAccountInfo("gm") }; });
	Both("SetAccountPlayKey restore", [](GameDatabase& db) { db.SetAccountPlayKey(3, 2); });
}

TEST_F(ParitySeeded, Characters) {
	Both("GetApprovedCharacterNames", [](GameDatabase& db) { auto names = db.GetApprovedCharacterNames(); std::sort(names.begin(), names.end()); return names; });
	Both("GetCharacterIdsAndNames", [](GameDatabase& db) { return db.GetCharacterIdsAndNames(); });
	Both("GetPendingNamesTable", [](GameDatabase& db) { return db.GetPendingNamesTable(0, 10); });
	Both("SetCharacterPermissionMap", [](GameDatabase& db) { db.SetCharacterPermissionMap(CHAR_BOB, 0x30); return db.GetCharacterInfo(CHAR_BOB); });
	Both("SetCharacterPermissionMap back", [](GameDatabase& db) { db.SetCharacterPermissionMap(CHAR_BOB, 0); });
	Both("GetCharacterInfo name", [](GameDatabase& db) { return db.GetCharacterInfo("AliceTwo"); });
	Both("GetAccountCharacterIds", [](GameDatabase& db) { return db.GetAccountCharacterIds(1); });
	Both("GetCharacterCount", [](GameDatabase& db) { return db.GetCharacterCount(); });
	Both("IsNameInUse", [](GameDatabase& db) { return json{ db.IsNameInUse("Bob"), db.IsNameInUse("AliceRenamed"), db.IsNameInUse("Nobody") }; });
	Both("GetCharacterById", [](GameDatabase& db) { return db.GetCharacterById(CHAR_ALICE); });
	Both("GetCharacterById missing", [](GameDatabase& db) { return db.GetCharacterById(42); });
	Both("UpdateLastLoggedInCharacter", [](GameDatabase& db) { db.UpdateLastLoggedInCharacter(CHAR_ALICE); return db.GetAccountCharacters(1); });
	Both("GetCharactersTable", [](GameDatabase& db) { return db.GetCharactersTable(0, 10); });
	Both("GetCharactersTable search account", [](GameDatabase& db) { return db.GetCharactersTable(0, 10, "alice"); });
	Both("GetCharactersTable search id", [](GameDatabase& db) { return db.GetCharactersTable(0, 10, std::to_string(CHAR_BOB)); });
	for (uint32_t column = 0; column < 5; column++) {
		Both("GetCharactersTable order " + std::to_string(column), [&](GameDatabase& db) { return db.GetCharactersTable(0, 10, "", column, true); });
	}
	Both("GetCharacterXml", [](GameDatabase& db) { return db.GetCharacterXml(CHAR_BOB); });
	Both("UpdateCharacterXml", [](GameDatabase& db) { db.UpdateCharacterXml(CHAR_GM, "<obj v=\"1\"><char cc=\"1\"/></obj>"); return db.GetCharacterXml(CHAR_GM); });
	// Stale save guard: a world loads (claims), saves, then loses the character to another world or the dashboard
	const auto claim = Both("ClaimCharacterXml", [](GameDatabase& db) {
		const auto before = db.GetCharacterSaveGeneration(CHAR_GM);
		const auto claimed = db.ClaimCharacterXml(CHAR_GM);
		return json{ claimed.has_value(), claimed ? claimed->xml : "", claimed ? claimed->generation - before : 0, db.ClaimCharacterXml(42).has_value() };
	});
	EXPECT_EQ(claim, json({ true, "<obj v=\"1\"><char cc=\"1\"/></obj>", 1, false }));
	const auto saves = Both("SaveCharacterXml", [](GameDatabase& db) {
		const auto first = db.ClaimCharacterXml(CHAR_GM)->generation;
		const bool saved = db.SaveCharacterXml(CHAR_GM, "<obj v=\"1\"><char cc=\"2\"/></obj>", first);
		// Same content again from the new generation: still counted as saved (the generation always changes)
		const bool savedAgain = db.SaveCharacterXml(CHAR_GM, "<obj v=\"1\"><char cc=\"2\"/></obj>", first + 1);
		// Another world takes the character over; the first world's next save is stale
		const auto second = db.ClaimCharacterXml(CHAR_GM)->generation;
		const bool stale = db.SaveCharacterXml(CHAR_GM, "<obj v=\"1\"><char cc=\"999\"/></obj>", first + 2);
		const bool fresh = db.SaveCharacterXml(CHAR_GM, "<obj v=\"1\"><char cc=\"3\"/></obj>", second);
		// A dashboard edit bumps it too
		db.UpdateCharacterXml(CHAR_GM, "<obj v=\"1\"><char cc=\"4\"/></obj>");
		const bool afterEdit = db.SaveCharacterXml(CHAR_GM, "<obj v=\"1\"><char cc=\"998\"/></obj>", second + 1);
		return json{ saved, savedAgain, stale, fresh, afterEdit, second - first, db.GetCharacterSaveGeneration(CHAR_GM) - first, db.GetCharacterXml(CHAR_GM),
			db.SaveCharacterXml(42, "<obj/>", 0), db.GetCharacterSaveGeneration(42) };
	});
	EXPECT_EQ(saves, json({ true, true, false, true, false, 3, 5, "<obj v=\"1\"><char cc=\"4\"/></obj>", false, 0 }));
	Both("GetDashboardSnapshot", [](GameDatabase& db) { return db.GetDashboardSnapshot(); });
}

TEST_F(ParitySeeded, BugReports) {
	Both("InsertNewBugReport", [](GameDatabase& db) {
		db.InsertNewBugReport({ "Fell through the floor", "1.10.64", "Bob", "123", CHAR_ALICE });
		db.InsertNewBugReport({ "Quest does not complete; 100%", "1.10.64", "", "", CHAR_BOB });
		db.InsertNewBugReport({ "Unknown reporter", "1.10.64", "", "", 777 });
	});
	Both("GetBugReportCount", [](GameDatabase& db) { return db.GetBugReportCount(); });
	Both("GetBugReport", [](GameDatabase& db) { return db.GetBugReport(1); });
	Both("GetBugReport missing", [](GameDatabase& db) { return db.GetBugReport(99); });
	Both("ResolveBugReport", [](GameDatabase& db) { db.ResolveBugReport(2, 3, "Fixed in the next patch"); return db.GetBugReport(2); });
	for (int8_t filter = -1; filter <= 1; filter++) {
		Both("GetBugReportsTable filter " + std::to_string(filter), [&](GameDatabase& db) { return db.GetBugReportsTable(0, 10, "", 0, true, filter); });
	}
	Both("GetBugReportsTable search", [](GameDatabase& db) { return db.GetBugReportsTable(0, 10, "floor"); });
	Both("GetBugReportsTable search name", [](GameDatabase& db) { return db.GetBugReportsTable(0, 10, "Bob"); });
	Both("GetBugReportsTable search id", [](GameDatabase& db) { return db.GetBugReportsTable(0, 10, "3"); });
	for (uint32_t column = 0; column < 5; column++) {
		Both("GetBugReportsTable order " + std::to_string(column), [&](GameDatabase& db) { return db.GetBugReportsTable(0, 10, "", column, false); });
	}
	Both("GetBugReportsAfter", [](GameDatabase& db) { return db.GetBugReportsAfter(1, 10); });
	Both("GetMaxBugReportId", [](GameDatabase& db) { return db.GetMaxBugReportId(); });
	Both("GetBugReportsBy", [](GameDatabase& db) { return db.GetBugReportsBy(CHAR_ALICE, 10); });
	Both("DeleteBugReport", [](GameDatabase& db) { db.DeleteBugReport(3); return db.GetBugReportCount(); });
}

TEST_F(ParitySeeded, PetNames) {
	Both("SetPetNameModerationStatus", [](GameDatabase& db) {
		db.SetPetNameModerationStatus(PET1, { "Sparky", 1, CHAR_BOB, 3520 });
		db.SetPetNameModerationStatus(PET2, { "Sparky", 2, CHAR_ALICE, 12432 });
		db.SetPetNameModerationStatus(PET3, { "Rex", 1, 0 });
	});
	Both("SetPetNameModerationStatus keeps owner and lot", [](GameDatabase& db) { db.SetPetNameModerationStatus(PET1, { "Sparky", 1, 0 }); return db.GetPetNameInfo(PET1); });
	Both("SetPetLotIfMissing", [](GameDatabase& db) {
		db.SetPetLotIfMissing(PET2, 3520); // PET2 already has its LOT: kept
		db.SetPetLotIfMissing(PET3, 12434); // PET3 has none yet: written
		db.SetPetLotIfMissing(5, 3520); // no row: nothing
		return json{ db.GetPetNameInfo(PET2), db.GetPetNameInfo(PET3), db.GetPetNameInfo(5) };
	});
	Both("GetPetNameInfo", [](GameDatabase& db) { return json{ db.GetPetNameInfo(PET2), db.GetPetNameInfo(PET3), db.GetPetNameInfo(5) }; });
	Both("GetPetNamesTable", [](GameDatabase& db) { return db.GetPetNamesTable(0, 10); });
	Both("GetPetNamesTable pending", [](GameDatabase& db) { return db.GetPetNamesTable(0, 10, "", 0, true, true); });
	Both("GetPetNamesTable search", [](GameDatabase& db) { return db.GetPetNamesTable(0, 10, "rex"); });
	Both("GetPetNamesTable search owner", [](GameDatabase& db) { return db.GetPetNamesTable(0, 10, "Bob"); });
	for (uint32_t column = 0; column < 5; column++) {
		Both("GetPetNamesTable order " + std::to_string(column), [&](GameDatabase& db) { return db.GetPetNamesTable(0, 10, "", column, false); });
	}
	Both("GetPetsWithUnknownOwner", [](GameDatabase& db) { return db.GetPetsWithUnknownOwner(); });
	Both("GetAllPetNames", [](GameDatabase& db) { auto names = db.GetAllPetNames(); std::sort(names.begin(), names.end()); return names; });
	Both("ApprovePreviouslyApprovedPetNames", [](GameDatabase& db) { return db.ApprovePreviouslyApprovedPetNames(); });
	Both("SetPetOwner", [](GameDatabase& db) { db.SetPetOwner(PET3, 0); return json{ db.GetPetsWithUnknownOwner(), db.GetPetNameInfo(PET3) }; });
	Both("ApprovePetName", [](GameDatabase& db) { db.ApprovePetName(PET3); return db.GetPetNameInfo(PET3); });
	Both("RejectPetName", [](GameDatabase& db) { db.RejectPetName(PET3); return db.GetPetNameInfo(PET3); });
	Both("GetDashboardSnapshot", [](GameDatabase& db) { return db.GetDashboardSnapshot(); });
}

TEST_F(ParitySeeded, Properties) {
	Both("InsertNewProperty", [](GameDatabase& db) {
		IProperty::Info info{};
		// clone ids are the owners' charinfo.prop_clone_id (MySQL has a foreign key on it): Alice 1, AliceTwo 2, Bob 3
		info.id = PROP1; info.ownerId = CHAR_ALICE; info.cloneId = 1; info.name = "Alice's Castle"; info.description = "Big castle";
		db.InsertNewProperty(info, 1, LWOZONEID(1150, 0, 1));
		info.id = PROP2; info.ownerId = CHAR_BOB; info.cloneId = 3; info.name = "Bob's Hut"; info.description = "";
		db.InsertNewProperty(info, 1, LWOZONEID(1250, 0, 3));
		info.id = PROP3; info.ownerId = CHAR_ALICE2; info.cloneId = 2; info.name = "Second"; info.description = "another one";
		db.InsertNewProperty(info, 1, LWOZONEID(1150, 0, 2));
	});
	Both("GetPropertyInfo", [](GameDatabase& db) { return json{ db.GetPropertyInfo(PROP1), db.GetPropertyInfo(1250, 3), db.GetPropertyInfo(5) }; });
	Both("UpdatePropertyDetails", [](GameDatabase& db) {
		auto info = db.GetPropertyInfo(PROP1).value();
		info.privacyOption = 2; info.name = "Alice's Castle"; info.description = "Biggest castle";
		db.UpdatePropertyDetails(info);
		info = db.GetPropertyInfo(PROP2).value();
		info.privacyOption = 2;
		db.UpdatePropertyDetails(info);
		info = db.GetPropertyInfo(PROP3).value();
		info.privacyOption = 2;
		db.UpdatePropertyDetails(info);
	});
	Both("UpdatePropertyModerationInfo", [](GameDatabase& db) {
		auto info = db.GetPropertyInfo(PROP2).value();
		info.modApproved = 0; info.rejectionReason = "Rude name";
		db.UpdatePropertyModerationInfo(info);
		return db.GetPropertyInfo(PROP2);
	});
	Both("UpdatePerformanceCost", [](GameDatabase& db) { db.UpdatePerformanceCost(LWOZONEID(1150, 0, 1), 0.5f); return db.GetPropertyInfo(PROP1); });
	Both("GetPropertyCount", [](GameDatabase& db) { return db.GetPropertyCount(); });
	Both("GetPropertiesTable", [](GameDatabase& db) { return db.GetPropertiesTable(0, 10); });
	Both("GetPropertiesTable pending", [](GameDatabase& db) { return db.GetPropertiesTable(0, 10, "", 0, true, true); });
	Both("GetPropertiesTable search", [](GameDatabase& db) { return db.GetPropertiesTable(0, 10, "castle"); });
	Both("GetPropertiesTable search owner", [](GameDatabase& db) { return db.GetPropertiesTable(0, 10, "Bob"); });
	for (uint32_t column = 0; column < 8; column++) {
		Both("GetPropertiesTable order " + std::to_string(column), [&](GameDatabase& db) { return db.GetPropertiesTable(0, 10, "", column, false); });
	}
	Both("ApproveProperty", [](GameDatabase& db) { db.ApproveProperty(PROP1); db.ApproveProperty(PROP3); return db.GetPropertyInfo(PROP1); });
	Both("InsertNewPropertyModel", [](GameDatabase& db) {
		IPropertyContents::Model model;
		model.id = 1152921510000400001LL; model.lot = 14; model.ugcId = 0;
		db.InsertNewPropertyModel(PROP1, model, "Objects_14_name");
		model.id = 1152921510000400002LL;
		db.InsertNewPropertyModel(PROP1, model, "Objects_14_name");
	});
	Both("GetModelPropertyId", [](GameDatabase& db) { return json{ db.GetModelPropertyId(1152921510000400001LL), db.GetModelPropertyId(1) }; });
	for (const auto sort : { IProperty::ShowcaseSort::REPUTATION, IProperty::ShowcaseSort::NEWEST, IProperty::ShowcaseSort::NAME }) {
		Both("GetShowcaseProperties " + std::to_string(static_cast<int>(sort)), [&](GameDatabase& db) {
			IProperty::ShowcaseQuery query;
			query.sort = sort;
			return db.GetShowcaseProperties(query);
		});
	}
	Both("GetShowcaseProperties search", [](GameDatabase& db) { IProperty::ShowcaseQuery query; query.search = "alice"; query.zoneId = 1150; return db.GetShowcaseProperties(query); });
	Both("GetShowcaseProperties page", [](GameDatabase& db) { IProperty::ShowcaseQuery query; query.start = 1; query.length = 1; return db.GetShowcaseProperties(query); });
	Both("GetPropertiesOwnedBy", [](GameDatabase& db) { return db.GetPropertiesOwnedBy(CHAR_ALICE); });
	Both("GetPropertyRecord", [](GameDatabase& db) { return json{ db.GetPropertyRecord(PROP1), db.GetPropertyRecord(1) }; });
	Both("GetPropertyModelRecords", [](GameDatabase& db) { return db.GetPropertyModelRecords(PROP1); });
	Both("FixPropertyCloneIds", [](GameDatabase& db) { return db.FixPropertyCloneIds(); });
	Both("GetDashboardSnapshot", [](GameDatabase& db) { return db.GetDashboardSnapshot(); });
}

TEST_F(ParitySeeded, UgcModel) {
	Both("InsertNewUgcModel", [](GameDatabase& db) {
		std::stringstream data("sd0\x01\x02 compressed model");
		db.InsertNewUgcModel(data, 1152921510000500001LL, 1, CHAR_ALICE);
	});
	Both("GetUgcModel unplaced", [](GameDatabase& db) {
		auto model = db.GetUgcModel(1152921510000500001LL);
		return model ? json{ model->id, model->modelID, model->lxfmlData.str() } : json();
	});

	// The UGC server's processing state (processed_at is the time of the call, so only whether it's set is compared)
	const auto infoJson = [](const std::optional<IUgc::ProcessInfo>& info) {
		return info ? json{ info->id, info->characterId, info->characterName, static_cast<int32_t>(info->state), info->attempts,
			info->processedAt > 0, info->error, info->bakeAo, info->details } : json();
	};
	const auto countsJson = [](const std::vector<std::pair<IUgc::eProcessState, uint64_t>>& counts) {
		json out = json::array();
		for (const auto& [state, count] : counts) out.push_back({ static_cast<int32_t>(state), count });
		return out;
	};
	Both("GetUgcModelsToProcess", [](GameDatabase& db) {
		json out = json::array();
		for (const auto& model : db.GetUgcModelsToProcess(10)) out.push_back({ model.id, model.lxfml, model.attempts });
		return out;
	});
	Both("SetUgcModelProcessed failed", [&](GameDatabase& db) {
		db.SetUgcModelProcessed(1152921510000500001LL, IUgc::eProcessState::FAILED, 3, "no bricks", false);
		return json{ infoJson(db.GetUgcProcessInfo(1152921510000500001LL)), countsJson(db.GetUgcProcessCounts()), db.GetUgcModelsToProcess(10).size() };
	});
	Both("GetUgcProcessList", [&](GameDatabase& db) {
		json out = json::array();
		for (const auto& info : db.GetUgcProcessList(IUgc::eProcessState::FAILED, 0, 10)) out.push_back(infoJson(info));
		out.push_back(db.GetUgcProcessList(std::nullopt, 0, 10).size());
		out.push_back(db.GetUgcProcessList(IUgc::eProcessState::DONE, 0, 10).size());
		return out;
	});
	Both("ResetUgcModelProcessing", [&](GameDatabase& db) {
		const auto changed = db.ResetUgcModelProcessing(std::nullopt, true);
		db.SetUgcModelProcessed(1152921510000500001LL, IUgc::eProcessState::DONE, 1, "", true);
		return json{ changed, infoJson(db.GetUgcProcessInfo(1152921510000500001LL)), infoJson(db.GetUgcProcessInfo(1)) };
	});
	Both("Modular build processing", [&](GameDatabase& db) {
		db.InsertUgcBuild("1:4713+1:4714+1:4715", 1152921510000500002LL, CHAR_ALICE);
		json out = json::array();
		for (const auto& build : db.GetModularBuildsToProcess(10)) out.push_back({ build.id, build.modules, build.attempts });
		db.SetModularBuildProcessed(1152921510000500002LL, IUgc::eProcessState::PENDING, 1, "no mesh");
		out.push_back(infoJson(db.GetModularBuildProcessInfo(1152921510000500002LL)));
		out.push_back(countsJson(db.GetModularBuildProcessCounts()));
		out.push_back(db.GetModularBuildProcessList(std::nullopt, 0, 10).size());
		out.push_back(db.ResetModularBuildProcessing(1152921510000500002LL, false));
		out.push_back(infoJson(db.GetModularBuildProcessInfo(1152921510000500002LL)));
		return out;
	});
}

TEST_F(ParitySeeded, LogsAndAudit) {
	Both("UpdateActivityLog", [](GameDatabase& db) {
		db.UpdateActivityLog(CHAR_ALICE, eActivityType::PlayerLoggedIn, 1000);
		db.UpdateActivityLog(CHAR_ALICE, eActivityType::PlayerChangedZone, 1100);
		db.UpdateActivityLog(CHAR_BOB, eActivityType::PlayerLoggedOut, 1200);
	});
	Both("GetActivityLogCount", [](GameDatabase& db) { return db.GetActivityLogCount(); });
	Both("GetActivityLogTable", [](GameDatabase& db) { return db.GetActivityLogTable(0, 10); });
	Both("GetActivityLogTable search", [](GameDatabase& db) { return db.GetActivityLogTable(0, 10, "Bob"); });
	for (uint32_t column = 0; column < 5; column++) {
		Both("GetActivityLogTable order " + std::to_string(column), [&](GameDatabase& db) { return db.GetActivityLogTable(0, 10, "", column, false); });
	}
	Both("InsertSlashCommandUsage", [](GameDatabase& db) { db.InsertSlashCommandUsage(CHAR_GM, "/gmlevel 9"); db.InsertSlashCommandUsage(CHAR_ALICE, "/fly"); });
	Both("GetCommandLogTable", [](GameDatabase& db) { return db.GetCommandLogTable(0, 10); });
	Both("GetCommandLogTable search", [](GameDatabase& db) { return db.GetCommandLogTable(0, 10, "fly"); });
	for (uint32_t column = 0; column < 4; column++) {
		Both("GetCommandLogTable order " + std::to_string(column), [&](GameDatabase& db) { return db.GetCommandLogTable(0, 10, "", column, true); });
	}
	Both("InsertAuditLog", [](GameDatabase& db) {
		db.InsertAuditLog(3, "gm", "ban", "Banned bob", 2, 0);
		db.InsertAuditLog(3, "gm", "rename", "Renamed AliceTwo", 1, CHAR_ALICE2);
		db.InsertAuditLog(3, "gm", "settings", "Changed a setting", 0, 0);
	});
	Both("GetAuditLogTable", [](GameDatabase& db) { return db.GetAuditLogTable(0, 10); });
	Both("GetAuditLogTable search", [](GameDatabase& db) { return db.GetAuditLogTable(0, 10, "bob"); });
	for (uint32_t column = 0; column < 5; column++) {
		Both("GetAuditLogTable order " + std::to_string(column), [&](GameDatabase& db) { return db.GetAuditLogTable(0, 10, "", column, true); });
	}
	Both("GetAuditAbout", [](GameDatabase& db) { return json{ db.GetAuditAbout(1, 10), db.GetAuditAbout(2, 10) }; });
	Both("InsertCheatDetection", [](GameDatabase& db) { db.InsertCheatDetection({ 2, "bob", "127.0.0.1:1234", "speed hack" }); });
	Both("GetCheatDetectionsFor", [](GameDatabase& db) { return db.GetCheatDetectionsFor(2, 10); });
	Both("PruneLog", [](GameDatabase& db) {
		json out = json::array();
		for (const auto log : { IDashboardAdmin::eLog::ACTIVITY, IDashboardAdmin::eLog::COMMAND, IDashboardAdmin::eLog::AUDIT,
			IDashboardAdmin::eLog::CHEAT_DETECTION, IDashboardAdmin::eLog::CHAT, IDashboardAdmin::eLog::LOGIN_ADDRESS }) {
			out.push_back(db.PruneLog(log, 1000)); // nothing is that old
		}
		return out;
	});
	Both("PruneLog cheat detections", [](GameDatabase& db) { return db.PruneLog(IDashboardAdmin::eLog::CHEAT_DETECTION, Now() + 86400); });
	Both("GetDashboardSnapshot", [](GameDatabase& db) { return db.GetDashboardSnapshot(); });
}

TEST_F(ParitySeeded, Leaderboard) {
	Both("SaveScore", [](GameDatabase& db) {
		db.SaveScore(CHAR_ALICE, 1864, { 100.0f, 2.0f, 0.0f });
		db.SaveScore(CHAR_BOB, 1864, { 50.0f, 1.0f, 0.0f });
		db.SaveScore(CHAR_ALICE, 5, { 1.5f, 0.0f, 0.0f });
	});
	Both("GetLeaderboardSizes", [](GameDatabase& db) { return db.GetLeaderboardSizes(); });
	Both("GetDescendingLeaderboard", [](GameDatabase& db) { return db.GetDescendingLeaderboard(1864); });
	Both("GetRankedLeaderboard", [](GameDatabase& db) {
		json out = json::array();
		// A foot race keeps the time left (more first); a monument race the time taken (less first)
		for (const auto type : { eLeaderboardType::FootRace, eLeaderboardType::MonumentRace, eLeaderboardType::SurvivalNS }) {
			json names = json::array();
			for (const auto& entry : db.GetRankedLeaderboard(type, 1864)) names.push_back(entry.name);
			out.push_back(names);
		}
		EXPECT_EQ(out[0], json({ "Alice", "Bob" }));
		EXPECT_EQ(out[1], json({ "Bob", "Alice" }));
		return out;
	});
	Both("DeleteLeaderboardScore", [](GameDatabase& db) { db.DeleteLeaderboardScore(CHAR_BOB, 1864); return db.GetLeaderboardSizes(); });
	Both("ResetLeaderboard", [](GameDatabase& db) { db.ResetLeaderboard(5); return db.GetLeaderboardSizes(); });
}

TEST_F(ParitySeeded, Mail) {
	Both("InsertNewMail", [](GameDatabase& db) {
		MailInfo mail;
		mail.senderUsername = "Alice"; mail.recipient = "Bob"; mail.subject = "Gift"; mail.body = "For you";
		mail.senderId = CHAR_ALICE; mail.receiverId = CHAR_BOB; mail.timeSent = 1700000000;
		mail.itemID = 1152921510000600001LL; mail.itemLOT = 6086; mail.itemCount = 3; mail.itemSubkey = 1152921510000600009LL;
		mail.itemConfig = "assemblyPartLOTs=0:1;2;3";
		db.InsertNewMail(mail);
		mail.itemID = 0; mail.itemLOT = 0; mail.itemCount = 0; mail.itemSubkey = 0; mail.itemConfig = ""; mail.subject = "Hi";
		db.InsertNewMail(mail);
	});
	Both("GetMail", [](GameDatabase& db) { return json{ db.GetMail(1), db.GetMail(9) }; });
	Both("GetMailForPlayer", [](GameDatabase& db) { return db.GetMailForPlayer(CHAR_BOB, 20); });
	Both("ForEachMailAttachment", [](GameDatabase& db) {
		json rows = json::array();
		db.ForEachMailAttachment([&](const IEconomyLedger::MailAttachment& attachment) { rows.push_back(attachment); });
		return rows;
	});
	Both("GetDashboardSnapshot", [](GameDatabase& db) { return db.GetDashboardSnapshot(); });
}

TEST_F(ParitySeeded, Friends) {
	Both("AddFriend", [](GameDatabase& db) { db.AddFriend(CHAR_ALICE, CHAR_BOB); db.SetBestFriendStatus(CHAR_ALICE, CHAR_BOB, 3); });
	Both("GetFriendsOf", [](GameDatabase& db) { return json{ db.GetFriendsOf(CHAR_ALICE), db.GetFriendsOf(CHAR_BOB) }; });
}

TEST_F(ParitySeeded, Economy) {
	const uint32_t day = 20000;
	Both("RecordEconomy", [&](GameDatabase& db) {
		db.RecordEconomy(
			{ { day, CHAR_ALICE, 1, 500, 20 }, { day, CHAR_BOB, 1, 100, 0 }, { day, CHAR_GM, 1, 99999, 0 }, { day, CHAR_ALICE, 3, 50, 0 }, { day + 1, CHAR_ALICE, 1, 10, 5 } },
			{ { day, CHAR_ALICE, 2, 300, 0 }, { day, CHAR_BOB, 2, 30, 10 } },
			{ { day, 6086, 1, false, 5, 1 }, { day, 6086, 1, true, 50, 0 }, { day, 14, 2, false, 2, 2 } },
			{ { 1700000000, IEconomyLedger::eTransferMethod::TRADE, 1152921510000700001LL, 1152921510000700001LL, 6086, 1, 0, CHAR_ALICE, CHAR_BOB, 1100 },
			  { 1700000100, IEconomyLedger::eTransferMethod::MAIL_SENT, 0, 0, 0, 0, 250, CHAR_BOB, CHAR_ALICE, 1100 } },
			{ { day, 1100, 0, IEconomyLedger::eMapEvent::ENEMY_KILLS, 4712, 3, -2, 1, 0 }, { day, 1100, 0, IEconomyLedger::eMapEvent::ENEMY_KILLS, 4712, 3, -2, 1, 0 },
			  { day, 1100, 0, IEconomyLedger::eMapEvent::COIN_DROPS, 0, 1, 1, 1, 12 }, { day + 1, 1200, 0, IEconomyLedger::eMapEvent::ITEM_DROPS, 6086, 0, 0, 1, 1 },
			  // Two properties on one property zone (clones 1 and 3), plus a row from before clones were recorded (0)
			  { day, 1150, 1, IEconomyLedger::eMapEvent::ENEMY_KILLS, 4712, 3, -2, 1, 0 }, { day, 1150, 3, IEconomyLedger::eMapEvent::ENEMY_KILLS, 4712, 3, -2, 2, 0 },
			  { day, 1150, 0, IEconomyLedger::eMapEvent::ENEMY_KILLS, 4712, 3, -2, 1, 0 },
			  { day, 1150, 1, IEconomyLedger::eMapEvent::POWERUP_DROPS, 177, 0, 0, 3, 3 }, { day + 1, 1150, 1, IEconomyLedger::eMapEvent::POWERUP_DROPS, 935, 0, 0, 1, 1 },
			  { day, 1100, 0, IEconomyLedger::eMapEvent::POWERUP_PICKUPS, 177, 0, 0, 2, 2 } },
			{ { day, 1100, 0, 1, false, 7 }, { day, 1100, 0, 1, true, 3 }, { day, 1200, 0, 2, false, 1 }, { day, 1150, 1, 1, false, 4 }, { day, 1150, 3, 1, false, 5 } });
		// Recording the same again adds to the daily rows
		db.RecordEconomy({ { day, CHAR_ALICE, 1, 500, 20 } }, {}, { { day, 6086, 1, false, 5, 1 } }, {}, { { day, 1150, 1, IEconomyLedger::eMapEvent::ENEMY_KILLS, 4712, 3, -2, 1, 0 } },
			{ { day, 1100, 0, 1, false, 7 }, { day, 1150, 1, 1, false, 1 } });
	});
	for (const bool excludeStaff : { false, true }) {
		const auto suffix = excludeStaff ? " excluding staff" : "";
		Both(std::string("GetCurrencyFlows") + suffix, [&](GameDatabase& db) { return db.GetCurrencyFlows(day, day + 1, excludeStaff); });
		Both(std::string("GetUScoreFlows") + suffix, [&](GameDatabase& db) { return db.GetUScoreFlows(day, day + 1, excludeStaff); });
		Both(std::string("GetItemFlows") + suffix, [&](GameDatabase& db) { return json{ db.GetItemFlows(day, day + 1, 0, excludeStaff), db.GetItemFlows(day, day + 1, 6086, excludeStaff) }; });
		Both(std::string("GetTopEarners") + suffix, [&](GameDatabase& db) { return db.GetTopEarners(day, day + 1, 10, excludeStaff); });
		Both(std::string("GetTopItems") + suffix, [&](GameDatabase& db) { return db.GetTopItems(day, day + 1, 10, excludeStaff); });
		Both(std::string("GetPlayerStatsPerDay") + suffix, [&](GameDatabase& db) {
			return json{ db.GetPlayerStatsPerDay(day, day + 1, excludeStaff, {}), db.GetPlayerStatsPerDay(day, day + 1, excludeStaff, { { 1150 }, std::nullopt }),
				db.GetPlayerStatsPerDay(day, day + 1, excludeStaff, { { 1150 }, 3u }), db.GetPlayerStatsPerDay(day, day + 1, excludeStaff, { { 1100, 1200 }, 0u }) };
		});
		Both(std::string("GetPlayerStatsPerZone") + suffix, [&](GameDatabase& db) { return db.GetPlayerStatsPerZone(day, day + 1, excludeStaff); });
	}
	Both("GetTransfers", [](GameDatabase& db) { return json{ db.GetTransfers(0, 10, 0, 0), db.GetTransfers(0, 10, CHAR_ALICE, 0), db.GetTransfers(0, 10, 0, 6086), db.GetTransfers(1, 1, 0, 0) }; });
	// A chain: traded, mailed, claimed into an existing stack, moved to the vault. Inventory moves are left out of the
	// trade and mail lists but are part of an item's transfers.
	Both("RecordEconomy chain", [](GameDatabase& db) {
		using enum IEconomyLedger::eTransferMethod;
		db.RecordEconomy({}, {}, {}, {
			{ 1700000200, TRADE, 1152921510000700010LL, 1152921510000700011LL, 6086, 1, 0, CHAR_ALICE, CHAR_BOB, 1100 },
			{ 1700000300, MAIL_SENT, 1152921510000700011LL, 1152921510000700012LL, 6086, 1, 0, CHAR_BOB, CHAR_ALICE2, 1100 },
			{ 1700000400, MAIL_CLAIMED, 1152921510000700012LL, 1152921510000700013LL, 6086, 1, 0, CHAR_BOB, CHAR_ALICE2, 1200, true },
			{ 1700000500, INVENTORY_MOVE, 1152921510000700013LL, 1152921510000700014LL, 6086, 1, 0, CHAR_ALICE2, CHAR_ALICE2, 1200 } }, {}, {});
	});
	Both("GetTransfersForItems", [](GameDatabase& db) {
		return json{ db.GetTransfersForItems({ 1152921510000700001LL }), db.GetTransfersForItems({ 1152921510000700011LL, 1152921510000700013LL }),
			db.GetTransfersForItems({}), db.GetTransfersForItems({ 42 }) };
	});
	Both("GetTransfers without inventory moves", [](GameDatabase& db) { return json{ db.GetTransfers(0, 10, 0, 0), db.GetTransfers(0, 10, CHAR_ALICE2, 0) }; });
	Both("GetTransfersForCharacters", [](GameDatabase& db) { return json{ db.GetTransfersForCharacters({ CHAR_ALICE, CHAR_ALICE2 }, 0, 10), db.GetTransfersForCharacters({}, 0, 10) }; });
	Both("GetMapZones", [&](GameDatabase& db) { return json{ db.GetMapZones(IEconomyLedger::eMapEvent::ENEMY_KILLS, day, day + 1), db.GetMapZones(IEconomyLedger::eMapEvent::POWERUP_DROPS, day, day + 1) }; });
	Both("GetMapZonesAllKinds", [&](GameDatabase& db) { return json{ db.GetMapZonesAllKinds(day, day + 1), db.GetMapZonesAllKinds(day + 5, day + 6) }; });
	Both("GetMapLots", [&](GameDatabase& db) {
		return json{ db.GetMapLots(1100, std::nullopt, IEconomyLedger::eMapEvent::ENEMY_KILLS, day, day + 1, 10), db.GetMapLots(1150, std::nullopt, IEconomyLedger::eMapEvent::ENEMY_KILLS, day, day + 1, 10),
			db.GetMapLots(1150, 3u, IEconomyLedger::eMapEvent::ENEMY_KILLS, day, day + 1, 10), db.GetMapLots(1150, 0u, IEconomyLedger::eMapEvent::ENEMY_KILLS, day, day + 1, 10) };
	});
	Both("GetMapCells", [&](GameDatabase& db) {
		return json{ db.GetMapCells(1100, std::nullopt, IEconomyLedger::eMapEvent::ENEMY_KILLS, day, day + 1, 0), db.GetMapCells(1100, std::nullopt, IEconomyLedger::eMapEvent::COIN_DROPS, day, day, 0),
			db.GetMapCells(1100, std::nullopt, IEconomyLedger::eMapEvent::ENEMY_KILLS, day, day, 4712), db.GetMapCells(1150, 1u, IEconomyLedger::eMapEvent::ENEMY_KILLS, day, day, 0),
			db.GetMapCells(1150, std::nullopt, IEconomyLedger::eMapEvent::ENEMY_KILLS, day, day, 0) };
	});
	Both("GetMapEventsPerDay", [&](GameDatabase& db) {
		return json{ db.GetMapEventsPerDay(day, day + 1, {}), db.GetMapEventsPerDay(day, day + 1, { { 1150 }, std::nullopt }), db.GetMapEventsPerDay(day, day + 1, { { 1150 }, 1u }),
			db.GetMapEventsPerDay(day, day + 1, { {}, 0u }) };
	});
	Both("GetMapEventsByLot", [&](GameDatabase& db) {
		return json{ db.GetMapEventsByLot(IEconomyLedger::eMapEvent::POWERUP_DROPS, day, day + 1, {}, 100), db.GetMapEventsByLot(IEconomyLedger::eMapEvent::POWERUP_DROPS, day, day + 1, { { 1150 }, 1u }, 100),
			db.GetMapEventsByLot(IEconomyLedger::eMapEvent::POWERUP_PICKUPS, day, day + 1, { { 1150 }, std::nullopt }, 100), db.GetMapEventsByLot(IEconomyLedger::eMapEvent::POWERUP_DROPS, day, day + 1, {}, 1) };
	});
	Both("GetCloneOwners", [](GameDatabase& db) { return json{ db.GetCloneOwners({ 1, 2, 3, 999 }), db.GetCloneOwners({}) }; });
	Both("GetDailyIncome", [&](GameDatabase& db) { auto rows = db.GetDailyIncome(day); std::sort(rows.begin(), rows.end()); return rows; });
	Both("GetItemCreationTotals", [&](GameDatabase& db) { return db.GetItemCreationTotals(day, day + 1); });

	// Economy flags
	Both("InsertEconomyFlag", [&](GameDatabase& db) {
		return json{
			db.InsertEconomyFlag({ day, IDashboardAdmin::eFlagKind::COIN_INCOME, CHAR_ALICE, 0, 0, 1000, 100, "10x the usual" }),
			db.InsertEconomyFlag({ day, IDashboardAdmin::eFlagKind::COIN_INCOME, CHAR_ALICE, 0, 0, 1000, 100, "again" }),
			db.InsertEconomyFlag({ 0, IDashboardAdmin::eFlagKind::DUPLICATE, CHAR_BOB, 6086, 1152921510000700001LL, 2, 1, "" }),
		};
	});
	Both("GetOpenEconomyFlagCount", [](GameDatabase& db) { return db.GetOpenEconomyFlagCount(); });
	Both("ReviewEconomyFlag", [](GameDatabase& db) { db.ReviewEconomyFlag(1, IDashboardAdmin::eFlagStatus::DISMISSED, 3, "fine"); return db.GetOpenEconomyFlagCount(); });
	for (const int32_t status : { -1, 0, 1 }) {
		Both("GetEconomyFlagsTable " + std::to_string(status), [&](GameDatabase& db) { return db.GetEconomyFlagsTable(0, 10, status); });
	}
	Both("GetEconomyFlagsFor", [](GameDatabase& db) { return json{ db.GetEconomyFlagsFor(CHAR_ALICE, 10), db.GetEconomyFlagsFor(CHAR_BOB, 10) }; });
	Both("GetDashboardSnapshot", [](GameDatabase& db) { return db.GetDashboardSnapshot(); });

	// Compaction: days 20000/20001 are 2024-10-04/05, so both merge into 2024-10-01 (day 19997)
	const auto month = IDashboardAdmin::MonthStartDay(day);
	EXPECT_EQ(month, 19997u);
	Both("CompactEconomy", [&](GameDatabase& db) { return db.CompactEconomy(day + 2, day + 2); });
	Both("GetCurrencyFlows after compaction", [&](GameDatabase& db) { return db.GetCurrencyFlows(month, day + 1, false); });
	Both("GetItemFlows after compaction", [&](GameDatabase& db) { return db.GetItemFlows(month, day + 1, 0, false); });
	Both("GetPlayerStatsPerDay after compaction", [&](GameDatabase& db) { return json{ db.GetPlayerStatsPerDay(month, day + 1, false, {}), db.GetPlayerStatsPerDay(month, day + 1, false, { { 1150 }, 1u }) }; });
	Both("GetPlayerStatsPerZone after compaction", [&](GameDatabase& db) { return db.GetPlayerStatsPerZone(month, day + 1, false); });
	Both("GetMapEventsPerDay after compaction", [&](GameDatabase& db) { return json{ db.GetMapEventsPerDay(month, day + 1, {}), db.GetMapEventsPerDay(month, day + 1, { { 1150 }, 3u }) }; });
	Both("GetMapZones after compaction", [&](GameDatabase& db) { return db.GetMapZones(IEconomyLedger::eMapEvent::ENEMY_KILLS, month, day + 1); });
	Both("CompactEconomy again", [&](GameDatabase& db) { return db.CompactEconomy(day + 2, day + 2); });
	Both("PruneTransfers", [](GameDatabase& db) { return db.PruneTransfers(1700000050); });
	Both("GetTransfers after prune", [](GameDatabase& db) { return db.GetTransfers(0, 10, 0, 0); });
}

TEST_F(ParitySeeded, DashboardAdmin) {
	Both("InsertWebhook", [](GameDatabase& db) {
		IDashboardAdmin::Webhook hook;
		hook.name = "Discord"; hook.url = "https://discord.example/hook"; hook.format = "discord"; hook.events = "*"; hook.secret = "";
		db.InsertWebhook(hook);
		hook.name = "Json"; hook.url = "https://json.example"; hook.format = "json"; hook.events = "ban,mute"; hook.secret = "s3cret"; hook.enabled = false;
		db.InsertWebhook(hook);
	});
	Both("GetWebhooks", [](GameDatabase& db) { return db.GetWebhooks(); });
	Both("UpdateWebhook", [](GameDatabase& db) { auto hook = db.GetWebhook(2).value(); hook.enabled = true; hook.events = "*"; db.UpdateWebhook(hook); return db.GetWebhook(2); });
	Both("RecordWebhookResult", [](GameDatabase& db) { db.RecordWebhookResult(1, 1700000000, 404, "Not Found"); return db.GetWebhook(1); });
	Both("DeleteWebhook", [](GameDatabase& db) { db.DeleteWebhook(2); return json{ db.GetWebhook(2), db.GetWebhooks().size() }; });
	Both("DashboardState", [](GameDatabase& db) {
		json out = json::array();
		out.push_back(db.GetDashboardState("restart"));
		db.SetDashboardState("restart", "1700000000");
		db.SetDashboardState("restart", "1700000500");
		out.push_back(db.GetDashboardState("restart"));
		db.DeleteDashboardState("restart");
		out.push_back(db.GetDashboardState("restart"));
		return out;
	});
	Both("DashboardPreferences", [](GameDatabase& db) {
		json out = json::array();
		out.push_back(db.GetDashboardPreferences(1));
		db.SetDashboardPreferences(1, R"({"showStaff":true})");
		db.SetDashboardPreferences(1, R"({"showStaff":false,"terrain":true})");
		out.push_back(db.GetDashboardPreferences(1));
		return out;
	});
	Both("Totp", [](GameDatabase& db) {
		json out = json::array();
		out.push_back(db.GetTotp(1));
		db.SetTotp(1, "encrypted", 1700000000);
		out.push_back(db.GetTotp(1));
		out.push_back(db.UseTotpStep(1, 100));
		out.push_back(db.UseTotpStep(1, 100));
		out.push_back(db.UseTotpStep(1, 99));
		out.push_back(db.UseTotpStep(1, 101));
		out.push_back(db.GetTotp(1));
		out.push_back(db.GetTotp(999));
		return out;
	});
	Both("RecoveryCodes", [](GameDatabase& db) {
		json out = json::array();
		db.ReplaceRecoveryCodes(1, { "h1", "h2", "h3" });
		out.push_back(db.GetRecoveryCodesLeft(1));
		out.push_back(db.UseRecoveryCode(1, "h2"));
		out.push_back(db.UseRecoveryCode(1, "h2"));
		out.push_back(db.UseRecoveryCode(2, "h1"));
		out.push_back(db.GetRecoveryCodesLeft(1));
		db.ReplaceRecoveryCodes(1, { "h4" });
		out.push_back(db.GetRecoveryCodesLeft(1));
		return out;
	});
}

TEST_F(ParitySeeded, ServerConfig) {
	Both("ReportConfigFromFile", [](GameDatabase& db) {
		db.ReportConfigFromFile({ "sharedconfig.ini", "max_clients", "999", "file", std::nullopt, false, false, "Most players", Now(), 0, "" });
		db.ReportConfigFromFile({ "sharedconfig.ini", "mysql_password", std::nullopt, "file", std::nullopt, false, true, "", Now(), 0, "" });
		db.ReportConfigFromFile({ "worldconfig.ini", "disable_chat", "0", "env", std::nullopt, false, false, "", Now(), 0, "" });
	});
	Both("SetWebConfigValue", [](GameDatabase& db) {
		db.SetWebConfigValue("sharedconfig.ini", "max_clients", "50", true, "gm");
		db.SetWebConfigValue("chatconfig.ini", "new_key", "x", false, "gm");
		db.SetWebConfigValue("worldconfig.ini", "disable_chat", std::nullopt, false, "gm");
	});
	Both("ReportConfigFromFile keeps web value", [](GameDatabase& db) {
		db.ReportConfigFromFile({ "sharedconfig.ini", "max_clients", "1000", "file", std::nullopt, false, false, "Most players", Now(), 0, "" });
	});
	Both("GetServerConfig", [](GameDatabase& db) { return db.GetServerConfig({}); });
	Both("GetServerConfig files", [](GameDatabase& db) { return db.GetServerConfig({ "sharedconfig.ini", "chatconfig.ini" }); });
	Both("DeleteServerConfig", [](GameDatabase& db) { db.DeleteServerConfig("chatconfig.ini", "new_key"); return db.GetServerConfig({ "chatconfig.ini" }); });
	Both("InsertSettingChange", [](GameDatabase& db) {
		IServerConfig::SettingChange change;
		change.file = "sharedconfig.ini"; change.name = "max_clients"; change.oldValue = std::nullopt; change.newValue = "50"; change.newWebWins = true;
		change.fileValue = "999"; change.changedAt = 1700000000; change.accountId = 3; change.changedBy = "gm";
		json ids = json::array();
		ids.push_back(db.InsertSettingChange(change));
		change.oldValue = "50"; change.newValue = std::nullopt; change.oldWebWins = true; change.newWebWins = false; change.revertOf = 1; change.changedAt = 1700000100;
		ids.push_back(db.InsertSettingChange(change));
		change.name = "mysql_password"; change.secret = true; change.removed = true; change.oldValue = std::nullopt; change.fileValue = std::nullopt; change.changedAt = 1700000200;
		ids.push_back(db.InsertSettingChange(change));
		return ids;
	});
	Both("GetSettingChanges", [](GameDatabase& db) {
		json out = json::array();
		uint64_t total = 0;
		out.push_back(db.GetSettingChanges("", "", 0, 10, total)); out.push_back(total);
		out.push_back(db.GetSettingChanges("sharedconfig.ini", "max_clients", 0, 1, total)); out.push_back(total);
		out.push_back(db.GetSettingChanges("worldconfig.ini", "", 0, 10, total)); out.push_back(total);
		return out;
	});
	Both("GetSettingChange", [](GameDatabase& db) { return json{ db.GetSettingChange(2), db.GetSettingChange(99) }; });
}

TEST_F(ParitySeeded, ScheduledTasks) {
	Both("GetScheduledTasks empty", [](GameDatabase& db) { return db.GetScheduledTasks(); });
	Both("SetScheduledTask", [](GameDatabase& db) {
		db.SetScheduledTask("prune_logs", "0 4 * * *", true, "gm");
		db.SetScheduledTask("economy_check", std::nullopt, false, "gm");
		db.SetTaskLastScheduled("prune_logs", 1700000000);
		db.SetTaskLastScheduled("snapshots", 1700000500); // no row yet
		return db.GetScheduledTasks();
	});
	Both("InsertTaskRun", [](GameDatabase& db) {
		db.InsertTaskRun({ 0, "prune_logs", "schedule", "", 1700000000, 1700000005, IScheduledTasks::eRunStatus::SUCCEEDED, "Deleted 5 rows", "line 1\nline 2" });
		db.InsertTaskRun({ 0, "prune_logs", "manual", "gm", 1700001000, 1700001001, IScheduledTasks::eRunStatus::FAILED, "Oops", "" });
		db.InsertTaskRun({ 0, "economy_check", "schedule", "", 1600000000, 1600000001, IScheduledTasks::eRunStatus::TIMED_OUT, "", "" });
	});
	Both("GetTaskRunsTable", [](GameDatabase& db) { return json{ db.GetTaskRunsTable("", 0, 10), db.GetTaskRunsTable("prune_logs", 1, 10) }; });
	Both("GetTaskRun", [](GameDatabase& db) { return json{ db.GetTaskRun(1), db.GetTaskRun(99) }; });
	Both("GetLatestTaskRuns", [](GameDatabase& db) { return db.GetLatestTaskRuns(); });
	Both("PruneTaskRuns", [](GameDatabase& db) { return db.PruneTaskRuns(1650000000); });
}

TEST_F(ParitySeeded, CharacterSnapshots) {
	std::string binary("x\x9c\x00\x01\x02\xff\xfe compressed\x00tail", 24);
	Both("InsertCharacterSnapshot", [&](GameDatabase& db) {
		db.InsertCharacterSnapshot({ 0, CHAR_ALICE, 1600000000, "daily", "", 100, std::string(64, 'a'), binary });
		db.InsertCharacterSnapshot({ 0, CHAR_ALICE, 1700000000, "before edit", "gm", 120, std::string(64, 'b'), binary + "more" });
		db.InsertCharacterSnapshot({ 0, CHAR_BOB, 1600000000, "daily", "", 50, std::string(64, 'c'), binary });
	});
	Both("GetCharacterSnapshots", [](GameDatabase& db) { return db.GetCharacterSnapshots(CHAR_ALICE); });
	Both("GetCharacterSnapshot", [](GameDatabase& db) { return json{ db.GetCharacterSnapshot(1), db.GetCharacterSnapshot(99) }; });
	auto snapshot = g_Backends[1].db->GetCharacterSnapshot(1);
	ASSERT_TRUE(snapshot);
	EXPECT_EQ(snapshot->compressed, binary) << "binary data must survive the round trip";
	Both("GetLatestSnapshotHashes", [](GameDatabase& db) { return db.GetLatestSnapshotHashes(); });
	Both("PruneCharacterSnapshots", [](GameDatabase& db) { return db.PruneCharacterSnapshots(1650000000, 1); });
	Both("GetCharacterSnapshots after prune", [](GameDatabase& db) { return json{ db.GetCharacterSnapshots(CHAR_ALICE), db.GetCharacterSnapshots(CHAR_BOB) }; });
}

TEST_F(ParitySeeded, AccountNotesAndStrikes) {
	Both("InsertAccountNote", [](GameDatabase& db) {
		db.InsertAccountNote({ 0, 2, "note", "Was rude", "gm", 1700000000 });
		db.InsertAccountNote({ 0, 2, "warning", "Stop it", "gm", 1700000100 });
		db.InsertAccountNote({ 0, 2, "warning", "Last warning", "gm", 1700000200 });
	});
	Both("GetAccountNotes", [](GameDatabase& db) { return db.GetAccountNotes(2); });
	Both("GetAccountWarningCount", [](GameDatabase& db) { return db.GetAccountWarningCount(2); });
	Both("DeleteAccountNote", [](GameDatabase& db) { db.DeleteAccountNote(1); return db.GetAccountNotes(2); });
	Both("SetAccountBan", [](GameDatabase& db) { db.SetAccountBan(2, true, 1700000000, "Temp ban"); return db.GetAccountInfo("bob"); });
	Both("SetAccountBan permanent", [](GameDatabase& db) { db.SetAccountBan(1, true, 0, "Forever"); return db.GetAccountById(1); });
	Both("LiftExpiredBans", [](GameDatabase& db) { return db.LiftExpiredBans(1700000500); });
	Both("After LiftExpiredBans", [](GameDatabase& db) { return json{ db.GetAccountInfo("bob"), db.GetAccountInfo("alice") }; });
	Both("SetAccountBan off", [](GameDatabase& db) { db.SetAccountBan(1, false, 0, ""); return db.GetAccountInfo("alice"); });

	Both("InsertStrike", [](GameDatabase& db) {
		json ids = json::array();
		ids.push_back(db.InsertStrike({ 0, 2, CHAR_BOB, "NAME", "BadName", "Offensive", 3, "gm", 1700000000, 0, "", "" }));
		ids.push_back(db.InsertStrike({ 0, 2, 0, "MANUAL", "", "Spam", 3, "gm", 1700000100, 0, "", "" }));
		ids.push_back(db.InsertStrike({ 0, 1, CHAR_ALICE, "PET_NAME", "Rex", "Rude", 3, "gm", 1600000000, 0, "", "" }));
		return ids;
	});
	Both("GetStrikes", [](GameDatabase& db) { return db.GetStrikes(2); });
	Both("GetStrike", [](GameDatabase& db) { return json{ db.GetStrike(1), db.GetStrike(99) }; });
	Both("RevokeStrike", [](GameDatabase& db) { db.RevokeStrike(2, "admin", "mistake", 1700000200); return db.GetStrike(2); });
	Both("CountActiveStrikes", [](GameDatabase& db) { return json{ db.CountActiveStrikes(2, 0), db.CountActiveStrikes(2, 1700000050), db.CountActiveStrikes(1, 1650000000) }; });
	Both("SetStrikeStep", [](GameDatabase& db) { db.SetStrikeStep(1, "WARN", 1); db.SetStrikeStep(2, "MUTE", 2); });
	Both("GetAppliedStrikeSteps", [](GameDatabase& db) { return json{ db.GetAppliedStrikeSteps(2, 0), db.GetAppliedStrikeSteps(2, 1700000050), db.GetAppliedStrikeSteps(1, 0) }; });
}

TEST_F(ParitySeeded, ApiKeys) {
	Both("InsertApiKey", [](GameDatabase& db) {
		json ids = json::array();
		ids.push_back(db.InsertApiKey({ 0, 2, "bot", "chat bridge", std::string(64, 'a'), "dlk_aaaa", "chat_view,players_view", true, "10.0.0.", "/api/chat", 60, 1000,
			1700000000, "bob", 1700000000, 1800000000 }));
		ids.push_back(db.InsertApiKey({ 0, 2, "all", "", std::string(64, 'b'), "dlk_bbbb", "*", false, "", "", 0, 0, 1700000100, "bob", 1700000100, 0 }));
		ids.push_back(db.InsertApiKey({ 0, 1, "alice", "", std::string(64, 'c'), "dlk_cccc", "own_characters", false, "", "", 0, 0, 1700000200, "alice", 1700000200, 0 }));
		return ids;
	});
	Both("GetApiKeys", [](GameDatabase& db) { return db.GetApiKeys(2); });
	Both("GetApiKey", [](GameDatabase& db) { return json{ db.GetApiKey(1), db.GetApiKey(99) }; });
	Both("GetApiKeyByHash", [](GameDatabase& db) { return json{ db.GetApiKeyByHash(std::string(64, 'b')), db.GetApiKeyByHash("nope") }; });
	Both("RecordApiKeyUsage", [](GameDatabase& db) {
		db.RecordApiKeyUsage({ { 1, 5, 1700000500, "10.0.0.2", 19675, 5 }, { 2, 1, 1700000600, "::1", 19675, 1 } });
		db.RecordApiKeyUsage({ { 1, 3, 1700000400, "10.0.0.3", 19676, 3 } });
		return json{ db.GetApiKey(1), db.GetApiKey(2) };
	});
	Both("RotateApiKey", [](GameDatabase& db) { db.RotateApiKey(1, std::string(64, 'd'), "dlk_dddd", 1700000700); return json{ db.GetApiKey(1), db.GetApiKeyByHash(std::string(64, 'a')) }; });
	Both("RevokeApiKey", [](GameDatabase& db) { db.RevokeApiKey(1, "gm", 1700000800); db.RevokeApiKey(1, "late", 1700000900); return db.GetApiKey(1); });
	Both("RotateApiKey revoked", [](GameDatabase& db) { db.RotateApiKey(1, std::string(64, 'e'), "dlk_eeee", 1700001000); return db.GetApiKey(1); });
	Both("RevokeAccountApiKeys", [](GameDatabase& db) { return json{ db.RevokeAccountApiKeys(2, "bob", 1700001100), db.GetApiKeys(2), db.GetApiKeys(1) }; });
}

TEST_F(ParitySeeded, Moderation) {
	Both("InsertPlayerReport", [](GameDatabase& db) {
		IModeration::PlayerReport report;
		report.createdAt = 1700000000; report.kind = "PLAYER"; report.reporterId = CHAR_ALICE; report.reporterAccountId = 1;
		report.targetCharacterId = CHAR_BOB; report.targetAccountId = 2; report.zoneId = 1100; report.instanceId = 3; report.cloneId = 0; report.body = "Said bad words";
		json ids = json::array();
		ids.push_back(db.InsertPlayerReport(report));
		report.createdAt = 1700000100; report.kind = "PROPERTY"; report.propertyId = 1152921510000300002LL; report.objectId = 1152921510000300002LL; report.objectLot = 3315; report.body = "Rude build";
		ids.push_back(db.InsertPlayerReport(report));
		report.createdAt = 1700000200; report.kind = "MODEL"; report.targetAccountId = 1; report.targetCharacterId = CHAR_ALICE; report.body = "";
		ids.push_back(db.InsertPlayerReport(report));
		return ids;
	});
	Both("GetPlayerReports", [](GameDatabase& db) {
		IModeration::PlayerReportQuery all;
		IModeration::PlayerReportQuery bob; bob.accountId = 2;
		IModeration::PlayerReportQuery page; page.limit = 1; page.offset = 1;
		return json{ db.GetPlayerReports(all), db.GetPlayerReports(bob), db.GetPlayerReports(page), db.CountPlayerReports(all), db.CountPlayerReports(bob) };
	});
	Both("SetPlayerReportStatus", [](GameDatabase& db) { db.SetPlayerReportStatus(1, 1, "gm", "Muted", 1700000300); return json{ db.GetPlayerReport(1), db.GetPlayerReport(99) }; });
	Both("GetPlayerReports by status", [](GameDatabase& db) {
		IModeration::PlayerReportQuery open; open.status = 0;
		IModeration::PlayerReportQuery actioned; actioned.status = 1;
		return json{ db.GetPlayerReports(open), db.CountPlayerReports(open), db.CountPlayerReports(actioned) };
	});
	Both("ChatFilterWords", [](GameDatabase& db) {
		json out = json::array();
		out.push_back(db.GetChatFilterWords());
		db.SetChatFilterWord({ "darn", false, "gm", 1700000000 });
		db.SetChatFilterWord({ "brickbuild", true, "gm", 1700000000 });
		db.SetChatFilterWord({ "darn", true, "admin", 1700000100 });
		out.push_back(db.GetChatFilterWords());
		out.push_back(db.DeleteChatFilterWord("brickbuild"));
		out.push_back(db.DeleteChatFilterWord("brickbuild"));
		out.push_back(db.GetChatFilterWords());
		return out;
	});
	Both("RecordLoginAddress", [](GameDatabase& db) {
		db.RecordLoginAddress(1, "10.0.0.1", 1700000000);
		db.RecordLoginAddress(1, "10.0.0.1", 1700000500);
		db.RecordLoginAddress(2, "10.0.0.1", 1700000300);
		db.RecordLoginAddress(2, "10.0.0.2", 1700000400);
		db.RecordLoginAddress(3, "10.0.0.9", 1700000400);
	});
	Both("CountLoginAddresses", [](GameDatabase& db) { return json{ db.CountLoginAddresses(1), db.CountLoginAddresses(2), db.CountLoginAddresses(99) }; });
	Both("GetLinkedAccounts", [](GameDatabase& db) { return json{ db.GetLinkedAccounts(1), db.GetLinkedAccounts(2), db.GetLinkedAccounts(3) }; });
	Both("PruneLog login addresses", [](GameDatabase& db) { return db.PruneLog(IDashboardAdmin::eLog::LOGIN_ADDRESS, 1700000350); });
	Both("CountLoginAddresses after prune", [](GameDatabase& db) { return json{ db.CountLoginAddresses(1), db.CountLoginAddresses(2) }; });
	Both("InsertModerationDecision", [](GameDatabase& db) {
		db.InsertModerationDecision("name", CHAR_ALICE2, "AliceRenamed", false, "Not allowed", 1700000000);
		db.InsertModerationDecision("name", CHAR_ALICE2, "AliceOther", true, "", 1700000100);
		db.InsertModerationDecision("pet_name", 1152921510000200001LL, "Sparky", true, "", 1700000100);
	});
	Both("GetModerationDecisions", [](GameDatabase& db) { return json{ db.GetModerationDecisions("name", CHAR_ALICE2, 10), db.GetModerationDecisions("name", CHAR_ALICE2, 1), db.GetModerationDecisions("pet_name", 5, 10) }; });
}

TEST_F(ParitySeeded, ChatLog) {
	Both("InsertChatMessage", [](GameDatabase& db) {
		json ids = json::array();
		ids.push_back(db.InsertChatMessage({ 0, 1700000000, "zone", CHAR_ALICE, "Alice", 1, 0, "", 1100, 1, 0, "Hello everyone", false }));
		ids.push_back(db.InsertChatMessage({ 0, 1700000010, "whisper", CHAR_BOB, "Bob", 2, CHAR_ALICE, "Alice", 1100, 1, 0, "psst 100% secret", false }));
		ids.push_back(db.InsertChatMessage({ 0, 1700000020, "zone", CHAR_BOB, "Bob", 2, 0, "", 1200, 0, 0, "bad word", true }));
		ids.push_back(db.InsertChatMessage({ 0, 1700000030, "web", 0, "Dashboard", 0, 0, "", 0, 0, 0, "Server restarting", false }));
		return ids;
	});
	const auto queries = [] {
		std::vector<std::pair<std::string, IChatLog::ChatQuery>> list;
		IChatLog::ChatQuery query;
		list.emplace_back("default", query);
		query.includePrivate = true; list.emplace_back("private", query);
		query.newestFirst = true; list.emplace_back("newest", query);
		IChatLog::ChatQuery after; after.afterId = 1; after.includePrivate = true; list.emplace_back("after", after);
		IChatLog::ChatQuery channel; channel.channel = "zone"; list.emplace_back("channel", channel);
		IChatLog::ChatQuery character; character.characterId = CHAR_ALICE; character.includePrivate = true; list.emplace_back("character", character);
		IChatLog::ChatQuery account; account.accountId = 2; list.emplace_back("account", account);
		IChatLog::ChatQuery zone; zone.zoneId = 1100; zone.instanceId = 1; list.emplace_back("zone", zone);
		IChatLog::ChatQuery instance0; instance0.instanceId = 0; list.emplace_back("instance 0", instance0);
		IChatLog::ChatQuery since; since.since = 1700000015; since.includePrivate = true; list.emplace_back("since", since);
		IChatLog::ChatQuery search; search.search = "100%"; search.includePrivate = true; list.emplace_back("search percent", search);
		IChatLog::ChatQuery searchName; searchName.search = "bob"; searchName.includePrivate = true; list.emplace_back("search name", searchName);
		IChatLog::ChatQuery blocked; blocked.blockedOnly = true; list.emplace_back("blocked", blocked);
		IChatLog::ChatQuery page; page.limit = 1; page.offset = 1; page.includePrivate = true; list.emplace_back("page", page);
		return list;
	}();
	for (const auto& [name, query] : queries) {
		Both("GetChatMessages " + name, [&](GameDatabase& db) { return json{ db.GetChatMessages(query), db.CountChatMessages(query) }; });
	}
	Both("PruneLog chat", [](GameDatabase& db) { return db.PruneLog(IDashboardAdmin::eLog::CHAT, 1700000015); });
	Both("GetDashboardSnapshot", [](GameDatabase& db) { return db.GetDashboardSnapshot(); });
}

TEST_F(ParitySeeded, ServerHealth) {
	Both("InsertHealthSample", [](GameDatabase& db) {
		db.InsertHealthSample({ 1700000000, 5, 2, true, true, 100000 });
		db.InsertHealthSample({ 1700000060, 7, 3, true, false, 110000, true, true });
		db.InsertHealthSample({ 1700000120, 3, 3, false, true, 90000, true, false });
		db.InsertHealthSample({ 1700000500, 1, 1, true, true, 50000 });
	});
	Both("GetHealthSamples", [](GameDatabase& db) { return json{ db.GetHealthSamples(1700000000, 1700001000, 60), db.GetHealthSamples(1700000000, 1700001000, 300), db.GetHealthSamples(1700000050, 1700000130, 60) }; });
	Both("InsertInstanceSamples", [](GameDatabase& db) {
		db.InsertInstanceSamples({ { 1700000000, 1100, 1, 0, 3 }, { 1700000000, 1200, 2, 0, 2 }, { 1700000060, 1100, 1, 0, 5 }, { 1700000120, 1100, 1, 0, 4 } });
	});
	Both("GetInstanceSamples", [](GameDatabase& db) { return json{ db.GetInstanceSamples(1700000000, 1700001000, 300, 0), db.GetInstanceSamples(1700000000, 1700001000, 60, 1100) }; });
	Both("PruneHealthSamples", [](GameDatabase& db) { return db.PruneHealthSamples(1700000100); });
	Both("After prune", [](GameDatabase& db) { return json{ db.GetHealthSamples(0, 1800000000, 60), db.GetInstanceSamples(0, 1800000000, 60, 0) }; });
}

TEST_F(ParitySeeded, PlayerPositions) {
	Both("InsertPositionSamples", [](GameDatabase& db) {
		db.InsertPositionSamples({ { 1700000000, 11, 1100, 1, 0, 1.5f, 2.0f, -3.25f }, { 1700000005, 11, 1100, 1, 0, 4.5f, 2.0f, -3.25f },
			{ 1700000010, 11, 1100, 2, 0, 8.0f, 2.5f, 0.0f }, { 1700000000, 12, 1100, 1, 0, -10.0f, 0.0f, 10.0f }, { 1700000030, 12, 1200, 3, 7, 1.0f, 1.0f, 1.0f } });
		// The same character and second again replaces the first
		db.InsertPositionSamples({ { 1700000005, 11, 1100, 1, 0, 5.5f, 2.0f, -3.25f } });
	});
	Both("GetPositionSamples", [](GameDatabase& db) {
		return json{ db.GetPositionSamples(1100, 0, 1700000000, 1700000100, 1, 100), db.GetPositionSamples(1100, 1, 1700000000, 1700000100, 1, 100),
			db.GetPositionSamples(1100, 0, 1700000000, 1700000100, 20, 100), db.GetPositionSamples(1100, 0, 1700000000, 1700000100, 1, 2) };
	});
	Both("GetPositionInstances", [](GameDatabase& db) { return json{ db.GetPositionInstances(0, 0, 1800000000), db.GetPositionInstances(1200, 0, 1800000000) }; });
	Both("GetCloneVisitors", [](GameDatabase& db) {
		return json{ db.GetCloneVisitors(1200, 7, 1700000000, 1700000100, 10), db.GetCloneVisitors(1200, 8, 1700000000, 1700000100, 10), db.GetCloneVisitors(1100, 0, 1700000000, 1700000100, 1) };
	});
	Both("GetMapCellsPerDay", [](GameDatabase& db) { return json{ db.GetMapCellsPerDay(1100, std::nullopt, 1, 0, 100000, 100), db.GetMapCellsPerDay(1150, 1u, 1, 0, 100000, 100) }; });
	Both("PrunePositionSamples", [](GameDatabase& db) { return db.PrunePositionSamples(1700000006); });
	Both("After prune", [](GameDatabase& db) { return db.GetPositionSamples(1100, 0, 0, 1800000000, 1, 100); });
}

TEST_F(ParitySeeded, SlashCommands) {
	Both("SetSlashCommand", [](GameDatabase& db) {
		db.SetSlashCommand({ "fly", { "fly", "flight" }, "Toggle flying", "Lets you fly", 4, 0, false, false, "", "" });
		db.SetSlashCommand({ "gmlevel", { "gmlevel", "setgmlevel", "makegm" }, "Set level", "", 1, 1, true, false, "Fixed", "accounts.gm" });
		db.SetSlashCommand({ "fly", { "fly" }, "Toggle flying!", "Lets you fly", 4, 2, false, true, "note", "" });
		db.SetSlashCommand({ "empty", {}, "", "", 0, 0, false, false, "", "" });
	});
	Both("GetSlashCommands", [](GameDatabase& db) { return db.GetSlashCommands(); });
	Both("DeleteSlashCommand", [](GameDatabase& db) { db.DeleteSlashCommand("gmlevel"); return db.GetSlashCommands(); });
}

TEST_F(ParitySeeded, ServerOperations) {
	Both("InsertScheduledAnnouncement", [](GameDatabase& db) {
		IServerOperations::ScheduledAnnouncement announcement;
		announcement.title = "Restart"; announcement.message = "Restart soon"; announcement.zones = { 1100, 1200 }; announcement.schedule = "0 * * * *";
		announcement.startsAt = 1700000000; announcement.createdAt = 1700000000; announcement.createdBy = "gm"; announcement.updatedAt = 1700000000; announcement.updatedBy = "gm";
		json ids = json::array();
		ids.push_back(db.InsertScheduledAnnouncement(announcement));
		announcement.title = "Everywhere"; announcement.zones = {}; announcement.enabled = false;
		ids.push_back(db.InsertScheduledAnnouncement(announcement));
		return ids;
	});
	Both("UpdateScheduledAnnouncement", [](GameDatabase& db) {
		auto list = db.GetScheduledAnnouncements();
		auto announcement = list.at(0);
		announcement.message = "Updated"; announcement.zones = { 1300 }; announcement.endsAt = 1800000000; announcement.lastSentAt = 5; announcement.sentCount = 9;
		db.UpdateScheduledAnnouncement(announcement);
		db.MarkAnnouncementSent(announcement.id, 1700000600);
		db.MarkAnnouncementSent(announcement.id, 1700000700);
		return db.GetScheduledAnnouncements();
	});
	Both("DeleteScheduledAnnouncement", [](GameDatabase& db) { db.DeleteScheduledAnnouncement(2); return db.GetScheduledAnnouncements(); });
	Both("InsertScheduledEvent", [](GameDatabase& db) {
		IServerOperations::ScheduledEvent event;
		event.name = "Halloween"; event.note = "Spooky"; event.startsAt = 1700000000; event.endsAt = 1700086400;
		event.parts = R"([{"kind":"feature","config":{"feature":"HalloweenEvent"}}])";
		event.createdAt = 1699990000; event.createdBy = "gm"; event.updatedAt = 1699990000; event.updatedBy = "gm";
		json ids = json::array();
		ids.push_back(db.InsertScheduledEvent(event));
		event.name = "Full moon"; event.mode = 2; event.priority = -3; event.startsAt = 0; event.endsAt = 0;
		event.schedule = R"({"match":"any","rules":[{"type":"moon","phase":"full_moon","days":0}],"utcOffset":0})";
		event.parts = R"([{"kind":"vanity","config":{"file":"werewolf.xml","removals":"","fileSwitches":{"summer.xml":false}},"applied":true}])";
		event.state = IServerOperations::eEventState::ACTIVE;
		ids.push_back(db.InsertScheduledEvent(event));
		return ids;
	});
	Both("UpdateScheduledEvent", [](GameDatabase& db) {
		auto event = db.GetScheduledEvents().at(0);
		event.state = IServerOperations::eEventState::ENDED; event.mode = 0; event.priority = 7; event.status = "done";
		event.parts = R"([{"kind":"feature","config":{"feature":"HalloweenEvent"},"applied":false,"state":{"slot":3}}])";
		db.UpdateScheduledEvent(event);
		return db.GetScheduledEvents();
	});
	Both("DeleteScheduledEvent", [](GameDatabase& db) { db.DeleteScheduledEvent(1); return db.GetScheduledEvents(); });
	Both("SetZoneLimit", [](GameDatabase& db) {
		db.SetZoneLimit({ 1100, 30, 40, 1, 1700000000, "gm" });
		db.SetZoneLimit({ 1200, std::nullopt, 60, 0, 1700000000, "gm" });
		db.SetZoneLimit({ 1100, std::nullopt, std::nullopt, 2, 1700000100, "admin" });
		return db.GetZoneLimits();
	});
	Both("DeleteZoneLimit", [](GameDatabase& db) { db.DeleteZoneLimit(1200); return db.GetZoneLimits(); });
}

TEST_F(ParitySeeded, Maintenance) {
	Both("ForEachCharacterXml", [](GameDatabase& db) {
		json rows = json::array();
		db.ForEachCharacterXml([&](LWOOBJID id, const std::string& xml) { rows.push_back({ id, xml }); });
		std::sort(rows.begin(), rows.end());
		return rows;
	});
	Both("ForEachCharacterXmlContaining", [](GameDatabase& db) {
		json rows = json::array();
		db.ForEachCharacterXmlContaining("<pet ", [&](LWOOBJID id, const std::string& xml) { rows.push_back({ id, xml }); });
		db.ForEachCharacterXmlContaining("%", [&](LWOOBJID id, const std::string& xml) { rows.push_back({ id, xml }); });
		return rows;
	});
}

TEST_F(ParitySeeded, AccountLockAndNoteAndTransactions) {
	Both("SetAccountLocked", [](GameDatabase& db) { db.SetAccountLocked(1, true); const auto locked = db.GetAccountInfo("alice"); db.SetAccountLocked(1, false); return json{ locked, db.GetAccountInfo("alice") }; });
	Both("SetLockout resets attempts", [](GameDatabase& db) {
		db.RecordFailedAttempt(2); db.RecordFailedAttempt(2); db.RecordFailedAttempt(2);
		const auto before = db.GetFailedAttempts(2);
		db.SetLockout(2, Now() + 60);
		const json out{ before, db.GetFailedAttempts(2), db.IsLockedOut(2) };
		db.SetLockout(2, 0);
		return json{ out, db.IsLockedOut(2) };
	});
	Both("GetAccountNote", [](GameDatabase& db) { db.InsertAccountNote({ 0, 3, "note", "Staff note", "gm", 1700000000 }); const auto notes = db.GetAccountNotes(3); return json{ db.GetAccountNote(notes.at(0).id), db.GetAccountNote(999999) }; });
	Both("DatabaseTransaction rolls back", [](GameDatabase& db) {
		{
			DatabaseTransaction transaction(db);
			db.SetDashboardState("tx_test", "rolled back");
			// no Commit: leaving the scope rolls back
		}
		const auto afterRollback = db.GetDashboardState("tx_test");
		{
			DatabaseTransaction transaction(db);
			db.SetDashboardState("tx_test", "kept");
			transaction.Commit();
		}
		return json{ afterRollback, db.GetDashboardState("tx_test"), db.GetAutoCommit() };
	});
}

TEST_F(ParitySeeded, SearchEscaping) {
	Both("GetShowcaseProperties wildcard search", [](GameDatabase& db) { IProperty::ShowcaseQuery query; query.search = "%"; return db.GetShowcaseProperties(query); });
	Both("GetShowcaseProperties underscore search", [](GameDatabase& db) { IProperty::ShowcaseQuery query; query.search = "_"; return db.GetShowcaseProperties(query); });
	Both("GetChatMessages wildcard search", [](GameDatabase& db) {
		IChatLog::ChatQuery query; query.includePrivate = true;
		json out = json::array();
		for (const auto* text : { "%", "_", "!", "100%", "'" }) {
			query.search = text;
			out.push_back(db.CountChatMessages(query));
		}
		return out;
	});
}

TEST_F(ParitySeeded, LiveOps) {
	Both("InsertLiveEvent", [](GameDatabase& db) {
		ILiveOps::LiveEvent event;
		event.type = "treasure_hunt"; event.title = "Hunt"; event.message = "Go!"; event.zones = { 1100, 1200 }; event.config = R"({"chests":5})";
		event.startsAt = 1700000000; event.endsAt = 1700003600; event.createdBy = "gm";
		json ids = json::array();
		ids.push_back(db.InsertLiveEvent(event));
		event.type = "bonus"; event.title = "Double coins"; event.zones = {}; event.instanceId = 3;
		ids.push_back(db.InsertLiveEvent(event));
		return ids;
	});
	Both("GetLiveEvents", [](GameDatabase& db) { return json{ db.GetLiveEvents(false, 10), db.GetLiveEvents(true, 1), db.GetLiveEvent(1), db.GetLiveEvent(99) }; });
	Both("EndLiveEvent", [](GameDatabase& db) { return json{ db.EndLiveEvent(1, ILiveOps::eLiveEventState::CANCELLED, 1700001000, "admin", "oops"), db.EndLiveEvent(1, ILiveOps::eLiveEventState::ENDED, 1700002000, "admin", "again"), db.GetLiveEvents(true, 10), db.GetLiveEvent(1) }; });
	Both("SetLiveEventInstance", [](GameDatabase& db) {
		db.SetLiveEventInstance({ 2, 1100, 1, R"({"found":1})", 1700000100 });
		db.SetLiveEventInstance({ 2, 1100, 1, R"({"found":2})", 1700000200 });
		db.SetLiveEventInstance({ 2, 1200, 4, "{}", 1700000200 });
		db.SetLiveEventInstance({ 1, 1100, 1, "{}", 1700000200 });
		return json{ db.GetLiveEventInstances({ 2 }), db.GetLiveEventInstances({ 1, 2 }), db.GetLiveEventInstances({}) };
	});
	Both("AddLiveEventScores", [](GameDatabase& db) {
		db.AddLiveEventScores(2, { { CHAR_ALICE, 5 }, { CHAR_BOB, 3 } }, 1700000100);
		db.AddLiveEventScores(2, { { CHAR_BOB, 4 } }, 1700000200);
		return json{ db.GetLiveEventScores(2, 10), db.GetLiveEventScores(2, 1), db.GetLiveEventScores(1, 10) };
	});
	Both("InsertChallenge", [](GameDatabase& db) {
		ILiveOps::Challenge challenge;
		challenge.title = "Smash 1000"; challenge.description = "Together"; challenge.metric = 3; challenge.zones = { 1100 }; challenge.target = 1000;
		challenge.startsAt = 1700000000; challenge.endsAt = 1800000000; challenge.rewardCoins = 100; challenge.rewardItems = R"([{"lot":6086,"count":1}])";
		challenge.createdAt = 1700000000; challenge.createdBy = "gm"; challenge.updatedAt = 1700000000; challenge.updatedBy = "gm";
		json ids = json::array();
		ids.push_back(db.InsertChallenge(challenge));
		challenge.title = "Kill"; challenge.metricKind = ILiveOps::eChallengeMetric::MAP_EVENT; challenge.metric = 1; challenge.lot = 4712; challenge.zones = {}; challenge.isPublic = false; challenge.includeStaff = true;
		ids.push_back(db.InsertChallenge(challenge));
		return ids;
	});
	Both("UpdateChallenge", [](GameDatabase& db) {
		auto challenge = db.GetChallenge(2).value();
		challenge.state = ILiveOps::eChallengeState::COMPLETED; challenge.milestone = 100; challenge.completedAt = 1700005000; challenge.rewardedAt = 1700005001; challenge.rewardedCount = 2;
		challenge.updatedBy = "admin";
		db.UpdateChallenge(challenge);
		return json{ db.GetChallenges(false), db.GetChallenges(true), db.GetChallenge(99) };
	});
	Both("AddChallengeContributions", [](GameDatabase& db) {
		db.AddChallengeContributions({ { 1, CHAR_ALICE, 10 }, { 1, CHAR_BOB, 7 }, { 2, CHAR_ALICE, 1 } }, 1700000100);
		db.AddChallengeContributions({ { 1, CHAR_ALICE, 5 } }, 1700000200);
		return json{ db.GetChallengeTotals({ 1, 2, 3 }), db.GetChallengeTotals({}), db.GetChallengeContributions(1, 0), db.GetChallengeContributions(1, 1), db.GetCharacterContributions(CHAR_ALICE) };
	});
	Both("ChallengeRewards", [](GameDatabase& db) {
		db.InsertChallengeReward({ 1, CHAR_ALICE, 15, 100, 1700006000, 0 });
		db.InsertChallengeReward({ 1, CHAR_BOB, 7, 0, 1700006000, 0 });
		json out = json::array();
		out.push_back(db.GetChallengeRewards(1));
		out.push_back(db.GetUnclaimedChallengeCoins(CHAR_ALICE));
		out.push_back(db.ClaimChallengeCoins(1, CHAR_ALICE, 1700007000));
		out.push_back(db.ClaimChallengeCoins(1, CHAR_ALICE, 1700008000));
		out.push_back(db.GetUnclaimedChallengeCoins(CHAR_ALICE));
		out.push_back(db.GetChallengeRewards(1));
		return out;
	});
	Both("DeleteChallenge", [](GameDatabase& db) { db.DeleteChallenge(1); return json{ db.GetChallenges(false), db.GetChallengeContributions(1, 0), db.GetChallengeRewards(1), db.GetCharacterContributions(CHAR_ALICE) }; });
}

TEST_F(ParitySeeded, FeaturedProperties) {
	Both("GetFeaturedPropertySlots empty", [](GameDatabase& db) { return db.GetFeaturedPropertySlots(); });
	Both("SetFeaturedPropertySlot", [](GameDatabase& db) {
		db.SetFeaturedPropertySlot({ 25188, 1, 0x1000000000000123, 1700000000, "gm" });
		db.SetFeaturedPropertySlot({ 25166, 2, 0, 1700000100, "gm" });
		return db.GetFeaturedPropertySlots();
	});
	Both("SetFeaturedPropertySlot replaces", [](GameDatabase& db) {
		db.SetFeaturedPropertySlot({ 25188, 0, 0, 1700000200, "admin" });
		return db.GetFeaturedPropertySlots();
	});
	Both("SetFeaturedPropertySlot with a location", [](GameDatabase& db) {
		db.SetFeaturedPropertySlot({ 25191, 1, 0x1000000000000456, 1700000300, "gm", 1151 });
		db.SetFeaturedPropertySlot({ 25188, 0, 0, 1700000400, "gm", 1251 });
		return db.GetFeaturedPropertySlots();
	});
	Both("GetFeaturedPropertiesSettings default", [](GameDatabase& db) { return db.GetFeaturedPropertiesSettings(); });
	Both("SetFeaturedPropertiesSettings", [](GameDatabase& db) {
		db.SetFeaturedPropertiesSettings({ true, 1700000500, "gm" });
		return db.GetFeaturedPropertiesSettings();
	});
	Both("SetFeaturedPropertiesSettings replaces", [](GameDatabase& db) {
		db.SetFeaturedPropertiesSettings({ false, 1700000600, "admin" });
		return db.GetFeaturedPropertiesSettings();
	});
}

namespace {
	constexpr LWOOBJID PROP_REP = 1152921510000900002LL;

	// A property of Alice's for tests that run on their own (ctest runs each test in its own process)
	LWOOBJID EnsureProperty(LWOOBJID id, uint32_t zone) {
		Both("InsertNewProperty " + std::to_string(id), [&](GameDatabase& db) {
			if (db.GetPropertyInfo(id)) return;
			IProperty::Info info{};
			info.id = id; info.ownerId = CHAR_ALICE; info.cloneId = 1; info.name = "Rented"; info.description = "";
			db.InsertNewProperty(info, 1, LWOZONEID(zone, 0, 1));
		});
		return id;
	}
}

TEST_F(ParitySeeded, PropertyRent) {
	Both("SetPropertyRentRate", [](GameDatabase& db) {
		db.SetPropertyRentRate({ 1151, 1000, 0, "admin", 1700000000 });
		db.SetPropertyRentRate({ 1250, 500, 7, "admin", 1700000001 });
		db.SetPropertyRentRate({ 1151, 0, 30, "mod", 1700000002 });
		return db.GetPropertyRentRates();
	});
	Both("DeletePropertyRentRate", [](GameDatabase& db) { return json{ db.DeletePropertyRentRate(1250), db.DeletePropertyRentRate(1250), db.GetPropertyRentRates() }; });
	EnsureProperty(1152921510000900001LL, 1151);
	const auto owned = Both("GetPropertiesOfOwner", [](GameDatabase& db) { return db.GetPropertiesOfOwner(CHAR_ALICE); });
	Both("GetPropertiesOfOwner nobody", [](GameDatabase& db) { return db.GetPropertiesOfOwner(42); });
	ASSERT_FALSE(owned.empty());
	const auto propertyId = owned[0]["id"].get<LWOOBJID>();
	Both("SetPropertyRent", [&](GameDatabase& db) {
		db.SetPropertyRent(propertyId, 1000, 1800000000);
		db.SetPropertyPrivacy(propertyId, 0);
		return json{ db.GetPropertyRentDue(propertyId), db.GetPropertyRentDue(42), db.GetPropertiesOfOwner(CHAR_ALICE) };
	});
}

TEST_F(ParitySeeded, PropertyReputation) {
	EnsureProperty(PROP_REP, 1250);
	const auto before = Both("reputation before", [](GameDatabase& db) { return db.GetPropertyInfo(PROP_REP)->reputation; });
	Both("AddPropertyReputation", [](GameDatabase& db) {
		db.AddPropertyReputation(PROP_REP, 1, 20000, 5, 300);
		db.AddPropertyReputation(PROP_REP, 1, 20000, 3, 120);
		db.AddPropertyReputation(PROP_REP, 1, 19990, 7, 600);
		db.AddPropertyReputation(PROP_REP, 1, 19950, 9, 600);   // outside a 30 day window
		db.AddPropertyReputation(PROP_REP, 1, 19995, 0, 60);    // time without points: not a repeat day
		db.AddPropertyReputation(PROP_REP, 2, 20000, 4, 240);
		return db.GetPropertyInfo(PROP_REP)->reputation;
	});
	EXPECT_EQ(Both("reputation after", [](GameDatabase& db) { return db.GetPropertyInfo(PROP_REP)->reputation; }).get<int64_t>() - before.get<int64_t>(), 28);
	const auto history = Both("GetPropertyVisitorHistory", [](GameDatabase& db) {
		const auto h = db.GetPropertyVisitorHistory(PROP_REP, 1, 20000, 30);
		const auto none = db.GetPropertyVisitorHistory(PROP_REP, 3, 20000, 30);
		return json{ h.today, h.previousDays, none.today, none.previousDays };
	});
	EXPECT_EQ(history, json({ 8, 1, 0, 0 }));
	EXPECT_EQ(Both("GetPropertyReputationOnDay", [](GameDatabase& db) { return db.GetPropertyReputationOnDay(PROP_REP, 20000); }), 12);
	Both("GetPropertyReputationDays", [](GameDatabase& db) { return db.GetPropertyReputationDays(PROP_REP, 19980); });
}

TEST_F(ParitySeeded, BbbAutosave) {
	std::string binary = "sd0\x01\xff";
	for (int i = 0; i < 256; i++) binary += static_cast<char>(i);
	const auto describe = [](const std::optional<IBbbAutosave::Info>& info) {
		if (!info) return json(nullptr);
		return json{ info->lxfml, info->sourceItems, info->updatedAt };
	};
	EXPECT_EQ(Both("GetBbbAutosave empty", [&](GameDatabase& db) { return describe(db.GetBbbAutosave(CHAR_BOB)); }), json(nullptr));
	Both("SetBbbAutosave", [&](GameDatabase& db) {
		db.SetBbbAutosave(CHAR_BOB, { "first", { 1152921510000000001LL }, 1700000000 });
		db.SetBbbAutosave(CHAR_BOB, { binary, { 1152921510000000002LL, 1152921510000000003LL }, 1700000001 });
		return describe(db.GetBbbAutosave(CHAR_BOB));
	});
	EXPECT_EQ(Both("DeleteBbbAutosave", [&](GameDatabase& db) {
		db.DeleteBbbAutosave(CHAR_BOB);
		return describe(db.GetBbbAutosave(CHAR_BOB));
	}), json(nullptr));
}

TEST_F(ParitySeeded, Contraband) {
	Both("GetContrabandItems empty", [](GameDatabase& db) { return db.GetContrabandItems(); });
	Both("SetContrabandItem", [](GameDatabase& db) {
		db.SetContrabandItem({ 14128, "Atlantis Squid Helm", IContraband::eContrabandAction::REMOVE, "admin", 1700000000 });
		db.SetContrabandItem({ 6655, "Stig's Helmet", IContraband::eContrabandAction::FLAG, "admin", 1700000001 });
		db.SetContrabandItem({ 6655, "changed", IContraband::eContrabandAction::REMOVE, "mod", 1700000002 });
		return db.GetContrabandItems();
	});
	Both("DeleteContrabandItem", [](GameDatabase& db) { return json{ db.DeleteContrabandItem(14128), db.DeleteContrabandItem(14128), db.GetContrabandItems() }; });
	Both("Contraband economy flag", [](GameDatabase& db) {
		const bool first = db.InsertEconomyFlag({ 0, IDashboardAdmin::eFlagKind::CONTRABAND, CHAR_BOB, 6655, 1152921510000300001LL, 1, 1, "Removed at login" });
		const bool again = db.InsertEconomyFlag({ 0, IDashboardAdmin::eFlagKind::CONTRABAND, CHAR_BOB, 6655, 1152921510000300001LL, 1, 1, "Removed at login" });
		return json{ first, again, db.GetEconomyFlagsFor(CHAR_BOB, 5).size() };
	});
}

TEST_F(ParitySeeded, MessageCaptures) {
	using Query = IMessageCaptures::SessionQuery;
	using eOrder = IMessageCaptures::eSessionOrder;
	// Every byte value, so binary payloads must come back unchanged
	std::string binary;
	for (int i = 0; i < 256; i++) binary += static_cast<char>(i);

	Both("GetMessageCaptureSessions empty", [](GameDatabase& db) { return json{ db.GetMessageCaptureSessions({}), db.CountMessageCaptureSessions({}) }; });
	Both("InsertMessageCaptureSession", [](GameDatabase& db) {
		const auto first = db.InsertMessageCaptureSession({ 0, CHAR_ALICE, "Alice", 1, "alice", 3, "Admin", 1700000000, 1700000120, 0, "", true, true, "", "1234,5", 0, 0, 0, "", 0, 0, 0 });
		const auto second = db.InsertMessageCaptureSession({ 0, CHAR_BOB, "bob", 2, "bob", 4, "gm", 1700001000, 1700001060, 1700001060, "Time limit reached", true, false, "154", "", 1200, 3, 7, "1200:3:7", 0, 0, 2 });
		db.InsertMessageCaptureSession({ 0, CHAR_ALICE, "Alice", 1, "alice", 4, "gm", 1700002000, 1700002030, 1700002010, "Stopped by gm", false, true, "", "", 1100, 1, 0, "1100:1:0", 0, 0, 0 });
		return json{ second == first + 1, db.CountMessageCaptureSessions({}) };
	});
	Both("InsertMessageCaptureEntries and UpdateMessageCaptureSession", [&](GameDatabase& db) {
		auto sessions = db.GetMessageCaptureSessions({ .order = eOrder::STARTED, .ascending = true });
		const auto id = sessions[0].id;
		db.InsertMessageCaptureEntries({
			{ id, 1, 1700000000123, 0, 1234, CHAR_ALICE, 80, 0, 1100, 1, 0, std::string("\x01\x00\xff", 3), R"({"skillID":42})" },
			{ id, 2, 1700000000456, 1, 5, 0x1000000000000123, 0, 3, 1100, 1, 0, "", "" },
			{ id, 3, 1700000001000, 1, 65535, -1, 2048 * 8 + 5, 0, 1200, 2, 9, binary, "" },
		});
		auto session = sessions[0];
		session.endedAt = 1700000100;
		session.endReason = "Stopped by Admin";
		session.zoneId = 1200;
		session.instanceId = 2;
		session.cloneId = 9;
		session.zones = "1100:1:0 1200:2:9";
		session.messageCount = 3;
		session.byteCount = 300;
		session.dropped = 3;
		db.UpdateMessageCaptureSession(session);
		auto updated = db.GetMessageCaptureSession(id);
		if (updated) updated->id = 0; // AUTO_INCREMENT and AUTOINCREMENT can differ
		auto entries = db.GetMessageCaptureEntries(id, 0, 10);
		for (auto& entry : entries) {
			entry.sessionId = 0;
			// Compared as hex: raw bytes aren't valid JSON text
			std::string hex;
			for (const unsigned char byte : entry.payload) hex += std::string{ "0123456789abcdef"[byte >> 4], "0123456789abcdef"[byte & 15] };
			entry.payload = hex;
		}
		return json{ updated, entries, db.GetMessageCaptureEntries(id, 1, 1).size(), db.GetMessageCaptureEntries(id, 3, 10).size(), db.GetMessageCaptureSession(999999) };
	});
	Both("GetMessageCaptureSessions filters and order", [](GameDatabase& db) {
		const auto list = [&db](const Query& query) {
			json out = json::array();
			for (const auto& session : db.GetMessageCaptureSessions(query)) out.push_back(session.startedAt);
			return json{ out, db.CountMessageCaptureSessions(query) };
		};
		return json{
			list({}),
			list({ .characterId = CHAR_ALICE }),
			list({ .accountId = 2 }),
			list({ .startedBy = "GM" }),
			list({ .since = 1700001000 }),
			list({ .until = 1700001000 }),
			list({ .since = 1700000500, .until = 1700001500 }),
			list({ .unfinishedOnly = true }),
			list({ .order = eOrder::CHARACTER, .ascending = true }),
			list({ .order = eOrder::STARTED_BY }),
			list({ .order = eOrder::MESSAGES }),
			list({ .order = eOrder::BYTES, .ascending = true }),
			list({ .limit = 1, .offset = 1 }),
		};
	});
	Both("DeleteMessageCaptureSession", [](GameDatabase& db) {
		const auto sessions = db.GetMessageCaptureSessions({ .order = eOrder::STARTED, .ascending = true });
		db.DeleteMessageCaptureSession(sessions[0].id);
		return json{ db.CountMessageCaptureSessions({}), db.GetMessageCaptureEntries(sessions[0].id, 0, 10).size() };
	});
}

// Runs last (gtest runs tests in the order they are defined): deletes what the other tests wrote about bob
TEST_F(ParitySeeded, ZZDeleteCharacterAndAccount) {
	Both("DeleteCharacter", [](GameDatabase& db) { db.DeleteCharacter(CHAR_GM); return json{ db.GetCharacterInfo(CHAR_GM), db.GetCharacterXml(CHAR_GM) }; });
	Both("DeleteAccount", [](GameDatabase& db) { db.DeleteAccount(2); return json{ db.GetAccountInfo("bob"), db.GetCharacterInfo(CHAR_BOB), db.GetCharacterXml(CHAR_BOB) }; });
	Both("GetDashboardSnapshot", [](GameDatabase& db) { return db.GetDashboardSnapshot(); });
}

// Two connections of the same kind are independent: closing one leaves the other working (the dashboard's background
// worker opens a second one with Database::CreateConnection)
TEST_F(ParitySeeded, SecondConnectionIsIndependent) {
	for (auto& backend : g_Backends) {
		setenv("DATABASE_TYPE", backend.name.c_str(), 1);
		auto second = Database::CreateConnection();
		EXPECT_EQ(second->GetAccountCount(), backend.db->GetAccountCount()) << backend.name;
		second->Destroy("dDatabaseTests second connection");
		second.reset();
		EXPECT_NO_THROW(backend.db->GetAccountCount()) << backend.name;
		EXPECT_GT(backend.db->GetAccountCount(), 0u) << backend.name;
	}
}
