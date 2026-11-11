// Charxml fields the client reads on load that DLU now saves. The formats are the ones live wrote (2011/2012 live
// captures of the charxml the world server sent) and the client reads (legouniverse.exe 1.10.64).
#include "GameDependencies.h"

#include "CDClientDatabase.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "PlayerMessages.h"
#include "Entity.h"
#include "User.h"
#include "dConfig.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "SkillComponent.h"
#include "InventoryComponent.h"
#include "DatabasePet.h"
#include "CDSkillBehaviorTable.h"
#include "CDClientManager.h"
#include "eMissionLockState.h"

#include "tinyxml2.h"

#include <ctime>
#include <string>

#include <gtest/gtest.h>

namespace {
	std::string Print(const tinyxml2::XMLDocument& doc) {
		tinyxml2::XMLPrinter printer(nullptr, true);
		doc.Print(&printer);
		return printer.CStr();
	}

	void Parse(tinyxml2::XMLDocument& doc, const std::string& xml) {
		ASSERT_EQ(doc.Parse(xml.c_str()), tinyxml2::XML_SUCCESS);
	}
}

class CharacterSaveFieldsTests : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
		CDClientDatabase::Connect(":memory:"); // MissionComponent counts the achievements
		CDClientDatabase::ExecuteDML("CREATE TABLE Missions (id INTEGER, isMission INTEGER);");
	}
	void TearDown() override { TearDownDependencies(); }
};

// <mis><ts>: the states of the mission journal tabs, written after <cur> the way live wrote them.
TEST_F(CharacterSaveFieldsTests, MissionTypeStatesRoundTrip) {
	tinyxml2::XMLDocument doc;
	Parse(doc, R"(<obj v="1"><mis><done/><cur/><ts><type v="Build"><st sub="" val="1"/></type><type v="Location"><st sub="Avant Gardens" val="1"/><st sub="Nimbus Station" val="2"/></type></ts></mis></obj>)");

	Entity player(20, info);
	auto* const missions = player.AddComponent<MissionComponent>(-1);
	missions->LoadFromSaveData(doc);
	const auto& states = missions->GetMissionTypeStates();
	ASSERT_EQ(states.size(), 2);
	EXPECT_EQ(states.at("Build").at(""), eMissionLockState::NEW);
	EXPECT_EQ(states.at("Location").at("Avant Gardens"), eMissionLockState::NEW);
	EXPECT_EQ(states.at("Location").at("Nimbus Station"), eMissionLockState::UNLOCKED);

	missions->SetMissionTypeState("Battle", "General", eMissionLockState::NEW);
	missions->WriteSaveData(doc);
	EXPECT_EQ(Print(doc), R"(<obj v="1"><mis><done/><cur/><ts><type v="Battle"><st sub="General" val="1"/></type><type v="Build"><st sub="" val="1"/></type><type v="Location"><st sub="Avant Gardens" val="1"/><st sub="Nimbus Station" val="2"/></type></ts></mis></obj>)");

	Entity reloaded(21, info);
	auto* const reloadedMissions = reloaded.AddComponent<MissionComponent>(-1);
	reloadedMissions->LoadFromSaveData(doc);
	EXPECT_EQ(reloadedMissions->GetMissionTypeStates(), states);
}

// Saves from before <ts> was written load with no states and gain an empty <ts>.
TEST_F(CharacterSaveFieldsTests, MissionTypeStatesMissingInOldSave) {
	tinyxml2::XMLDocument doc;
	Parse(doc, R"(<obj v="1"><mis><done/><cur/></mis></obj>)");

	Entity player(22, info);
	auto* const missions = player.AddComponent<MissionComponent>(-1);
	missions->LoadFromSaveData(doc);
	EXPECT_TRUE(missions->GetMissionTypeStates().empty());
	missions->WriteSaveData(doc);
	EXPECT_EQ(Print(doc), R"(<obj v="1"><mis><done/><cur/><ts/></mis></obj>)");
}

