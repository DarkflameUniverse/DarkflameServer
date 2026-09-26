#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <set>

#include "SettingsCatalog.h"

using SettingsCatalog::eType;

namespace {
	std::optional<std::string> Normalize(const std::string& file, const std::string& key, const std::string& value) {
		const auto* setting = SettingsCatalog::Find(file, key);
		EXPECT_NE(setting, nullptr) << key;
		std::string error;
		return setting ? SettingsCatalog::Normalize(*setting, value, error) : std::nullopt;
	}

	bool InCatalog(const std::string& key) {
		for (const auto& setting : SettingsCatalog::All()) {
			if (setting.key == key) return true;
		}
		return false;
	}
}

TEST(SettingsCatalogTests, BooleansBecomeOneOrZero) {
	EXPECT_EQ(Normalize("worldconfig.ini", "solo_racing", "true"), "1");
	EXPECT_EQ(Normalize("worldconfig.ini", "solo_racing", " YES "), "1");
	EXPECT_EQ(Normalize("worldconfig.ini", "solo_racing", "off"), "0");
	EXPECT_EQ(Normalize("worldconfig.ini", "solo_racing", "0"), "0");
	EXPECT_FALSE(Normalize("worldconfig.ini", "solo_racing", "maybe"));
	// Empty means unset, so the default applies
	EXPECT_EQ(Normalize("worldconfig.ini", "solo_racing", ""), "");
}

TEST(SettingsCatalogTests, NumbersAreRangeChecked) {
	EXPECT_EQ(Normalize("sharedconfig.ini", "maximum_mtu_size", " 1400 "), "1400");
	EXPECT_FALSE(Normalize("sharedconfig.ini", "maximum_mtu_size", "2000"));
	EXPECT_FALSE(Normalize("sharedconfig.ini", "maximum_mtu_size", "12.5"));
	EXPECT_FALSE(Normalize("masterconfig.ini", "master_server_port", "70000"));
	EXPECT_EQ(Normalize("worldconfig.ini", "hardcore_coin_keep", "0.25"), "0.25");
	EXPECT_FALSE(Normalize("worldconfig.ini", "hardcore_coin_keep", "2"));
	EXPECT_FALSE(Normalize("worldconfig.ini", "hardcore_coin_keep", "nan"));
}

TEST(SettingsCatalogTests, ListsAreCleanedUp) {
	EXPECT_EQ(Normalize("worldconfig.ini", "hardcore_excluded_item_drops", " 6086 , 7044,,"), "6086,7044");
	EXPECT_FALSE(Normalize("worldconfig.ini", "hardcore_excluded_item_drops", "6086,abc"));
	EXPECT_EQ(Normalize("authconfig.ini", "rewardcodes", "4, 30"), "4,30");
}

TEST(SettingsCatalogTests, ChoicesMatchIgnoringCase) {
	EXPECT_EQ(Normalize("dashboardconfig.ini", "smtp_security", "STARTTLS"), "starttls");
	EXPECT_FALSE(Normalize("dashboardconfig.ini", "smtp_security", "ssl"));
}

TEST(SettingsCatalogTests, TextIsOneLineUnlessMultiline) {
	EXPECT_FALSE(Normalize("worldconfig.ini", "source", "a\nb"));
	EXPECT_EQ(Normalize("worldconfig.ini", "help_0_description", "a<br/>\nb"), "a<br/>\nb");
}

TEST(SettingsCatalogTests, EntriesAreWellFormed) {
	std::set<std::pair<std::string, std::string>> seen;
	std::set<std::string> files;
	for (const auto& [file, name] : SettingsCatalog::Files()) files.insert(file);
	for (const auto& setting : SettingsCatalog::All()) {
		EXPECT_TRUE(seen.insert({ setting.file, setting.key }).second) << "duplicate " << setting.key;
		EXPECT_TRUE(files.contains(setting.file)) << setting.key;
		EXPECT_FALSE(setting.title.empty()) << setting.key;
		EXPECT_FALSE(setting.section.empty()) << setting.key;
		// The default must pass its own validation
		std::string error;
		EXPECT_EQ(SettingsCatalog::Normalize(setting, setting.defaultValue, error), setting.defaultValue) << setting.key << ": " << error;
		if (setting.type == eType::CHOICE) EXPECT_FALSE(setting.choices.empty()) << setting.key;
		if (!setting.choiceLabels.empty()) EXPECT_EQ(setting.choiceLabels.size(), setting.choices.size()) << setting.key << ": one label per choice";
	}
}

