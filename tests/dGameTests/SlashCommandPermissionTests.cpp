#include <gtest/gtest.h>

#include "SlashCommandHandler.h"
#include "Permissions.h"
#include "Game.h"
#include "dConfig.h"

namespace {
	// Game::config set to a config with these database values for the length of a test
	class ScopedConfig {
	public:
		explicit ScopedConfig(std::map<std::string, std::string> overrides) : m_Previous(Game::config), m_Config("does_not_exist_config.ini") {
			m_Config.SetDatabaseValues(std::move(overrides), {});
			Game::config = &m_Config;
		}
		~ScopedConfig() { Game::config = m_Previous; }
	private:
		dConfig* m_Previous;
		dConfig m_Config;
	};

	Command Paired(const std::string& name, eGameMasterLevel level, const std::string& permission) {
		Command command{ .requiredLevel = level, .minLevel = eGameMasterLevel::FORUM_MODERATOR, .dashboardPermission = permission };
		command.name = name;
		return command;
	}
}

// A paired command needs its permission's level, follows it when it changes, and keeps an explicit override
TEST(SlashCommandPermissionTests, PairedCommandUsesThePermissionLevel) {
	const auto mute = Paired("mute", eGameMasterLevel::JUNIOR_DEVELOPER, "accounts_mute");
	{
		ScopedConfig config({});
		EXPECT_EQ(static_cast<uint8_t>(SlashCommandHandler::GetRequiredLevel(mute)), Permissions::Find("accounts_mute")->defaultLevel);
	}
	{
		ScopedConfig config({ { "permission_accounts_mute", "5" } });
		EXPECT_EQ(SlashCommandHandler::GetRequiredLevel(mute), eGameMasterLevel::LEAD_MODERATOR);
	}
	{
		// An older command_level_mute stays in force
		ScopedConfig config({ { "permission_accounts_mute", "5" }, { "command_level_mute", "6" } });
		EXPECT_EQ(SlashCommandHandler::GetRequiredLevel(mute), eGameMasterLevel::JUNIOR_DEVELOPER);
	}
	{
		// ...until the page drops it
		ScopedConfig config({ { "permission_accounts_mute", "5" }, { "command_level_mute", "permission" } });
		EXPECT_EQ(SlashCommandHandler::GetRequiredLevel(mute), eGameMasterLevel::LEAD_MODERATOR);
	}
	// An unknown permission: an ordinary command with its own level
	const auto unknown = Paired("zz_unknown", eGameMasterLevel::MODERATOR, "no_such_permission");
	ScopedConfig config({ { "command_level_zz_unknown", "4" } });
	EXPECT_EQ(SlashCommandHandler::GetRequiredLevel(unknown), eGameMasterLevel::SENIOR_MODERATOR);
}

TEST(SlashCommandPermissionTests, FloorsApplyToPairedCommands) {
	Command command{ .requiredLevel = eGameMasterLevel::DEVELOPER, .minLevel = eGameMasterLevel::DEVELOPER, .dashboardPermission = "accounts_kick" };
	command.name = "zz_floor";
	ScopedConfig config({ { "permission_accounts_kick", "2" }, { "command_level_zz_floor", "3" } });
	EXPECT_EQ(SlashCommandHandler::GetRequiredLevel(command), eGameMasterLevel::DEVELOPER);
	Command fixed{ .requiredLevel = eGameMasterLevel::CIVILIAN, .fixedLevel = true, .dashboardPermission = "accounts_kick" };
	EXPECT_EQ(SlashCommandHandler::GetRequiredLevel(fixed), eGameMasterLevel::CIVILIAN);
}