// char@ttip: the tooltip bits (the client reads it with GetLongLongValue). Live wrote it on every character.
TEST_F(CharacterSaveFieldsTests, TooltipFlagsRoundTrip) {
	tinyxml2::XMLDocument doc;
	Parse(doc, R"(<obj v="1"><mf/><char ls="0" ttip="16777216"/></obj>)");

	Character character(1, nullptr);
	Entity player(23, info);
	auto* const characterComponent = player.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS);
	characterComponent->LoadFromSaveData(doc);
	EXPECT_EQ(characterComponent->GetTooltipFlags(), 16777216u);

	characterComponent->SetTooltipFlag(2, true);
	characterComponent->WriteSaveData(doc);
	EXPECT_STREQ(doc.FirstChildElement("obj")->FirstChildElement("char")->Attribute("ttip"), "16777220");

	Entity reloaded(24, info);
	auto* const reloadedComponent = reloaded.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS);
	reloadedComponent->LoadFromSaveData(doc);
	EXPECT_EQ(reloadedComponent->GetTooltipFlags(), 16777220u);
}

// Saves from before ttip was written load with no tooltips flagged and gain ttip="0".
TEST_F(CharacterSaveFieldsTests, TooltipFlagsMissingInOldSave) {
	tinyxml2::XMLDocument doc;
	Parse(doc, R"(<obj v="1"><mf/><char ls="0"/></obj>)");

	Character character(1, nullptr);
	Entity player(25, info);
	auto* const characterComponent = player.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS);
	characterComponent->LoadFromSaveData(doc);
	EXPECT_EQ(characterComponent->GetTooltipFlags(), 0u);
	characterComponent->WriteSaveData(doc);
	EXPECT_STREQ(doc.FirstChildElement("obj")->FirstChildElement("char")->Attribute("ttip"), "0");
}

// The <char> attributes live wrote on every character and the client reads: cm, edit, mldt, gid, gn, v, vd and the
// last respawn point. A save that has them keeps what the server does not own, and nothing touches the coins.
TEST_F(CharacterSaveFieldsTests, LiveCharacterAttributesRoundTrip) {
	tinyxml2::XMLDocument doc;
	Parse(doc, R"(<obj v="1"><mf/><char cc="22609" cm="5000" ls="0" mldt="1301715419" v="1" vd="1" lrx="1" lry="2" lrz="3" lrrx="0" lrry="0.5" lrrz="0" lrrw="0.5" stime="349" lzcs="2233038349"/></obj>)");

	Character character(1, nullptr);
	Entity player(40, info);
	auto* const characterComponent = player.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS);
	characterComponent->LoadFromSaveData(doc);
	EXPECT_EQ(characterComponent->GetVotesLeft(), 1);
	EXPECT_EQ(characterComponent->GetVoteDay(), 1u);

	characterComponent->WriteSaveData(doc);
	const auto* const saved = doc.FirstChildElement("obj")->FirstChildElement("char");
	EXPECT_STREQ(saved->Attribute("cc"), "22609");
	EXPECT_STREQ(saved->Attribute("cm"), "5000"); // kept, not replaced and not applied
	EXPECT_STREQ(saved->Attribute("mldt"), "1301715419");
	EXPECT_STREQ(saved->Attribute("edit"), "0");
	EXPECT_STREQ(saved->Attribute("gid"), "0");
	EXPECT_STREQ(saved->Attribute("gn"), "");
	// A new day: the votes are filled up
	const uint64_t today = static_cast<uint64_t>(std::time(nullptr)) / 86400;
	EXPECT_LE(saved->Unsigned64Attribute("vd") - today, 1u); // (the day may turn during the test)
	EXPECT_STREQ(saved->Attribute("v"), "3");
	// No respawn point was reached in this world: the saved one stays
	EXPECT_STREQ(saved->Attribute("lrx"), "1");
	EXPECT_STREQ(saved->Attribute("lrrw"), "0.5");
	// The session goes on (no other login is known): its seconds are kept
	EXPECT_GE(saved->Unsigned64Attribute("stime"), 349u);
	EXPECT_LE(saved->Unsigned64Attribute("stime"), 351u);
	// Not written by the server: left alone
	EXPECT_STREQ(saved->Attribute("lzcs"), "2233038349");

	// The same day: the votes left are kept
	Entity reloaded(41, info);
	auto* const reloadedComponent = reloaded.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS);
	doc.FirstChildElement("obj")->FirstChildElement("char")->SetAttribute("v", 2);
	reloadedComponent->LoadFromSaveData(doc);
	EXPECT_EQ(reloadedComponent->GetVotesLeft(), 2);
	reloadedComponent->SetGuild(77, u"Builders");
	reloadedComponent->WriteSaveData(doc);
	EXPECT_STREQ(saved->Attribute("v"), "2");
	EXPECT_STREQ(saved->Attribute("gid"), "77");
	EXPECT_STREQ(saved->Attribute("gn"), "Builders");
}