// The page groups settings by category and section, and hides ones whose condition isn't met
TEST(SettingsCatalogTests, SectionsAndConditionsHoldTogether) {
	std::set<std::string> categories, sections;
	for (const auto& category : SettingsCatalog::Categories()) EXPECT_TRUE(categories.insert(category.id).second) << "duplicate category " << category.id;
	for (const auto& section : SettingsCatalog::Sections()) {
		EXPECT_TRUE(sections.insert(section.name).second) << "section names must be unique: " << section.name;
		EXPECT_TRUE(categories.contains(section.category)) << section.name;
	}
	const auto checkCondition = [](const std::optional<SettingsCatalog::Condition>& condition, const std::string& what) {
		if (!condition) return;
		const auto* controller = SettingsCatalog::Find(condition->file, condition->key);
		ASSERT_NE(controller, nullptr) << what << " depends on " << condition->key << ", which isn't in the catalog";
		EXPECT_FALSE(condition->values.empty()) << what;
		for (const auto& value : condition->values) {
			std::string error;
			EXPECT_EQ(SettingsCatalog::Normalize(*controller, value, error), value) << what << ": " << value << " isn't a value of " << condition->key;
		}
	};
	for (const auto& section : SettingsCatalog::Sections()) {
		checkCondition(section.condition, "section " + section.name);
		// A section can't be switched on by one of its own settings: it would hide the switch
		if (section.condition) {
			const auto* controller = SettingsCatalog::Find(section.condition->file, section.condition->key);
			if (controller) EXPECT_NE(controller->section, section.name) << section.name;
		}
	}
	std::set<std::string> used;
	for (const auto& setting : SettingsCatalog::All()) {
		EXPECT_TRUE(sections.contains(setting.section)) << setting.key;
		used.insert(setting.section);
		checkCondition(setting.condition, setting.key);
		// Units go next to the input, not in the title
		static const std::regex unitInTitle(R"(\((ms|days|seconds|%|bytes)\))");
		EXPECT_FALSE(std::regex_search(setting.title, unitInTitle)) << setting.key << ": put the unit in .unit";
		if (setting.type == eType::INT_LIST) EXPECT_TRUE(setting.defaultValue.empty() || setting.defaultValue.find(' ') == std::string::npos) << setting.key;
	}
	for (const auto& section : sections) EXPECT_TRUE(used.contains(section)) << "empty section " << section;
}

TEST(SettingsCatalogTests, FormatsAreChecked) {
	EXPECT_EQ(Normalize("dashboardconfig.ini", "dashboard_url", "https://dash.example.com"), "https://dash.example.com");
	EXPECT_FALSE(Normalize("dashboardconfig.ini", "dashboard_url", "dash.example.com"));
	EXPECT_EQ(Normalize("dashboardconfig.ini", "smtp_from_address", "noreply@example.com"), "noreply@example.com");
	EXPECT_FALSE(Normalize("dashboardconfig.ini", "smtp_from_address", "noreply example.com"));
	EXPECT_FALSE(Normalize("sharedconfig.ini", "external_ip", "http://1.2.3.4/"));
	EXPECT_EQ(Normalize("sharedconfig.ini", "external_ip", "play.example.com"), "play.example.com");
}

// Every setting in the shipped config files is described in the catalog
TEST(SettingsCatalogTests, CoversEveryShippedSetting) {
	const std::filesystem::path resources = std::filesystem::path(DLU_SOURCE_DIR) / "resources";
	size_t checked = 0;
	for (const auto& entry : std::filesystem::directory_iterator(resources)) {
		if (entry.path().extension() != ".ini") continue;
		const auto file = entry.path().filename().string();
		std::ifstream in(entry.path());
		std::string line;
		while (std::getline(in, line)) {
			if (line.empty() || line[0] == '#' || line.find('=') == std::string::npos) continue;
			const auto key = line.substr(0, line.find('='));
			EXPECT_NE(SettingsCatalog::Find(file, key), nullptr) << file << " " << key << " is not in SettingsCatalog.cpp";
			checked++;
		}
	}
	EXPECT_GT(checked, 100u);
}

// Every setting the servers read by name is described too, including ones no .ini mentions
TEST(SettingsCatalogTests, CoversEverySettingTheCodeReads) {
	const std::regex read(R"re((?:GetValue(?:<[^>]+>)?|ConfigFlag|Setting|ParseDelimSetting<[^>]+>\([^,]+,)\s*\(?\s*"([a-z0-9_]+)")re");
	std::set<std::string> missing;
	size_t found = 0;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(DLU_SOURCE_DIR)) {
		const auto relative = std::filesystem::relative(entry.path(), DLU_SOURCE_DIR).string();
		if (!relative.starts_with('d') || relative.starts_with("docs") || !entry.is_regular_file()) continue;
		const auto extension = entry.path().extension();
		if (extension != ".cpp" && extension != ".h") continue;
		std::ifstream in(entry.path());
		std::string line;
		while (std::getline(in, line)) {
			const auto code = line.substr(0, line.find("//"));
			if (code.find("GetValue") == std::string::npos && code.find("ConfigFlag") == std::string::npos && code.find("Setting") == std::string::npos) continue;
			for (std::sregex_iterator it(code.begin(), code.end(), read), end; it != end; ++it) {
				const auto key = (*it)[1].str();
				found++;
				if (!InCatalog(key)) missing.insert(key + " (" + relative + ")");
			}
		}
	}
	EXPECT_GT(found, 100u);
	for (const auto& key : missing) ADD_FAILURE() << key << " is read by the code but not in SettingsCatalog.cpp";
}
