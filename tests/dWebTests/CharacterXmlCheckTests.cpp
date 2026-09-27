#include <gtest/gtest.h>

#include <algorithm>

#include "CharacterXmlCheck.h"

namespace {
	// Shaped like what the server writes (UserManager's new character plus what the components save), made-up values
	const std::string VALID =
		"<obj v=\"1\">"
		"<mf hc=\"1\" hs=\"2\" hd=\"0\" t=\"3\" l=\"4\" hdc=\"0\" cd=\"5\" lh=\"6\" rh=\"7\" es=\"8\" ess=\"9\" ms=\"10\"/>"
		"<char acct=\"7\" cc=\"150\" gm=\"0\" ft=\"0\" llog=\"1700000000\" ls=\"150\" lzx=\"-626.5\" lzy=\"613.3\" lzz=\"-28.6\" "
		"lzrx=\"0.0\" lzry=\"0.7\" lzrz=\"0.0\" lzrw=\"0.7\" lzid=\"1000\" lnzid=\"1000\" rpt=\"0\" time=\"3600\" stt=\"0;0;0;\">"
		"<ue><e id=\"1\"/></ue><vl><l id=\"1000\" cid=\"0\"/></vl><zs><s map=\"1000\" ac=\"1\" bc=\"2\" cc=\"3\" es=\"4\" qbc=\"5\"/></zs></char>"
		"<dest hm=\"4\" hc=\"4\" im=\"0\" ic=\"0\" am=\"0\" ac=\"0\" d=\"0\"><buff><b id=\"5\" t=\"10\" tk=\"0\" tt=\"0\" s=\"1\" sr=\"0\" b=\"0\" refCount=\"1\" cancelOnDeath=\"1\"/></buff></dest>"
		"<inv csl=\"-1\"><bag><b t=\"0\" m=\"20\"/><b t=\"1\" m=\"40\"/><b t=\"2\" m=\"240\"/><b t=\"5\" m=\"240\"/></bag>"
		"<items><in t=\"0\"><i l=\"1001\" id=\"100\" s=\"0\" c=\"1\" eq=\"1\" b=\"1\" sk=\"0\"/><i l=\"1002\" id=\"101\" s=\"1\" c=\"1\" eq=\"1\" b=\"1\"/></in>"
		"<in t=\"2\"><i l=\"2000\" id=\"102\" s=\"0\" c=\"500\" eq=\"0\" b=\"0\"/></in></items></inv>"
		"<lvl l=\"2\" cv=\"1\" sb=\"500\"/>"
		"<flag><f id=\"1\" v=\"18446744073709551615\"/><s si=\"42\"/></flag>"
		"<mis><done><m id=\"1\" state=\"8\" cct=\"1\" cts=\"1700000000\"/></done><cur><m id=\"2\" state=\"2\" o=\"1\"><sv v=\"3\"/></m></cur></mis>"
		"<pet><p id=\"200\" l=\"3000\" m=\"0\" n=\"Rex\" t=\"0\"/></pet>"
		"<res><r w=\"1100\" x=\"1\" y=\"2\" z=\"3\"/></res>"
		"</obj>";

	CharacterXmlCheck::Context MakeContext() {
		CharacterXmlCheck::Context context;
		context.ownerAccountId = 7;
		auto& l = context.lookups;
		l.isItem = [](LOT lot) { return lot == 1001 || lot == 1002 || lot == 2000 || lot == 6666; };
		l.stackSize = [](LOT lot) { return lot == 2000 ? 999 : 1; };
		l.missionExists = [](int32_t id) { return id == 1 || id == 2; };
		l.levelUScore = [](uint32_t level) -> std::optional<int64_t> {
			if (level == 1) return 0;
			if (level == 2) return 100;
			if (level == 3) return 300;
			return std::nullopt;
		};
		l.maxLevel = 3;
		return context;
	}

	std::string Replace(std::string text, const std::string& from, const std::string& to) {
		const auto at = text.find(from);
		EXPECT_NE(at, std::string::npos) << from;
		if (at != std::string::npos) text.replace(at, from.size(), to);
		return text;
	}

	bool Mentions(const std::vector<std::string>& list, const std::string& part) {
		return std::any_of(list.begin(), list.end(), [&](const std::string& s) { return s.find(part) != std::string::npos; });
	}

	CharacterXmlCheck::Result Check(const std::string& xml) { return CharacterXmlCheck::Check(xml, MakeContext()); }