// edit, the editor level: the account's GM level for an account at editor_gm_level (9 unless set) or above, 0 for
// everyone else
TEST_F(CharacterSaveFieldsTests, EditAttributeFollowsTheAccountGmLevel) {
	using enum eGameMasterLevel;
	EXPECT_EQ(CharacterComponent::EditAttribute(OPERATOR, 9), 9u);
	EXPECT_EQ(CharacterComponent::EditAttribute(DEVELOPER, 9), 0u);
	EXPECT_EQ(CharacterComponent::EditAttribute(CIVILIAN, 9), 0u);
	EXPECT_EQ(CharacterComponent::EditAttribute(DEVELOPER, 8), 8u);
	EXPECT_EQ(CharacterComponent::EditAttribute(OPERATOR, 8), 9u);
	EXPECT_EQ(CharacterComponent::EditAttribute(JUNIOR_DEVELOPER, 8), 0u);
	// A civilian never, whatever the setting
	EXPECT_EQ(CharacterComponent::EditAttribute(CIVILIAN, 0), 0u);

	const auto savedEdit = [this](const eGameMasterLevel accountLevel) {
		tinyxml2::XMLDocument doc;
		Parse(doc, R"(<obj v="1"><mf/><char ls="0" edit="0"/></obj>)");
		User user(UNASSIGNED_SYSTEM_ADDRESS, "editor", "key");
		user.SetMaxGMLevel(accountLevel);
		Character character(1, &user);
		Entity player(43, info);
		auto* const characterComponent = player.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS);
		characterComponent->LoadFromSaveData(doc);
		characterComponent->WriteSaveData(doc);
		return std::string(doc.FirstChildElement("obj")->FirstChildElement("char")->Attribute("edit"));
	};
	// The default level, 9
	EXPECT_EQ(savedEdit(OPERATOR), "9");
	EXPECT_EQ(savedEdit(DEVELOPER), "0");
	EXPECT_EQ(savedEdit(CIVILIAN), "0");

	// editor_gm_level from the settings
	Game::config->SetDatabaseValues({ { "editor_gm_level", "8" } }, {});
	EXPECT_EQ(savedEdit(DEVELOPER), "8");
	EXPECT_EQ(savedEdit(JUNIOR_DEVELOPER), "0");
	Game::config->SetDatabaseValues({}, {});
}

