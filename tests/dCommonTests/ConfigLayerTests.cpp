#include <gtest/gtest.h>

#include <cstdlib>

#include "dConfig.h"
#include "ConfigSync.h"
#include "BinaryPathFinder.h"

#include <filesystem>
#include <fstream>

namespace {
	void SetEnv(const char* name, const char* value) {
#ifdef _WIN32
		_putenv_s(name, value ? value : "");
#else
		if (value) setenv(name, value, 1); else unsetenv(name);
#endif
	}
}

TEST(ConfigLayerTests, DatabaseValuesLayerAroundFilesAndEnvironment) {
	dConfig config("does_not_exist_config.ini");
	config.SetDatabaseValues({ { "zz_override", "web" }, { "mysql_host", "evil" } }, { { "zz_fallback", "web" }, { "zz_both", "web" } });
	EXPECT_EQ(config.GetValue("zz_override"), "web");
	EXPECT_EQ(config.GetValue("zz_fallback"), "web");
	EXPECT_EQ(config.GetValue("mysql_host"), ""); // connection settings never come from the database
	EXPECT_EQ(config.GetValue("zz_missing"), "");

	// The environment beats a fallback but not an override
	SetEnv("ZZ_BOTH", "env");
	SetEnv("ZZ_OVERRIDE", "env");
	EXPECT_EQ(config.GetValue("zz_both"), "env");
	EXPECT_EQ(config.GetValue("zz_override"), "web");
	SetEnv("ZZ_BOTH", nullptr);
	SetEnv("ZZ_OVERRIDE", nullptr);
}

TEST(ConfigLayerTests, KeyRules) {
	EXPECT_TRUE(dConfig::IsFileOnlyKey("mysql_password"));
	EXPECT_TRUE(dConfig::IsFileOnlyKey("database_type"));
	EXPECT_TRUE(dConfig::IsFileOnlyKey("jwt_secret"));
	EXPECT_FALSE(dConfig::IsFileOnlyKey("max_clients"));
	EXPECT_TRUE(dConfig::IsSecretKey("smtp_password"));
	EXPECT_TRUE(dConfig::IsSecretKey("smtp_oauth2_client_secret"));
	EXPECT_TRUE(dConfig::IsSecretKey("totp_key"));
	EXPECT_FALSE(dConfig::IsSecretKey("smtp_host"));
}

TEST(ConfigLayerTests, ResolveOwnFileFirstAndRespectLocalValues) {
	auto row = [](std::string file, std::string name, std::optional<std::string> web, bool wins) {
		IServerConfig::Setting setting;
		setting.file = std::move(file);
		setting.name = std::move(name);
		setting.webValue = std::move(web);
		setting.webWins = wins;
		return setting;
	};
	const std::vector<IServerConfig::Setting> rows{
		row("sharedconfig.ini", "max_clients", "50", false),
		row("worldconfig.ini", "max_clients", "80", false),     // own file beats shared for the same key
		row("sharedconfig.ini", "log_debug_statements", "1", true),
		row("sharedconfig.ini", "client_location", "/web", false), // set in the file locally: fallback ignored
		row("sharedconfig.ini", "no_web_value", std::nullopt, true),
	};
	const auto resolved = ConfigSync::Resolve(rows, "worldconfig.ini", { "client_location" });
	EXPECT_EQ(resolved.fallbacks.at("max_clients"), "80");
	EXPECT_EQ(resolved.overrides.at("log_debug_statements"), "1");
	EXPECT_FALSE(resolved.fallbacks.contains("client_location"));
	EXPECT_FALSE(resolved.overrides.contains("no_web_value"));
	EXPECT_FALSE(resolved.fallbacks.contains("no_web_value"));
}

// The worlds read the dashboard's permission levels from what it stored: its web value, its file or environment, then a web fallback
TEST(ConfigLayerTests, OwnedValuesComeFromTheOwner) {
	auto row = [](std::string name, std::optional<std::string> web, bool wins, std::string source, std::optional<std::string> file) {
		IServerConfig::Setting setting;
		setting.file = "dashboardconfig.ini";
		setting.name = std::move(name);
		setting.webValue = std::move(web);
		setting.webWins = wins;
		setting.fileSource = std::move(source);
		setting.fileValue = std::move(file);
		return setting;
	};
	std::vector<IServerConfig::Setting> rows{
		row("permission_accounts_kick", "3", true, "file", "5"),           // page beats the file
		row("permission_accounts_ban", std::nullopt, false, "file", "6"),  // the dashboard's file
		row("permission_accounts_mute", std::nullopt, false, "env", "4"),  // the dashboard's environment
		row("permission_players_view", "2", false, "", std::nullopt),      // web value without override: a fallback
		row("permission_self_items", std::nullopt, false, "", std::nullopt), // cleared: nothing
		row("dashboard_port", "8080", true, "file", "80"),                 // not a permission
	};
	rows.push_back(row("permission_other_file", "1", true, "", std::nullopt));
	rows.back().file = "worldconfig.ini";

	ConfigSync::Resolved resolved;
	resolved.overrides["permission_accounts_ban"] = "9"; // what the reader had: replaced
	resolved.fallbacks["permission_accounts_kick"] = "8";
	ConfigSync::AddOwnedValues(resolved, rows, "dashboardconfig.ini", "permission_");
	EXPECT_EQ(resolved.overrides.at("permission_accounts_kick"), "3");
	EXPECT_FALSE(resolved.fallbacks.contains("permission_accounts_kick"));
	EXPECT_EQ(resolved.overrides.at("permission_accounts_ban"), "6");
	EXPECT_EQ(resolved.overrides.at("permission_accounts_mute"), "4");
	EXPECT_EQ(resolved.fallbacks.at("permission_players_view"), "2");
	EXPECT_FALSE(resolved.overrides.contains("permission_players_view"));
	EXPECT_FALSE(resolved.overrides.contains("permission_self_items"));
	EXPECT_FALSE(resolved.fallbacks.contains("permission_self_items"));
	EXPECT_FALSE(resolved.overrides.contains("dashboard_port"));
	EXPECT_FALSE(resolved.overrides.contains("permission_other_file"));

	// Layered into a world's config: the dashboard's value wins over the world's own files and environment
	dConfig config("does_not_exist_config.ini");
	config.SetDatabaseValues(std::move(resolved.overrides), std::move(resolved.fallbacks));
	EXPECT_EQ(config.GetValue("permission_accounts_ban"), "6");
	EXPECT_EQ(config.GetValue("permission_players_view"), "2");
}

TEST(ConfigLayerTests, WhitespaceAroundKeysAndValuesIsIgnored) {
	const auto path = BinaryPathFinder::GetBinaryDir() / "whitespace_test_config.ini";
	{
		std::ofstream out(path, std::ios::binary);
		out << "zz_plain=value\n";
		out << "zz_spaced = spaced value \r\n";
		out << "\tzz_tabbed\t=\tx\t\n";
		out << " = no key\n";
		out << "zz_empty=   \n";
	}

	dConfig config("whitespace_test_config.ini");
	EXPECT_EQ(config.GetValue("zz_plain"), "value");
	EXPECT_EQ(config.GetValue("zz_spaced"), "spaced value"); // inner spaces are kept
	EXPECT_EQ(config.GetValue("zz_tabbed"), "x");
	EXPECT_EQ(config.GetValue("zz_empty"), "");
	EXPECT_EQ(config.GetValue(""), "");

	std::filesystem::remove(path);
}