// The dashboard's self and rank rules, as the commands apply them
TEST(SlashCommandPermissionTests, TargetRulesMatchTheDashboard) {
	using SlashCommandLevels::eTargetRule;
	using AccountRules::eManageDenial;
	ScopedConfig config({});
	// Nobody below GM 9 acts on a higher level; GM 9 on anyone
	EXPECT_EQ(SlashCommandHandler::TargetDenial(8, 1, 9, 2, eTargetRule::TOOLS), eManageDenial::HIGHER_RANK);
	EXPECT_EQ(SlashCommandHandler::TargetDenial(8, 1, 9, 2, eTargetRule::OTHERS), eManageDenial::HIGHER_RANK);
	EXPECT_EQ(SlashCommandHandler::TargetDenial(9, 1, 9, 2, eTargetRule::MODERATION), eManageDenial::NONE);
	EXPECT_EQ(SlashCommandHandler::TargetDenial(9, 1, 9, 1, eTargetRule::MODERATION), eManageDenial::NONE);
	// Same level: manage_equal_rank (GM 9 by default)
	EXPECT_EQ(SlashCommandHandler::TargetDenial(4, 1, 4, 2, eTargetRule::MODERATION), eManageDenial::EQUAL_RANK);
	EXPECT_EQ(SlashCommandHandler::TargetDenial(4, 1, 3, 2, eTargetRule::MODERATION), eManageDenial::NONE);
	// Themselves: the self permission for the kind (self_tools GM 1, self_items and self_moderation GM 9)
	EXPECT_EQ(SlashCommandHandler::TargetDenial(2, 1, 2, 1, eTargetRule::TOOLS), eManageDenial::NONE);
	EXPECT_EQ(SlashCommandHandler::TargetDenial(8, 1, 8, 1, eTargetRule::ITEMS), eManageDenial::SELF);
	EXPECT_EQ(SlashCommandHandler::TargetDenial(8, 1, 8, 1, eTargetRule::MODERATION), eManageDenial::SELF);
	// OTHERS: on themselves as before (like /setlevel without a name)
	EXPECT_EQ(SlashCommandHandler::TargetDenial(6, 1, 6, 1, eTargetRule::OTHERS), eManageDenial::NONE);
	// NONE: not checked
	EXPECT_EQ(SlashCommandHandler::TargetDenial(1, 1, 9, 2, eTargetRule::NONE), eManageDenial::NONE);

	// The refusal names the permission
	const auto refusal = SlashCommandHandler::TargetRefusal(eManageDenial::SELF, eTargetRule::ITEMS, "mailitem");
	EXPECT_EQ(refusal.rfind("/mailitem: ", 0), 0u);
	EXPECT_NE(refusal.find("self_items"), std::string::npos);
	EXPECT_NE(SlashCommandHandler::TargetRefusal(eManageDenial::EQUAL_RANK, eTargetRule::MODERATION, "ban").find("manage_equal_rank"), std::string::npos);
	EXPECT_EQ(SlashCommandHandler::TargetRefusal(eManageDenial::NONE, eTargetRule::TOOLS, "kick"), "");
}

TEST(SlashCommandPermissionTests, TargetRulesFollowThePermissionsPage) {
	using SlashCommandLevels::eTargetRule;
	using AccountRules::eManageDenial;
	ScopedConfig config({ { "permission_self_moderation", "4" }, { "permission_manage_equal_rank", "4" } });
	EXPECT_EQ(SlashCommandHandler::TargetDenial(4, 1, 4, 1, eTargetRule::MODERATION), eManageDenial::NONE);
	EXPECT_EQ(SlashCommandHandler::TargetDenial(4, 1, 4, 2, eTargetRule::MODERATION), eManageDenial::NONE);
	EXPECT_EQ(SlashCommandHandler::TargetDenial(3, 1, 3, 2, eTargetRule::MODERATION), eManageDenial::EQUAL_RANK);
	EXPECT_EQ(SlashCommandHandler::TargetDenial(4, 1, 5, 2, eTargetRule::MODERATION), eManageDenial::HIGHER_RANK);
}

#include "GameDependencies.h"

class SlashCommandCatalogTest : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
		static bool started = false;
		if (!started) SlashCommandHandler::Startup();
		started = true;
	}
	void TearDown() override { TearDownDependencies(); }
};