// Saves from before they were written load as before and gain them with the values live had: the coins are not
// touched and cm is the client's own "no limit".
TEST_F(CharacterSaveFieldsTests, LiveCharacterAttributesMissingInOldSave) {
	tinyxml2::XMLDocument doc;
	Parse(doc, R"(<obj v="1"><mf/><char acct="1" cc="123456789012" gm="0" ls="15700" ttip="4" time="54390"/></obj>)");

	Character character(1, nullptr);
	Entity player(42, info);
	auto* const characterComponent = player.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS);
	characterComponent->LoadFromSaveData(doc);
	EXPECT_EQ(characterComponent->GetVotesLeft(), 3);
	EXPECT_EQ(characterComponent->GetTooltipFlags(), 4u);
	EXPECT_EQ(characterComponent->GetUScore(), 15700);

	characterComponent->WriteSaveData(doc);
	const auto* const saved = doc.FirstChildElement("obj")->FirstChildElement("char");
	EXPECT_STREQ(saved->Attribute("cc"), "123456789012");
	EXPECT_STREQ(saved->Attribute("acct"), "1");
	EXPECT_STREQ(saved->Attribute("ls"), "15700");
	EXPECT_STREQ(saved->Attribute("cm"), "9223372036854775807");
	EXPECT_STREQ(saved->Attribute("edit"), "0");
	EXPECT_STREQ(saved->Attribute("mldt"), "0");
	EXPECT_STREQ(saved->Attribute("gid"), "0");
	EXPECT_STREQ(saved->Attribute("gn"), "");
	EXPECT_STREQ(saved->Attribute("v"), "3");
	EXPECT_NE(saved->Attribute("vd"), nullptr);
	EXPECT_EQ(saved->Attribute("lrx"), nullptr); // no respawn point reached yet
	EXPECT_NE(saved->Attribute("stime"), nullptr);
}

namespace {
	// Loads <char> with the given attributes for a character whose login (charinfo.last_login) is sessionLogin, lets
	// seconds pass and saves. Returns the saved <char>.
	const tinyxml2::XMLElement* LoadPlayAndSave(tinyxml2::XMLDocument& doc, Character& character, CharacterComponent& component,
		const std::string& attributes, const uint64_t sessionLogin, const int64_t seconds) {
		const auto xml = R"(<obj v="1"><mf/><char acct="1" cc="5" )" + attributes + "/></obj>";
		EXPECT_EQ(doc.Parse(xml.c_str()), tinyxml2::XML_SUCCESS);
		character.SetSessionLogin(sessionLogin);
		component.LoadFromSaveData(doc);
		component.SetLastUpdateTimestamp(std::time(nullptr) - seconds);
		component.WriteSaveData(doc);
		return doc.FirstChildElement("obj")->FirstChildElement("char");
	}

	// The seconds asked for, or one more when the clock ticked during the test
	void ExpectAbout(const uint64_t value, const uint64_t expected) {
		EXPECT_GE(value, expected);
		EXPECT_LE(value, expected + 1);
	}
}

// Play time as live saved it: time is the total before the session, stime the session's seconds, and the client
// shows time + stime (LWOCharacterComponent::LoadFromSaveData 0x00ca4910, SendMessage 0x00d34330 GetTimePlayed).
// A save from before stime was written has every past session in time: the total stays what it was.
TEST_F(CharacterSaveFieldsTests, OldSavePlayTimeKeepsItsTotal) {
	tinyxml2::XMLDocument doc;
	Character character(1, nullptr);
	Entity player(60, info);
	auto* const component = player.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS);
	const auto* const saved = LoadPlayAndSave(doc, character, *component, R"(llog="1700000000" time="54390")", 1700090000, 0);
	EXPECT_EQ(component->GetTotalTimePlayed() - component->GetSessionTimePlayed(), 54390u);
	ExpectAbout(component->GetTotalTimePlayed(), 54390);
	EXPECT_STREQ(saved->Attribute("time"), "54390");
	ExpectAbout(saved->Unsigned64Attribute("stime"), 0);
	EXPECT_STREQ(saved->Attribute("llog"), "1700090000");
	EXPECT_EQ(saved->Unsigned64Attribute("time") + saved->Unsigned64Attribute("stime"), component->GetTotalTimePlayed());

	// The same without a known login (no charinfo row): nothing is lost and llog is left alone
	tinyxml2::XMLDocument unknownDoc;
	Character unknown(2, nullptr);
	Entity other(61, info);
	auto* const otherComponent = other.AddComponent<CharacterComponent>(-1, &unknown, UNASSIGNED_SYSTEM_ADDRESS);
	const auto* const otherSaved = LoadPlayAndSave(unknownDoc, unknown, *otherComponent, R"(llog="1700000000" time="54390")", 0, 0);
	EXPECT_STREQ(otherSaved->Attribute("time"), "54390");
	EXPECT_STREQ(otherSaved->Attribute("llog"), "1700000000");
	ExpectAbout(otherComponent->GetTotalTimePlayed(), 54390);
}

