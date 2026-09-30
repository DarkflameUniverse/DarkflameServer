#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "GameText.h"
#include "HTTPContext.h"
#include "Locale.h"

using namespace GameText;

namespace {
	const std::vector<std::string> LANGUAGES{ "en_US", "de_DE", "en_GB" };

	// A small locale.xml like the client's: three locales, a few phrases (some without every translation)
	class GameTextLocaleTest : public ::testing::Test {
	protected:
		void SetUp() override {
			m_Path = std::filesystem::temp_directory_path() / "dlu_gametext_test_locale.xml";
			std::ofstream out(m_Path);
			out << R"(<?xml version="1.0" encoding="UTF-8"?>
<localization version="1.2">
	<locales count="3"><locale>en_US</locale><locale>de_DE</locale><locale>en_GB</locale></locales>
	<phrases count="5">
		<phrase id="ZoneTable_1200_DisplayDescription"><translation locale="en_US">Test Station</translation><translation locale="de_DE">Test-Station</translation><translation locale="en_GB">Test Station</translation></phrase>
		<phrase id="ZoneTable_1100_DisplayDescription"><translation locale="en_US">Test Gardens</translation></phrase>
		<phrase id="Objects_1727_name"><translation locale="en_US">Test Hat</translation><translation locale="de_DE">Testhut</translation></phrase>
		<phrase id="UI_COINS"><translation locale="en_US">Coins</translation><translation locale="de_DE">Muenzen</translation></phrase>
		<phrase id="MissionEmail_1_subjectText"><translation locale="en_US">Hello</translation><translation locale="de_DE">Hallo</translation></phrase>
	</phrases>
</localization>)";
			out.close();
			Locale::LoadFromFile(m_Path.string(), "en_US", true);
			Init();
		}

		void TearDown() override {
			std::filesystem::remove(m_Path);
		}

		std::filesystem::path m_Path;
	};
}

TEST(GameTextTest, NormalizesTags) {
	EXPECT_EQ(NormalizeTag("de-DE"), "de_DE");
	EXPECT_EQ(NormalizeTag(" EN-gb "), "en_GB");
	EXPECT_EQ(NormalizeTag("de"), "de");
}

TEST(GameTextTest, MatchesAcceptLanguage) {
	EXPECT_EQ(MatchAcceptLanguage("de-DE,de;q=0.9,en;q=0.8", LANGUAGES), "de_DE");
	EXPECT_EQ(MatchAcceptLanguage("en-GB", LANGUAGES), "en_GB");
	// Same language, other region: the first listed of that language
	EXPECT_EQ(MatchAcceptLanguage("de-CH", LANGUAGES), "de_DE");
	EXPECT_EQ(MatchAcceptLanguage("en-AU,en;q=0.5", LANGUAGES), "en_US");
	// Weights win over order
	EXPECT_EQ(MatchAcceptLanguage("fr;q=0.9,de;q=0.95", LANGUAGES), "de_DE");
	EXPECT_EQ(MatchAcceptLanguage("en;q=0.2, de-AT", LANGUAGES), "de_DE");
	// Nothing fitting, refused (q=0) or junk
	EXPECT_EQ(MatchAcceptLanguage("fr-FR,fr;q=0.9", LANGUAGES), std::nullopt);
	EXPECT_EQ(MatchAcceptLanguage("de;q=0", LANGUAGES), std::nullopt);
	EXPECT_EQ(MatchAcceptLanguage("*", LANGUAGES), std::nullopt);
	EXPECT_EQ(MatchAcceptLanguage("", LANGUAGES), std::nullopt);
	EXPECT_EQ(MatchAcceptLanguage("de;q=abc", LANGUAGES), std::nullopt);
}

TEST(GameTextTest, ReadsCookies) {
	EXPECT_EQ(CookieValue("token=abc; game_lang=de_DE; x=1", "game_lang"), "de_DE");
	EXPECT_EQ(CookieValue("game_lang=en_GB", "game_lang"), "en_GB");
	EXPECT_EQ(CookieValue("my_game_lang=de_DE", "game_lang"), "");
	EXPECT_EQ(CookieValue("", "game_lang"), "");
}