	void ExpectError(const std::string& xml, const std::string& part) {
		const auto result = Check(xml);
		EXPECT_FALSE(result.Ok()) << part;
		EXPECT_TRUE(Mentions(result.errors, part)) << "expected an error mentioning: " << part << (result.errors.empty() ? "" : " got: " + result.errors.front());
	}

	void ExpectWarning(const std::string& xml, const std::string& part) {
		const auto result = Check(xml);
		EXPECT_TRUE(result.Ok()) << (result.errors.empty() ? "" : result.errors.front());
		EXPECT_TRUE(Mentions(result.warnings, part)) << "expected a warning mentioning: " << part;
	}
}

TEST(CharacterXmlCheckTests, ValidCharacterPasses) {
	const auto result = Check(VALID);
	EXPECT_TRUE(result.errors.empty()) << (result.errors.empty() ? "" : result.errors.front());
	EXPECT_TRUE(result.warnings.empty()) << (result.warnings.empty() ? "" : result.warnings.front());
}

TEST(CharacterXmlCheckTests, Structure) {
	ExpectError("", "empty");
	ExpectError("<obj><char>", "not valid XML");
	ExpectError("<root/>", "<obj>");
	ExpectError(Replace(Replace(VALID, "<char acct", "<chr acct"), "</char>", "</chr>"), "<char>");
	ExpectError(Replace(VALID, "<mf ", "<mx "), "<mf>");
	ExpectError(Replace(Replace(VALID, "<bag>", "<bog>"), "</bag>", "</bog>"), "<bag>");
	ExpectError(Replace(VALID, VALID.substr(VALID.find("<items>"), VALID.find("</items>") - VALID.find("<items>")), "<items>"), "at least one <in>");
	ExpectError(Replace(Replace(VALID, "<inv csl", "<inx csl"), "</inv>", "</inx>"), "<inv>");
}

TEST(CharacterXmlCheckTests, Numbers) {
	ExpectError(Replace(VALID, "cc=\"150\"", "cc=\"lots\""), "cc=\"lots\"");
	ExpectError(Replace(VALID, "cc=\"150\"", "cc=\"-5\""), "negative");
	ExpectError(Replace(VALID, "ls=\"150\"", "ls=\"-1\""), "negative");
	ExpectError(Replace(VALID, "lzx=\"-626.5\"", "lzx=\"west\""), "lzx");
	ExpectError(Replace(VALID, "hc=\"1\"", "hc=\"red\""), "<mf>");
	ExpectError(Replace(VALID, "hm=\"4\"", "hm=\"full\""), "<dest>");
	ExpectError(Replace(VALID, "refCount=\"1\"", "refCount=\"x\""), "<buff>");
	ExpectError(Replace(VALID, "<l id=\"1000\"", "<l id=\"ag\""), "<vl>");
	ExpectError(Replace(VALID, "<e id=\"1\"/>", "<e/>"), "emote: id is missing");
	ExpectError(Replace(VALID, "map=\"1000\" ", ""), "map is missing");
	ExpectError(Replace(VALID, "<r w=\"1100\"", "<r w=\"x\""), "<res>");
	ExpectError(Replace(VALID, "l=\"2\" cv", "l=\"two\" cv"), "<lvl>");
	ExpectError(Replace(VALID, "csl=\"-1\"", "csl=\"none\""), "csl");
	ExpectError(Replace(VALID, "<p id=\"200\"", "<p id=\"pet\""), "<pet>");
}

TEST(CharacterXmlCheckTests, Flags) {
	// The game reads these with std::stoul/std::stoull, which throw
	ExpectError(Replace(VALID, "<f id=\"1\"", "<f id=\"one\""), "<flag>: id");
	ExpectError(Replace(VALID, "v=\"18446744073709551615\"", "v=\"18446744073709551616\""), "<flag>: v");
	ExpectError(Replace(VALID, "<f id=\"1\"", "<f id=\"-1\""), "<flag>: id");
}

TEST(CharacterXmlCheckTests, Enums) {
	ExpectError(Replace(VALID, "gm=\"0\"", "gm=\"12\""), "not a GM level");
	ExpectError(Replace(VALID, "cv=\"1\"", "cv=\"99\""), "newer than");
	ExpectError(Replace(VALID, "<b t=\"5\"", "<b t=\"40\""), "not an inventory type");
	ExpectError(Replace(VALID, "<in t=\"2\">", "<in t=\"17\">"), "not an inventory type");
	ExpectError(Replace(VALID, "state=\"2\"", "state=\"3\""), "not a mission state");
}