// Which commands share a dashboard permission's level, and which rule they follow on other players
TEST_F(SlashCommandCatalogTest, PairsAndTargetRules) {
	using SlashCommandLevels::eTargetRule;
	const std::map<std::string, std::string> pairs{
		{ "kick", "accounts_kick" }, { "mute", "accounts_mute" }, { "ban", "accounts_ban" },
		{ "announce", "server_announce" }, { "setanntitle", "server_announce" }, { "setannmsg", "server_announce" },
		{ "mailitem", "mail_items" }, { "approveproperty", "moderate_properties" },
		{ "showall", "players_view" }, { "findplayer", "players_view" }, { "spectate", "players_view" },
		{ "shutdown", "worlds_manage" }, { "shutdownuniverse", "server_restart" },
		{ "uptime", "health_view" }, { "metrics", "health_view" },
		// Only on the user's own character, or a movement tool: their own level (see SlashCommandHandler.cpp)
		{ "teleport", "" }, { "gmadditem", "" }, { "givemoney", "" }, { "setcurrency", "" }, { "giveuscore", "" }, { "setlevel", "" },
		{ "execute", "" }, { "setgmlevel", "" },
	};
	for (const auto& [alias, permission] : pairs) {
		const auto* command = SlashCommandHandler::FindCommand(alias);
		ASSERT_NE(command, nullptr) << alias;
		EXPECT_EQ(command->dashboardPermission, permission) << alias;
		if (!permission.empty()) {
			ASSERT_NE(Permissions::Find(permission), nullptr) << permission;
			EXPECT_FALSE(command->fixedLevel) << alias;
		}
	}
	const std::map<std::string, eTargetRule> rules{
		{ "kick", eTargetRule::TOOLS }, { "kill", eTargetRule::TOOLS }, { "mailitem", eTargetRule::ITEMS },
		{ "ban", eTargetRule::MODERATION }, { "mute", eTargetRule::MODERATION },
		{ "teleport", eTargetRule::OTHERS }, { "setlevel", eTargetRule::OTHERS }, { "execute", eTargetRule::OTHERS }, { "tpall", eTargetRule::OTHERS },
		{ "gmadditem", eTargetRule::NONE }, { "spectate", eTargetRule::NONE }, { "setgmlevel", eTargetRule::NONE },
	};
	for (const auto& [alias, rule] : rules) {
		const auto* command = SlashCommandHandler::FindCommand(alias);
		ASSERT_NE(command, nullptr) << alias;
		EXPECT_EQ(command->targetRule, rule) << alias;
	}
	// /execute keeps its floor even though the self and rank rules now apply to it too
	EXPECT_EQ(SlashCommandHandler::GetRequiredLevel(*SlashCommandHandler::FindCommand("execute")), eGameMasterLevel::DEVELOPER);
	EXPECT_EQ(SlashCommandHandler::GetRequiredLevel(*SlashCommandHandler::FindCommand("setgmlevel")), eGameMasterLevel::CIVILIAN);
}

// When commands start using their permission's level, a server that had them keeps each one's old level once
TEST(SlashCommandPermissionTests, UpgradeOverrideDecision) {
	using SlashCommandLevels::UpgradeOverride;
	// Existing server, code level differs from the permission's: keep it (/mute: GM 6, accounts_mute GM 2)
	EXPECT_EQ(UpgradeOverride(true, false, false, 6, 1, "", 2), std::optional<uint8_t>(6));
	// Same level: nothing to keep
	EXPECT_EQ(UpgradeOverride(true, false, false, 2, 1, "", 2), std::nullopt);
	// Brand-new server (no stored row), or the upgrade already happened: none
	EXPECT_EQ(UpgradeOverride(false, false, false, 6, 1, "", 2), std::nullopt);
	EXPECT_EQ(UpgradeOverride(true, true, false, 6, 1, "", 2), std::nullopt);
	// An explicit value is already an override; "permission" means follow it
	EXPECT_EQ(UpgradeOverride(true, false, false, 6, 1, "4", 2), std::nullopt);
	EXPECT_EQ(UpgradeOverride(true, false, false, 6, 1, "permission", 2), std::nullopt);
	// A bad value meant the code's level
	EXPECT_EQ(UpgradeOverride(true, false, false, 6, 1, "junk", 2), std::optional<uint8_t>(6));
	EXPECT_EQ(UpgradeOverride(true, false, true, 6, 1, "", 2), std::nullopt);
}