// A session in progress: the seconds since the last save go to stime, time stays, and a world transfer in the same
// session (the same login) goes on counting in stime. Live: time=262486 with stime=6795, then 7259 in the next world.
TEST_F(CharacterSaveFieldsTests, SessionPlayTimeGrowsInStime) {
	tinyxml2::XMLDocument doc;
	Character character(1, nullptr);
	Entity player(62, info);
	auto* const component = player.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS);
	const auto* saved = LoadPlayAndSave(doc, character, *component, R"(llog="1327837739" time="262486" stime="6795")", 1327837739, 464);
	EXPECT_STREQ(saved->Attribute("time"), "262486");
	ExpectAbout(saved->Unsigned64Attribute("stime"), 7259);
	EXPECT_STREQ(saved->Attribute("llog"), "1327837739");
	ExpectAbout(component->GetTotalTimePlayed(), 262486 + 7259);
	ExpectAbout(component->GetSessionTimePlayed(), 7259);

	// A second save in the same world adds only what passed since the first
	component->SetLastUpdateTimestamp(std::time(nullptr) - 10);
	component->WriteSaveData(doc);
	EXPECT_STREQ(saved->Attribute("time"), "262486");
	EXPECT_GE(saved->Unsigned64Attribute("stime"), 7269u);
	EXPECT_LE(saved->Unsigned64Attribute("stime"), 7271u);

	// The next world of the session loads what this one saved and goes on
	tinyxml2::XMLPrinter printer(nullptr, true);
	doc.Print(&printer);
	const std::string written = printer.CStr();
	tinyxml2::XMLDocument next;
	ASSERT_EQ(next.Parse(written.c_str()), tinyxml2::XML_SUCCESS);
	Entity inNextWorld(63, info);
	auto* const nextComponent = inNextWorld.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS);
	nextComponent->LoadFromSaveData(next);
	EXPECT_EQ(nextComponent->GetTotalTimePlayed(), component->GetTotalTimePlayed());
	EXPECT_EQ(nextComponent->GetSessionTimePlayed(), component->GetSessionTimePlayed());
	nextComponent->SetLastUpdateTimestamp(std::time(nullptr) - 30);
	nextComponent->WriteSaveData(next);
	const auto* const nextSaved = next.FirstChildElement("obj")->FirstChildElement("char");
	EXPECT_STREQ(nextSaved->Attribute("time"), "262486");
	EXPECT_GE(nextSaved->Unsigned64Attribute("stime"), 7299u);
	EXPECT_LE(nextSaved->Unsigned64Attribute("stime"), 7302u);
}

// The next login: the last session's seconds join time and stime starts over. Live: time=262486 stime=7344 at the
// end of one session, time=269830 in the next. The total is the same before and after.
TEST_F(CharacterSaveFieldsTests, ANewSessionMovesStimeIntoTime) {
	tinyxml2::XMLDocument doc;
	Character character(1, nullptr);
	Entity player(64, info);
	auto* const component = player.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS);
	const auto* const saved = LoadPlayAndSave(doc, character, *component, R"(llog="1327837739" time="262486" stime="7344")", 1327848273, 74);
	EXPECT_STREQ(saved->Attribute("time"), "269830");
	ExpectAbout(saved->Unsigned64Attribute("stime"), 74);
	EXPECT_STREQ(saved->Attribute("llog"), "1327848273");
	ExpectAbout(component->GetTotalTimePlayed(), 269830 + 74);
}