TEST(CharacterXmlCheckTests, Items) {
	ExpectError(Replace(VALID, " s=\"1\" c=\"1\"", " c=\"1\""), "s is missing");
	ExpectError(Replace(VALID, "c=\"500\" eq=\"0\"", "c=\"500\""), "eq is missing");
	ExpectError(Replace(VALID, "id=\"101\"", "id=\"abc\""), "id=\"abc\"");
	ExpectError(Replace(VALID, "id=\"101\"", "id=\"0\""), "id can't be 0");
	ExpectError(Replace(VALID, "id=\"101\"", "id=\"100\""), "used by another item");
	ExpectError(Replace(VALID, "id=\"101\" s=\"1\"", "id=\"101\" s=\"0\""), "slot 0");
	ExpectError(Replace(VALID, "l=\"1002\"", "l=\"4242\""), "not an item");
	ExpectError(Replace(VALID, "c=\"500\"", "c=\"-3\""), "c=\"-3\"");
	// Build inventories are re-slotted when loading, so shared slots there are fine
	const auto bbb = Replace(VALID, "</items>", "<in t=\"3\"><i l=\"2000\" id=\"300\" s=\"0\" c=\"1\" eq=\"0\" b=\"0\"/><i l=\"2000\" id=\"301\" s=\"0\" c=\"1\" eq=\"0\" b=\"0\"/></in></items>");
	EXPECT_TRUE(Check(bbb).Ok());
}

TEST(CharacterXmlCheckTests, MissionsAndOwnership) {
	ExpectError(Replace(VALID, "<m id=\"2\"", "<m id=\"99\""), "no such mission");
	ExpectError(Replace(VALID, "<m id=\"1\" ", "<m "), "id is missing");
	ExpectError(Replace(VALID, "acct=\"7\"", "acct=\"8\""), "not the account");
}

TEST(CharacterXmlCheckTests, Warnings) {
	ExpectWarning(Replace(VALID, "c=\"500\"", "c=\"1000\""), "stack size");
	ExpectWarning(Replace(VALID, "c=\"500\"", "c=\"5000000\""), "is above 999999");
	ExpectWarning(Replace(VALID, "c=\"500\"", "c=\"0\""), "count is 0");
	ExpectWarning(Replace(VALID, "cc=\"150\"", "cc=\"3000000000\""), "Coins");
	ExpectWarning(Replace(VALID, "l=\"2\" cv", "l=\"9\" cv"), "above the highest level");
	ExpectWarning(Replace(VALID, "ls=\"150\"", "ls=\"50\""), "needs 100 u-score");
	ExpectWarning(Replace(VALID, "ls=\"150\"", "ls=\"400\""), "enough for level 3");
	ExpectWarning(Replace(VALID, "gm=\"0\"", "gm=\"8\""), "above the account");
	ExpectWarning(Replace(VALID, "state=\"8\" cct", "state=\"2\" cct"), "not complete");
}

TEST(CharacterXmlCheckTests, Contraband) {
	auto context = MakeContext();
	context.contraband[2000] = { "duped", IContraband::eContrabandAction::REMOVE };
	context.contraband[1001] = { "", IContraband::eContrabandAction::FLAG };
	auto result = CharacterXmlCheck::Check(VALID, context);
	ASSERT_TRUE(result.Ok());
	ASSERT_EQ(result.contraband.size(), 2u);
	EXPECT_EQ(result.contraband[0].item.id, 100);
	EXPECT_EQ(result.contraband[1].item.lot, 2000);
	EXPECT_EQ(result.contraband[1].item.count, 500u);
	EXPECT_EQ(result.contraband[1].item.inventory, eInventoryType::BRICKS);
	EXPECT_TRUE(Mentions(result.warnings, "flag and remove"));

	// Staff accounts aren't checked (contraband_ignore_staff), like in the world
	context.contrabandApplies = false;
	EXPECT_TRUE(CharacterXmlCheck::Check(VALID, context).contraband.empty());

	// Removing drops the item and any set items that belong to it
	const auto withProxy = Replace(VALID, "</in></items>", "<i l=\"1002\" id=\"103\" s=\"1\" c=\"1\" eq=\"0\" b=\"0\" parent=\"102\"/></in></items>");
	const auto removed = CharacterXmlCheck::RemoveItems(withProxy, { 102 });
	EXPECT_EQ(removed.find("id=\"102\""), std::string::npos);
	EXPECT_EQ(removed.find("id=\"103\""), std::string::npos);
	EXPECT_NE(removed.find("id=\"101\""), std::string::npos);
	EXPECT_TRUE(Check(removed).Ok());
}