namespace {
	// Just the tables ReportCommands uses
	class UpgradeDatabase : public TestSQLDatabase {
	public:
		std::map<std::string, SlashCommand> commands;
		std::map<std::string, Setting> config; // worldconfig.ini, by name
		std::vector<std::string> audits;

		std::vector<SlashCommand> GetSlashCommands() override {
			std::vector<SlashCommand> rows;
			for (const auto& [_, row] : commands) rows.push_back(row);
			return rows;
		}
		void SetSlashCommand(const SlashCommand& command) override { commands[command.name] = command; }
		std::vector<Setting> GetServerConfig(const std::vector<std::string>&) override {
			std::vector<Setting> rows;
			for (const auto& [_, row] : config) rows.push_back(row);
			return rows;
		}
		void SetWebConfigValue(const std::string& file, const std::string& name, const std::optional<std::string>& value, bool webWins, const std::string& by) override {
			auto& row = config[name];
			row.file = file;
			row.name = name;
			row.webValue = value;
			row.webWins = webWins;
			row.updatedBy = by;
		}
		void InsertAuditLog(uint32_t, const std::string_view accountName, const std::string_view action, const std::string_view description, uint32_t, LWOOBJID) override {
			audits.push_back(std::string(accountName) + " " + std::string(action) + " " + std::string(description));
		}
		// What a world's settings sync would hand it
		std::map<std::string, std::string> WebValues() const {
			std::map<std::string, std::string> values;
			for (const auto& [name, row] : config) if (row.webValue) values[name] = *row.webValue;
			return values;
		}
	};

	ISlashCommands::SlashCommand OldRow(const std::string& name, uint8_t level, const std::string& permission) {
		return { .name = name, .aliases = { name }, .defaultLevel = level, .minLevel = 1, .dashboardPermission = permission };
	}
}

TEST_F(SlashCommandCatalogTest, ExistingServerKeepsItsLevelsOnce) {
	auto* db = new UpgradeDatabase();
	Database::_setDatabase(db);
	// Rows written before commands followed their permission
	db->commands["mute"] = OldRow("mute", 6, "accounts_mute");
	db->commands["kick"] = OldRow("kick", 2, "accounts_kick");
	db->commands["mailitem"] = OldRow("mailitem", 3, "mail_items");
	db->commands["spectate"] = OldRow("spectate", 2, "");
	db->commands["ban"] = OldRow("ban", 4, "accounts_ban");
	db->config["command_level_ban"] = { .file = "worldconfig.ini", .name = "command_level_ban", .webValue = "3", .webWins = true, .updatedBy = "admin" };

	SlashCommandHandler::ReportCommands();
	const std::string upgrade(SlashCommandLevels::UPGRADE_ACTOR);
	EXPECT_EQ(db->config.at("command_level_mute").webValue, std::optional<std::string>("6"));
	EXPECT_EQ(db->config.at("command_level_mute").updatedBy, upgrade);
	EXPECT_EQ(db->config.at("command_level_mailitem").webValue, std::optional<std::string>("3"));
	EXPECT_EQ(db->config.at("command_level_spectate").webValue, std::optional<std::string>("2"));
	EXPECT_FALSE(db->config.contains("command_level_kick"));          // same level as accounts_kick
	EXPECT_EQ(db->config.at("command_level_ban").updatedBy, "admin"); // its own value was already an override
	EXPECT_FALSE(db->config.contains("command_level_announce"));      // not stored before: follows
	EXPECT_EQ(db->audits.size(), 3u);
	for (const auto& audit : db->audits) EXPECT_EQ(audit.rfind(upgrade + " change_command_level ", 0), 0u) << audit;
	EXPECT_TRUE(db->commands.at("mute").followsPermission);

	// The world uses the kept level
	Game::config->SetDatabaseValues(db->WebValues(), {});
	EXPECT_EQ(SlashCommandHandler::GetRequiredLevel(*SlashCommandHandler::FindCommand("mute")), eGameMasterLevel::JUNIOR_DEVELOPER);

	// Only once: another start stores nothing new
	SlashCommandHandler::ReportCommands();
	EXPECT_EQ(db->audits.size(), 3u);

	// Dropped on the Permissions page: it follows accounts_mute, and a later start doesn't bring it back
	db->SetWebConfigValue("worldconfig.ini", "command_level_mute", std::nullopt, false, "admin");
	Game::config->SetDatabaseValues(db->WebValues(), {});
	EXPECT_EQ(static_cast<uint8_t>(SlashCommandHandler::GetRequiredLevel(*SlashCommandHandler::FindCommand("mute"))), Permissions::Level("accounts_mute"));
	SlashCommandHandler::ReportCommands();
	EXPECT_FALSE(db->config.at("command_level_mute").webValue.has_value());
	EXPECT_EQ(db->audits.size(), 3u);
}