// The client reads stime as a float; a fraction written by something else is read, not refused
TEST_F(CharacterSaveFieldsTests, FractionalStimeIsRead) {
	tinyxml2::XMLDocument doc;
	Character character(1, nullptr);
	Entity player(65, info);
	auto* const component = player.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS);
	const auto* const saved = LoadPlayAndSave(doc, character, *component, R"(time="100" stime="15.6958")", 0, 0);
	EXPECT_STREQ(saved->Attribute("time"), "100");
	ExpectAbout(saved->Unsigned64Attribute("stime"), 15);
}

// <skil sc>: the cooldown groups still running and the seconds left, as live wrote them (sc="17:15.6958;") and the
// client reads them (split on ';' and ':').
TEST_F(CharacterSaveFieldsTests, SkillCooldownsRoundTrip) {
	CDSkillBehavior skill{};
	skill.skillID = 394;
	skill.cooldowngroup = 17;
	skill.cooldown = 30.0f;
	CDClientManager::GetEntriesMutable<CDSkillBehaviorTable>()[394] = skill;
	CDSkillBehavior ungrouped{};
	ungrouped.skillID = 395;
	ungrouped.cooldowngroup = static_cast<uint32_t>(-1);
	ungrouped.cooldown = 30.0f;
	CDClientManager::GetEntriesMutable<CDSkillBehaviorTable>()[395] = ungrouped;

	tinyxml2::XMLDocument doc;
	Parse(doc, R"(<obj v="1"><skil sc="8:26.6559;78:30.7363;"/></obj>)");

	Entity player(26, info);
	auto* const skills = player.AddComponent<SkillComponent>(-1);
	skills->LoadFromSaveData(doc);
	ASSERT_EQ(skills->GetCooldownGroups().size(), 2u);
	EXPECT_FLOAT_EQ(skills->GetCooldownGroups().at(8), 26.6559f);

	skills->Run(26.7f); // group 8 runs out
	skills->StartCooldown(394);
	skills->StartCooldown(395); // no group: not saved
	skills->WriteSaveData(doc);
	EXPECT_EQ(Print(doc), R"(<obj v="1"><skil sc="17:30;78:4.0363;"/></obj>)");

	Entity reloaded(27, info);
	auto* const reloadedSkills = reloaded.AddComponent<SkillComponent>(-1);
	reloadedSkills->LoadFromSaveData(doc);
	EXPECT_EQ(reloadedSkills->GetCooldownGroups().size(), 2u);
	EXPECT_FLOAT_EQ(reloadedSkills->GetCooldownGroups().at(17), 30.0f);

	CDClientManager::GetEntriesMutable<CDSkillBehaviorTable>().clear();
}

// Saves without <skil> load with no cooldowns and gain <skil/>, as live wrote it with none running.
TEST_F(CharacterSaveFieldsTests, SkillCooldownsMissingInOldSave) {
	tinyxml2::XMLDocument doc;
	Parse(doc, R"(<obj v="1"/>)");

	Entity player(28, info);
	auto* const skills = player.AddComponent<SkillComponent>(-1);
	skills->LoadFromSaveData(doc);
	EXPECT_TRUE(skills->GetCooldownGroups().empty());
	skills->WriteSaveData(doc);
	EXPECT_EQ(Print(doc), R"(<obj v="1"><skil/></obj>)");
}