TEST(GameTextTest, ChoosesLanguage) {
	EXPECT_EQ(ChooseLanguage("de_DE", "en-US", LANGUAGES), "de_DE");     // the viewer's pick wins
	EXPECT_EQ(ChooseLanguage("xx_XX", "en-GB", LANGUAGES), "en_GB");     // an unknown pick is ignored
	EXPECT_EQ(ChooseLanguage("", "fr-FR", LANGUAGES), "en_US");          // nothing fits: the default
	EXPECT_EQ(ChooseLanguage("", "", {}), DEFAULT_LANGUAGE);
}

TEST(GameTextTest, BuildsKeys) {
	EXPECT_EQ(Key("Objects", 1727), "Objects_1727_name");
	EXPECT_EQ(Key("Missions", 5, "description"), "Missions_5_description");
}

TEST_F(GameTextLocaleTest, LoadsEveryLocale) {
	EXPECT_EQ(Languages(), LANGUAGES);
	EXPECT_EQ(Locale::GetPhrase("Objects_1727_name"), "Test Hat");
	EXPECT_EQ(Locale::GetPhrase("Objects_1727_name", "de_DE"), "Testhut");
	// Falls back to the default locale
	EXPECT_EQ(Locale::GetPhrase("Objects_1727_name", "en_GB"), "Test Hat");
	EXPECT_EQ(Locale::GetPhrase("Objects_1727_name", "xx_XX"), "Test Hat");
	EXPECT_EQ(Locale::GetPhrase("Nope", "de_DE"), "");
}

TEST_F(GameTextLocaleTest, LooksUpInTheViewersLanguage) {
	EXPECT_EQ(Language(), DEFAULT_LANGUAGE);
	EXPECT_EQ(ZoneName(1200), "Test Station");
	EXPECT_EQ(Name("Objects", 1727), "Test Hat");
	{
		LanguageScope german("de_DE");
		EXPECT_EQ(Language(), "de_DE");
		EXPECT_EQ(ZoneName(1200), "Test-Station");
		EXPECT_EQ(ZoneName(1100), "Test Gardens"); // no German name: the default's
		EXPECT_EQ(ZoneNames()["1200"], "Test-Station");
		EXPECT_EQ(Name("Objects", 1727), "Testhut");
		EXPECT_EQ(Terms()["coins"], "Muenzen");
		EXPECT_EQ(Expand("%[MissionEmail_1_subjectText]!"), "Hallo!");
		EXPECT_EQ(PageJson()["language"], "de_DE");
	}
	EXPECT_EQ(Language(), DEFAULT_LANGUAGE);
	EXPECT_EQ(Terms()["coins"], "Coins");
}

TEST_F(GameTextLocaleTest, FallsBackToTheKeyOrId) {
	EXPECT_EQ(ZoneName(9999), "Zone 9999");
	EXPECT_EQ(ZoneName(0), "Character Select");
	EXPECT_EQ(Name("Objects", 42), "Objects 42");
	EXPECT_EQ(Phrase("UI_NOT_THERE"), "");
	EXPECT_EQ(TextOrKey("UI_NOT_THERE"), "UI_NOT_THERE");
	EXPECT_EQ(Text("UI_NOT_THERE", "fallback"), "fallback");
	EXPECT_EQ(Terms()["reputation"], "UI_REPUTATION");
	EXPECT_EQ(Expand("%[Unknown] ok"), "%[Unknown] ok");
}

TEST_F(GameTextLocaleTest, PicksTheRequestsLanguage) {
	HTTPContext context;
	context.headers["accept-language"] = "de-AT,de;q=0.9";
	{
		LanguageScope scope(context);
		EXPECT_EQ(Language(), "de_DE");
	}
	context.headers["cookie"] = "game_lang=en_GB";
	{
		LanguageScope scope(context);
		EXPECT_EQ(Language(), "en_GB");
	}
	EXPECT_EQ(Language(), DEFAULT_LANGUAGE);
}

TEST_F(GameTextLocaleTest, EveryTermHasAKey) {
	for (const auto& [name, key] : TermKeys()) {
		EXPECT_FALSE(name.empty());
		EXPECT_TRUE(key.starts_with("UI_")) << key;
	}
}