TEST_F(SlashCommandCatalogTest, NewServerFollowsThePermissions) {
	auto* db = new UpgradeDatabase();
	Database::_setDatabase(db);
	SlashCommandHandler::ReportCommands();
	EXPECT_TRUE(db->config.empty());
	EXPECT_TRUE(db->audits.empty());
	EXPECT_TRUE(db->commands.at("mute").followsPermission);
	EXPECT_FALSE(db->commands.at("gmadditem").followsPermission);
	Game::config->SetDatabaseValues({}, {});
	EXPECT_EQ(static_cast<uint8_t>(SlashCommandHandler::GetRequiredLevel(*SlashCommandHandler::FindCommand("mute"))), Permissions::Level("accounts_mute"));
}

#include "Character.h"
#include "Entity.h"
#include "PermissionGrants.h"
#include "User.h"

// Grants and denies from the dashboard (PermissionGrants.h), as a player using commands sees them
class SlashCommandGrantsTest : public GameDependenciesTest {
protected:
	static constexpr LWOOBJID CHARACTER_ID = 42;

	void SetUp() override {
		SetUpDependencies();
		static bool started = false;
		if (!started) SlashCommandHandler::Startup();
		started = true;
		user = std::make_unique<User>(UNASSIGNED_SYSTEM_ADDRESS, "tester", "key");
		character = std::make_unique<Character>(CHARACTER_ID, user.get());
		entity = std::make_unique<Entity>(1, info);
		entity->SetCharacter(character.get());
		character->SetEntity(entity.get());
	}

	void TearDown() override {
		entity->SetCharacter(nullptr);
		entity.reset();
		character.reset();
		user.reset();
		TearDownDependencies();
	}

	// Play at a GM level (the account's is at least that) with these grants loaded for the character
	void Play(eGameMasterLevel level, std::vector<PermissionGrants::Rule> rules, eGameMasterLevel accountLevel = eGameMasterLevel::CIVILIAN) {
		user->SetMaxGMLevel(std::max(level, accountLevel));
		entity->SetGMLevel(level);
		auto held = std::make_shared<PermissionGrants::Held>();
		held->rules = std::move(rules);
		user->SetGrants(held, CHARACTER_ID);
	}

	bool MayUse(const std::string& alias) {
		const auto* command = SlashCommandHandler::FindCommand(alias);
		EXPECT_NE(command, nullptr) << alias;
		return command && SlashCommandHandler::MayUse(entity.get(), *command);
	}

	std::unique_ptr<User> user;
	std::unique_ptr<Character> character;
	std::unique_ptr<Entity> entity;
};