// <pet a="0">: live wrote the active pet as 0 on every character (the charxml is read at load, before any pet is out),
// and each pet's taming type t as 0. Old saves without a load as before.
TEST_F(CharacterSaveFieldsTests, PetsAsLiveWroteThem) {
	CDClientDatabase::ExecuteDML("CREATE TABLE ComponentsRegistry (id INTEGER, component_type INTEGER, component_id INTEGER);");
	tinyxml2::XMLDocument doc;
	Parse(doc, R"(<obj v="1"><pet><p id="1152921510000000001" l="3254" t="0" n="Fluffy" m="2"/></pet></obj>)");

	Entity player(29, info);
	auto* const inventory = player.AddComponent<InventoryComponent>(-1);
	inventory->LoadXml(doc); // the pets load first; there is no <inv> here
	EXPECT_EQ(inventory->GetDatabasePet(1152921510000000001LL).lot, 3254);
	inventory->WriteSaveData(doc);
	EXPECT_EQ(Print(doc), R"(<obj v="1"><pet a="0"><p id="1152921510000000001" l="3254" m="2" n="Fluffy" t="0"/></pet></obj>)");
}

// The editor level is told to the client with the character: after the GM level, whether the editor is on and at
// which level
TEST_F(CharacterSaveFieldsTests, EditorLevelIsSerializedAfterTheGmLevel) {
	const auto gmBlock = [this](const eGameMasterLevel accountLevel) {
		User user(UNASSIGNED_SYSTEM_ADDRESS, "editor", "key");
		user.SetMaxGMLevel(accountLevel);
		Character character(1, &user);
		Entity player(44, info);
		auto* const characterComponent = player.AddComponent<CharacterComponent>(-1, &character, UNASSIGNED_SYSTEM_ADDRESS);
		EXPECT_EQ(characterComponent->GetEditorLevel(), accountLevel == eGameMasterLevel::OPERATOR ? 9 : 0);
		EXPECT_EQ(characterComponent->GetEditorEnabled(), accountLevel == eGameMasterLevel::OPERATOR);
		// An update with the GM block: its flag, PvP, is a GM, the GM level, then the two editor fields
		characterComponent->SetGMLevel(eGameMasterLevel::CIVILIAN);
		RakNet::BitStream stream;
		characterComponent->Serialize(stream, false);
		bool written{}, pvp{}, isGm{}, editorEnabled{};
		uint8_t gmLevel{}, editorLevel{};
		EXPECT_TRUE(stream.Read(written) && stream.Read(pvp) && stream.Read(isGm) && stream.Read(gmLevel) && stream.Read(editorEnabled) && stream.Read(editorLevel));
		EXPECT_TRUE(written);
		EXPECT_EQ(gmLevel, 0);
		return std::pair<bool, uint8_t>{ editorEnabled, editorLevel };
	};
	EXPECT_EQ(gmBlock(eGameMasterLevel::OPERATOR), (std::pair<bool, uint8_t>{ true, 9 }));
	EXPECT_EQ(gmBlock(eGameMasterLevel::DEVELOPER), (std::pair<bool, uint8_t>{ false, 0 }));
	EXPECT_EQ(gmBlock(eGameMasterLevel::CIVILIAN), (std::pair<bool, uint8_t>{ false, 0 }));
}

// GetEditorLevel's answer: one bit, then the level as 32 bits
TEST_F(CharacterSaveFieldsTests, GetEditorLevelAnswerLayout) {
	GameMessages::GetEditorLevel answer;
	answer.editorEnabled = true;
	answer.editorLevel = 9;
	RakNet::BitStream stream;
	answer.Serialize(stream);
	EXPECT_EQ(stream.GetNumberOfBitsUsed(), 33u);
	GameMessages::GetEditorLevel read;
	ASSERT_TRUE(read.Deserialize(stream));
	EXPECT_TRUE(read.editorEnabled);
	EXPECT_EQ(read.editorLevel, 9);
	// A question with nothing in it reads too
	RakNet::BitStream empty;
	EXPECT_TRUE(GameMessages::GetEditorLevel().Deserialize(empty));
}
