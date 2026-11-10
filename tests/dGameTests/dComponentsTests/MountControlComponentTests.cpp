// The mount element of a character's save: LWOMountControlComponent::LoadFromSaveData (0x00bf99c0) reads <mnt a>, and
// every live save had the element, always as <mnt a="0"/> (450 looked at).
#include "GameDependencies.h"

#include "Entity.h"
#include "MountControlComponent.h"
#include "PossessorComponent.h"

#include "tinyxml2.h"

#include <gtest/gtest.h>
#include <string>

class MountControlComponentTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }

	static std::string Print(const tinyxml2::XMLDocument& doc) {
		tinyxml2::XMLPrinter printer(nullptr, true);
		doc.Print(&printer);
		return printer.CStr();
	}
};

// A save from before the element was written gains it, and nothing else changes
TEST_F(MountControlComponentTests, OldSaveGainsTheElement) {
	tinyxml2::XMLDocument doc;
	ASSERT_EQ(doc.Parse(R"(<obj v="1"><mf hc="1"/><char cc="5"/><dest hm="4"/></obj>)"), tinyxml2::XML_SUCCESS);
	Entity player(70, info);
	auto* const mountControl = player.AddComponent<MountControlComponent>(-1);
	mountControl->LoadFromSaveData(doc);
	EXPECT_EQ(mountControl->GetSavedMountItemID(), LWOOBJID_EMPTY);
	mountControl->WriteSaveData(doc);
	EXPECT_EQ(Print(doc), R"(<obj v="1"><mf hc="1"/><char cc="5"/><dest hm="4"/><mnt a="0"/></obj>)");
	// Saving again changes nothing
	mountControl->WriteSaveData(doc);
	EXPECT_EQ(Print(doc), R"(<obj v="1"><mf hc="1"/><char cc="5"/><dest hm="4"/><mnt a="0"/></obj>)");
}

// Live wrote 0 in every save, so 0 is written while a mount is ridden as well. A save that holds another value is
// read without failing and gets 0 at the next save.
TEST_F(MountControlComponentTests, ZeroIsWrittenAlsoWhileRiding) {
	tinyxml2::XMLDocument doc;
	ASSERT_EQ(doc.Parse(R"(<obj v="1"><mis/><mnt a="1152921510436607007"/><dest hm="4"/></obj>)"), tinyxml2::XML_SUCCESS);
	Entity player(71, info);
	auto* const possessor = player.AddComponent<PossessorComponent>(-1);
	auto* const mountControl = player.AddComponent<MountControlComponent>(-1);
	mountControl->LoadFromSaveData(doc);
	EXPECT_EQ(mountControl->GetSavedMountItemID(), 1152921510436607007LL);

	mountControl->WriteSaveData(doc);
	EXPECT_EQ(Print(doc), R"(<obj v="1"><mis/><mnt a="0"/><dest hm="4"/></obj>)");

	possessor->SetMountItemID(1152921510436607123LL);
	mountControl->WriteSaveData(doc);
	EXPECT_EQ(Print(doc), R"(<obj v="1"><mis/><mnt a="0"/><dest hm="4"/></obj>)");
}

// An a that is not a number is not an error: nothing is read and 0 is written
TEST_F(MountControlComponentTests, AnUnreadableValueDoesNotFail) {
	tinyxml2::XMLDocument doc;
	ASSERT_EQ(doc.Parse(R"(<obj v="1"><mnt a="x"/></obj>)"), tinyxml2::XML_SUCCESS);
	Entity player(72, info);
	auto* const mountControl = player.AddComponent<MountControlComponent>(-1);
	mountControl->LoadFromSaveData(doc);
	EXPECT_EQ(mountControl->GetSavedMountItemID(), LWOOBJID_EMPTY);
	mountControl->WriteSaveData(doc);
	EXPECT_EQ(Print(doc), R"(<obj v="1"><mnt a="0"/></obj>)");
}