TEST_F(SlashCommandGrantsTest, GrantsLetAPlayerUseACommand) {
	using PermissionGrants::eKind;
	Play(eGameMasterLevel::MODERATOR, {});
	EXPECT_FALSE(MayUse("spawn"));
	EXPECT_TRUE(MayUse("kick")); // accounts_kick: GM 2

	Play(eGameMasterLevel::MODERATOR, { { eKind::COMMAND, "spawn", false, 0 } });
	EXPECT_TRUE(MayUse("spawn"));
	EXPECT_FALSE(MayUse("gmadditem"));

	// A grant of a dashboard permission covers the commands that follow it
	Play(eGameMasterLevel::CIVILIAN, { { eKind::PERMISSION, "accounts_kick", false, 0 } });
	EXPECT_TRUE(MayUse("kick"));
	// Every command up to a GM level
	Play(eGameMasterLevel::CIVILIAN, { { eKind::COMMAND_GROUP, "8", false, 0 } });
	EXPECT_TRUE(MayUse("spawn"));
	EXPECT_TRUE(MayUse("gmadditem"));
	// ...but never /execute below its floor
	Play(eGameMasterLevel::MODERATOR, { { eKind::COMMAND_GROUP, "9", false, 0 }, { eKind::COMMAND, "execute", false, 0 } });
	EXPECT_FALSE(MayUse("execute"));
	// Past its expiry a grant does nothing
	Play(eGameMasterLevel::MODERATOR, { { eKind::COMMAND, "spawn", false, 1 } });
	EXPECT_FALSE(MayUse("spawn"));
}

TEST_F(SlashCommandGrantsTest, DeniesTakeCommandsAwayExceptFromOperators) {
	using PermissionGrants::eKind;
	Play(eGameMasterLevel::DEVELOPER, { { eKind::COMMAND, "spawn", true, 0 }, { eKind::PERMISSION, "accounts_kick", true, 0 } });
	EXPECT_FALSE(MayUse("spawn"));
	EXPECT_FALSE(MayUse("kick"));
	EXPECT_TRUE(MayUse("gmadditem"));
	// A GM 9 account playing at GM 8 keeps everything
	Play(eGameMasterLevel::DEVELOPER, { { eKind::COMMAND, "spawn", true, 0 } }, eGameMasterLevel::OPERATOR);
	EXPECT_TRUE(MayUse("spawn"));
	// A player command can be taken from a player
	Play(eGameMasterLevel::CIVILIAN, { { eKind::COMMAND, "pvp", true, 0 } });
	EXPECT_FALSE(MayUse("pvp"));
}

TEST_F(SlashCommandGrantsTest, ForgottenGrantsAreReadAgain) {
	using PermissionGrants::eKind;
	Play(eGameMasterLevel::MODERATOR, { { eKind::COMMAND, "spawn", false, 0 } });
	EXPECT_TRUE(MayUse("spawn"));
	// The dashboard changed them: read from the database again (none there)
	user->ForgetGrants();
	EXPECT_FALSE(MayUse("spawn"));
	ASSERT_NE(user->GetGrants(), nullptr);
	EXPECT_EQ(user->GetGrantsCharacter(), CHARACTER_ID);
}

TEST(SlashCommandPermissionTests, TargetRulesFollowGrants) {
	using SlashCommandLevels::eTargetRule;
	using AccountRules::eManageDenial;
	using PermissionGrants::eKind;
	ScopedConfig config({});
	PermissionGrants::Held held;
	held.rules = { { eKind::PERMISSION, "self_moderation", false, 0 }, { eKind::PERMISSION, "manage_equal_rank", false, 0 } };
	EXPECT_EQ(SlashCommandHandler::TargetDenial(8, 1, 8, 1, eTargetRule::MODERATION), eManageDenial::SELF);
	EXPECT_EQ(SlashCommandHandler::TargetDenial(8, 1, 8, 1, eTargetRule::MODERATION, &held), eManageDenial::NONE);
	EXPECT_EQ(SlashCommandHandler::TargetDenial(4, 1, 4, 2, eTargetRule::MODERATION, &held), eManageDenial::NONE);
	// Never a higher GM level
	EXPECT_EQ(SlashCommandHandler::TargetDenial(4, 1, 5, 2, eTargetRule::MODERATION, &held), eManageDenial::HIGHER_RANK);
}
