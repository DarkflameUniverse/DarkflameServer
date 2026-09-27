#include "PropertyMessages.h"
#include "GameDependencies.h"
#include "GameMessageTestUtils.h"
#include "Legacy/PropertyMessagesLegacy.h"

#include "Amf3.h"
#include "GeneralUtils.h"

#include <functional>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace GameMessageTestUtils;

namespace {
	const std::vector<int32_t> g_Ints = { 0, 1, -1, 1727, std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min() };
	// UTF-8 as the database holds it; the structs take UTF-16 converted the way the old code converted.
	const std::vector<std::string> g_Utf8 = { "", "Home", "Caf\xc3\xa9 \xe2\x98\x83 \xf0\x9f\x8f\xa0", std::string(300, 'x') };
	const std::vector<NiPoint3> g_Points = { NiPoint3Constant::ZERO, NiPoint3(1.5f, -2.0f, 1e6f) };
	const std::vector<NiQuaternion> g_Rotations = { QuatUtils::IDENTITY, NiQuaternion(0.5f, 0.5f, -0.5f, 0.5f) };

	PacketBytes Payload(const GameMessages::NetGameMsg& msg) {
		RakNet::BitStream bitStream;
		msg.Serialize(bitStream);
		return FromBitStream(bitStream);
	}

	// Serializes msg, then reads it with the legacy read sequence; both must consume exactly the same bits.
	template<typename Result>
	Result ReadWithLegacy(const GameMessages::NetGameMsg& msg, const std::function<Result(RakNet::BitStream&)>& read) {
		RakNet::BitStream wire;
		msg.Serialize(wire);
		RakNet::BitStream legacyStream(wire.GetData(), wire.GetNumberOfBytesUsed(), false);
		auto result = read(legacyStream);
		EXPECT_EQ(legacyStream.GetReadOffset(), wire.GetNumberOfBitsUsed());
		return result;
	}

	// The inputs PropertyManagementComponent::OnQueryPropertyData fills in.
	struct PropertyDataInput {
		uint32_t templateId{};
		uint32_t mapId{};
		uint32_t vendorMapId{};
		std::string spawnName;
		bool moderatorRequested{};
		uint32_t reputation{};
		uint32_t lastUpdatedTime{};
		LWOOBJID ownerId{};
		std::string ownerName;
		std::string name;
		std::string description;
		uint64_t claimed{};
		char privacy{};
		LWOCLONEID cloneId{};
		std::string rejectionReason;
		std::vector<NiPoint3> paths;
	};

	// Builds the struct exactly as PropertyManagementComponent::OnQueryPropertyData does.
	GameMessages::DownloadPropertyData Build(const PropertyDataInput& in, LWOOBJID author) {
		GameMessages::DownloadPropertyData message;
		message.target = author;
		message.propertyId = 0;
		message.templateId = static_cast<int32_t>(in.templateId);
		message.mapId = static_cast<uint16_t>(in.mapId);
		message.vendorMapId = static_cast<uint16_t>(in.vendorMapId);
		message.cloneId = in.cloneId;
		message.name = GeneralUtils::UTF8ToUTF16(in.name);
		message.description = GeneralUtils::UTF8ToUTF16(in.description);
		message.ownerName = GeneralUtils::UTF8ToUTF16(in.ownerName);
		message.ownerId = in.ownerId;
		message.propertyType = 0;
		message.zoneCode = 0;
		message.rent = 0;
		message.rentalPeriod = 1;
		message.expirationDate = in.lastUpdatedTime;
		message.rentAmount = 1;
		message.reputation = in.reputation;
		message.spawnName = GeneralUtils::ASCIIToUTF16(in.spawnName);
		message.rentDuration = 0;
		message.votes = 1;
		message.durationType = 1;
		message.renew = static_cast<uint8_t>(in.privacy);
		message.ownerAccountID = 0;
		if (in.rejectionReason != "") message.moderationStatus = GameMessages::DownloadPropertyData::REJECTION_STATUS_REJECTED;
		else if (in.moderatorRequested == true && in.rejectionReason == "") message.moderationStatus = GameMessages::DownloadPropertyData::REJECTION_STATUS_APPROVED;
		else message.moderationStatus = GameMessages::DownloadPropertyData::REJECTION_STATUS_PENDING;
		message.lastLogoutTime = 0;
		message.dayOfMonthPlaqueWasBought = 1;
		message.repAchievementReq = 1;
		message.zonePosition = { 548.0f, 406.0f, 178.0f };
		message.maxBuildHeight = 128.0f;
		message.rentalDate = in.claimed;
		message.accessType = static_cast<uint8_t>(in.privacy);
		message.pathPositions = in.paths;
		return message;
	}

	LegacyGameMessages::PropertyDataMessage BuildLegacy(const PropertyDataInput& in) {
		LegacyGameMessages::PropertyDataMessage message(in.mapId);
		message.TemplateID = in.templateId;
		message.ZoneId = static_cast<uint16_t>(in.mapId);
		message.VendorMapId = static_cast<uint16_t>(in.vendorMapId);
		message.SpawnName = in.spawnName;
		message.moderatorRequested = in.moderatorRequested;
		message.reputation = in.reputation;
		message.LastUpdatedTime = in.lastUpdatedTime;
		message.OwnerId = in.ownerId;
		message.OwnerName = in.ownerName;
		message.Name = in.name;
		message.Description = in.description;
		message.ClaimedTime = in.claimed;
		message.PrivacyOption = in.privacy;
		message.cloneId = in.cloneId;
		message.rejectionReason = in.rejectionReason;
		message.Paths = in.paths;
		return message;
	}

	std::vector<GameMessages::PropertySelectQuery::PropertyInfo> ToInfos(const std::vector<LegacyGameMessages::PropertySelectQueryProperty>& legacy) {
		std::vector<GameMessages::PropertySelectQuery::PropertyInfo> out;
		for (const auto& entry : legacy) {
			auto& info = out.emplace_back();
			info.cloneId = entry.CloneId;
			info.ownerName = GeneralUtils::UTF8ToUTF16(entry.OwnerName);
			info.name = GeneralUtils::UTF8ToUTF16(entry.Name);
			info.description = GeneralUtils::UTF8ToUTF16(entry.Description);
			info.reputation = entry.Reputation;
			info.isBff = entry.IsBestFriend;
			info.isFriend = entry.IsFriend;
			info.isModApproved = entry.IsModeratorApproved;
			info.isAlt = entry.IsAlt;
			info.isOwned = entry.IsOwned;
			info.accessType = entry.AccessType;
			info.dateLastPublished = entry.DateLastPublished;
			info.performanceCost = entry.PerformanceCost;
		}
		return out;
	}
}

class PropertyMessagesTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

TEST_F(PropertyMessagesTests, NoPayloadMessagesMatchLegacy) {
	for (const auto target : g_Targets) {
		GameMessages::OpenPropertyVendor vendor;
		vendor.target = target;
		ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendOpenPropertyVendor(target, a); }, vendor);
		EXPECT_EQ(Payload(vendor).bits, 0);

		GameMessages::PropertyEntranceBegin begin;
		begin.target = target;
		ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendPropertyEntranceBegin(target, a); }, begin);
		EXPECT_EQ(Payload(begin).bits, 0);
	}

	// The legacy SendOpenPropertyManagment took its target from PropertyManagementComponent::Instance(), which needs a
	// property world, so it is checked against hand written bytes: header, target, message id and nothing else.
	GameMessages::OpenPropertyManagement management;
	management.target = 0x0102030405060708LL;
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 08 07 06 05 04 03 02 01 5c 03"), StructPacket(management));
}

TEST_F(PropertyMessagesTests, DownloadPropertyDataMatchesLegacy) {
	std::vector<PropertyDataInput> inputs;
	for (const auto& text : g_Utf8) {
		PropertyDataInput in;
		in.templateId = 25166;
		in.mapId = 1150;
		in.vendorMapId = 1100;
		in.spawnName = "AGSmallProperty";
		in.name = text;
		in.description = text + "!";
		in.ownerName = text.substr(0, 10);
		inputs.push_back(in);
	}
	PropertyDataInput full;
	full.templateId = 0xFFFFFFFF;
	full.mapId = 0x12345;
	full.vendorMapId = 0xFFFF;
	full.spawnName = "NSSmallProperty";
	full.moderatorRequested = true;
	full.reputation = 0xFFFFFFFF;
	full.lastUpdatedTime = 1727000000;
	full.ownerId = 0x1000000000000456LL;
	full.ownerName = "Owner";
	full.name = "Name";
	full.description = "Desc";
	full.claimed = 0x0102030405060708ULL;
	full.privacy = 2;
	full.cloneId = 1234567;
	full.paths = { NiPoint3(1, 2, 3), NiPoint3(-4, 5.5f, 6), NiPoint3Constant::ZERO };
	inputs.push_back(full);
	full.rejectionReason = "no";
	inputs.push_back(full);
	full.rejectionReason = "";
	full.moderatorRequested = false;
	full.privacy = static_cast<char>(-1);
	inputs.push_back(full);

	for (const auto target : g_Targets) {
		for (const auto& in : inputs) {
			const auto msg = Build(in, target);
			const auto legacy = BuildLegacy(in);
			ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendDownloadPropertyData(target, legacy, a); }, msg);
			const auto copy = RoundTrip(msg);
			EXPECT_EQ(copy.name, msg.name);
			EXPECT_EQ(copy.moderationStatus, msg.moderationStatus);
			EXPECT_EQ(copy.pathPositions, msg.pathPositions);
			EXPECT_EQ(copy.rentalDate, msg.rentalDate);
			ExpectTruncatedFails(msg);
		}
	}
}

TEST_F(PropertyMessagesTests, PropertyRentalResponseMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const auto value : g_Ints) {
			GameMessages::PropertyRentalResponse msg;
			msg.target = target;
			msg.cloneid = static_cast<LWOCLONEID>(value);
			msg.code = static_cast<uint32_t>(value / 3);
			msg.propertyID = target;
			msg.rentdue = static_cast<int64_t>(value) * 1000;
			ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendPropertyRentalResponse(target, msg.cloneid, msg.code, msg.propertyID, msg.rentdue, a); }, msg);
			const auto copy = RoundTrip(msg);
			EXPECT_EQ(copy.cloneid, msg.cloneid);
			EXPECT_EQ(copy.rentdue, msg.rentdue);
			ExpectTruncatedFails(msg);
		}
	}
}

TEST_F(PropertyMessagesTests, PropertySelectQueryMatchesLegacy) {
	std::vector<std::vector<LegacyGameMessages::PropertySelectQueryProperty>> pages = { {} };
	std::vector<LegacyGameMessages::PropertySelectQueryProperty> page;
	for (size_t i = 0; i < g_Utf8.size(); i++) {
		LegacyGameMessages::PropertySelectQueryProperty entry;
		entry.CloneId = static_cast<LWOCLONEID>(i * 1000);
		entry.OwnerName = g_Utf8[i];
		entry.Name = g_Utf8[(i + 1) % g_Utf8.size()];
		entry.Description = g_Utf8[(i + 2) % g_Utf8.size()];
		entry.Reputation = static_cast<float>(i) * 12.5f;
		entry.IsBestFriend = i & 1;
		entry.IsFriend = i & 2;
		entry.IsModeratorApproved = i & 1;
		entry.IsAlt = i & 2;
		entry.IsOwned = i == 0;
		entry.AccessType = static_cast<uint32_t>(i);
		entry.DateLastPublished = 1727000000ULL + i;
		entry.PerformanceCost = 0.25f * static_cast<float>(i);
		page.push_back(entry);
	}
	page.emplace_back(); // defaults (cloneId LWOCLONEID_INVALID)
	pages.push_back(page);

	for (const auto target : g_Targets) {
		for (const auto& entries : pages) {
			for (const auto navOffset : { 0, 7, -1 }) {
				for (const bool flag : { false, true }) {
					GameMessages::PropertySelectQuery msg;
					msg.target = target;
					msg.navOffset = navOffset;
					msg.thereAreMore = flag;
					msg.cloneId = navOffset * 11;
					msg.hasFeaturedProperty = !flag;
					msg.wasFriends = flag;
					msg.properties = ToInfos(entries);
					ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendPropertySelectQuery(target, navOffset, flag, msg.cloneId, !flag, flag, entries, a); }, msg);
					const auto copy = RoundTrip(msg);
					ASSERT_EQ(copy.properties.size(), msg.properties.size());
					for (size_t i = 0; i < copy.properties.size(); i++) {
						EXPECT_EQ(copy.properties[i].name, msg.properties[i].name);
						EXPECT_EQ(copy.properties[i].cloneId, msg.properties[i].cloneId);
						EXPECT_EQ(copy.properties[i].isAlt, msg.properties[i].isAlt);
					}
					ExpectTruncatedFails(msg);
				}
			}
		}
	}
}

TEST_F(PropertyMessagesTests, GetModelsOnPropertyMatchesLegacy) {
	const std::vector<std::map<LWOOBJID, LWOOBJID>> modelSets = { {}, { { 5, 6 } }, { { 0x1000000000000001LL, 1 }, { 2, 0x0102030405060708LL }, { -1, 0 } } };
	for (const auto target : g_Targets) {
		for (const auto& models : modelSets) {
			GameMessages::GetModelsOnProperty msg;
			msg.target = target;
			msg.models = { models.begin(), models.end() };
			ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendGetModelsOnProperty(target, models, a); }, msg);
			EXPECT_EQ(RoundTrip(msg).models, msg.models);
			ExpectTruncatedFails(msg);
		}
	}
}

TEST_F(PropertyMessagesTests, PlaceModelResponseMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const auto& position : g_Points) {
			for (const auto plaque : g_Targets) {
				for (const auto response : { 0, 14, 16, -1 }) {
					for (const auto& rotation : g_Rotations) {
						GameMessages::PlaceModelResponse msg;
						msg.target = target;
						msg.position = position;
						msg.propertyPlaqueID = plaque;
						msg.response = response;
						msg.rotation = rotation;
						ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendPlaceModelResponse(target, a, position, plaque, response, rotation); }, msg);
						// The kept wire bug (response written where rotation belongs) only round trips with the default rotation.
						if (rotation == QuatUtils::IDENTITY) {
							const auto copy = RoundTrip(msg);
							EXPECT_EQ(copy.position, position);
							EXPECT_EQ(copy.propertyPlaqueID, plaque);
							EXPECT_EQ(copy.response, response);
							ExpectTruncatedFails(msg);
						}
					}
				}
			}
		}
	}
}

TEST_F(PropertyMessagesTests, UgcEquipMessagesMatchLegacy) {
	for (const auto target : g_Targets) {
		for (const auto id : g_Targets) {
			for (const auto value : g_Ints) {
				GameMessages::HandleUGCEquipPreCreateBasedOnEditMode pre;
				pre.target = target;
				pre.modelCount = value;
				pre.modelID = id;
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendUGCEquipPreCreateBasedOnEditMode(target, a, value, id); }, pre);
				EXPECT_EQ(RoundTrip(pre).modelCount, value);

				GameMessages::HandleUGCEquipPostDeleteBasedOnEditMode post;
				post.target = target;
				post.invItem = id;
				post.itemsTotal = value;
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendUGCEquipPostDeleteBasedOnEditMode(target, a, id, value); }, post);
				const auto copy = RoundTrip(post);
				EXPECT_EQ(copy.invItem, id);
				EXPECT_EQ(copy.itemsTotal, value);
				ExpectTruncatedFails(post);
			}
		}
	}
}

TEST_F(PropertyMessagesTests, HotPropertiesMatchLegacy) {
	GameMessages::NewsSendHotPropertiesInfoToClient msg;
	for (int i = 0; i < 3; i++) {
		RakNet::BitStream legacy;
		LegacyGameMessages::SerializeHotProperties(legacy, msg.properties);
		EXPECT_PACKET_EQ(FromBitStream(legacy), Payload(msg));
		const auto copy = RoundTrip(msg);
		EXPECT_EQ(copy.properties.size(), msg.properties.size());

		auto& info = msg.properties.emplace_back();
		info.propertyId = 0x1000000000000123LL + i;
		info.ownerId = 0x1000000000000456LL;
		info.ownerName = GeneralUtils::UTF8ToUTF16(g_Utf8[i + 1]);
		info.reputation = 12345ULL << (i * 10);
		info.templateId = 25166 + i;
		info.name = GeneralUtils::UTF8ToUTF16(g_Utf8[i]);
		info.description = u"Desc";
		info.performanceCost = 0.5f * static_cast<float>(i);
		info.lastPublished = 1727000000ULL;
		info.cloneId = static_cast<uint32_t>(i);
	}
	ExpectTruncatedFails(msg);
}

TEST_F(PropertyMessagesTests, InboundReadsLikeLegacy) {
	for (const uint8_t access : { 0, 1, 2, 255 }) {
		for (const auto renew : g_Ints) {
			GameMessages::SetPropertyAccess msg;
			msg.accessType = access;
			msg.renew = renew;
			const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacySetPropertyAccess>(msg, LegacyGameMessages::ReadSetPropertyAccess);
			const auto copy = RoundTrip(msg);
			EXPECT_EQ(copy.accessType, legacy.accessType);
			EXPECT_EQ(copy.renew, legacy.renew);
			ExpectTruncatedFails(msg);
		}
	}

	for (const auto id : g_Targets) {
		for (const auto& text : g_Utf8) {
			for (const bool isProperty : { false, true }) {
				GameMessages::UpdatePropertyOrModelForFilterCheck msg;
				msg.isProperty = isProperty;
				msg.ugcId = id;
				msg.playerId = ~id;
				msg.worldId = id / 3;
				msg.newDescription = GeneralUtils::UTF8ToUTF16(text);
				msg.newName = GeneralUtils::UTF8ToUTF16(text.substr(0, 5));
				const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyFilterCheck>(msg, LegacyGameMessages::ReadUpdatePropertyOrModelForFilterCheck);
				const auto copy = RoundTrip(msg);
				EXPECT_FALSE(legacy.rejected);
				EXPECT_EQ(copy.isProperty, legacy.isProperty);
				EXPECT_EQ(copy.ugcId, legacy.objectId);
				EXPECT_EQ(copy.playerId, legacy.playerId);
				EXPECT_EQ(copy.worldId, legacy.worldId);
				EXPECT_EQ(copy.newName, legacy.name);
				EXPECT_EQ(copy.newDescription, legacy.description);
				ExpectTruncatedFails(msg);
			}

			for (const auto reason : { 0, 1, 2, -1 }) {
				GameMessages::DeleteModelFromClient del;
				del.modelID = id;
				del.reason = reason;
				const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyDeletePropertyModel>(del, LegacyGameMessages::ReadDeletePropertyModel);
				const auto copy = RoundTrip(del);
				EXPECT_EQ(copy.modelID, legacy.model);
				EXPECT_EQ(copy.reason, legacy.deleteReason);
				ExpectTruncatedFails(del);
			}

			GameMessages::ReportOffensiveModel model;
			model.description = GeneralUtils::UTF8ToUTF16(text);
			model.offensiveObjectID = id;
			GameMessages::ReportOffensiveProperty property;
			property.description = model.description;
			property.propertyPlaqueObjectID = id;
			for (const GameMessages::NetGameMsg* report : std::initializer_list<const GameMessages::NetGameMsg*>{ &model, &property }) {
				std::string description;
				LWOOBJID objectId{};
				const bool read = ReadWithLegacy<bool>(*report, [&](RakNet::BitStream& bs) { return LegacyGameMessages::ReadDescriptionAndObject(bs, description, objectId); });
				EXPECT_TRUE(read);
				EXPECT_EQ(description, GeneralUtils::UTF16ToWTF8(model.description));
				EXPECT_EQ(objectId, id);
			}
			EXPECT_EQ(RoundTrip(model).description, model.description);
			EXPECT_EQ(RoundTrip(property).propertyPlaqueObjectID, id);
			ExpectTruncatedFails(model);
			ExpectTruncatedFails(property);
		}

		GameMessages::PlacePropertyModel place;
		place.modelID = id;
		EXPECT_EQ(ReadWithLegacy<LWOOBJID>(place, LegacyGameMessages::ReadPlacePropertyModel), id);
		EXPECT_EQ(RoundTrip(place).modelID, id);
		ExpectTruncatedFails(place);

		for (const auto& position : g_Points) {
			for (const auto& rotation : g_Rotations) {
				GameMessages::UpdateModelFromClient update;
				update.modelID = id;
				update.position = position;
				update.rotation = rotation;
				const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyUpdatePropertyModel>(update, LegacyGameMessages::ReadUpdatePropertyModel);
				const auto copy = RoundTrip(update);
				EXPECT_EQ(copy.modelID, legacy.model);
				EXPECT_EQ(copy.position, legacy.position);
				EXPECT_EQ(copy.rotation, legacy.rotation);
				ExpectTruncatedFails(update);
			}
		}
	}

	for (const auto value : g_Ints) {
		for (const bool flag : { false, true }) {
			GameMessages::PropertyEntranceSync sync;
			sync.includeNullAddress = flag;
			sync.includeNullDescription = !flag;
			sync.playersOwn = flag;
			sync.updateUI = !flag;
			sync.numResults = value;
			sync.reputationTime = -value;
			sync.sortMethod = value / 2;
			sync.startIndex = value / 3;
			sync.filterText = flag ? "search" : "";
			const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyPropertyEntranceSync>(sync, LegacyGameMessages::ReadPropertyEntranceSync);
			const auto copy = RoundTrip(sync);
			EXPECT_FALSE(legacy.rejected);
			EXPECT_EQ(copy.includeNullAddress, legacy.includeNullAddress);
			EXPECT_EQ(copy.includeNullDescription, legacy.includeNullDescription);
			EXPECT_EQ(copy.playersOwn, legacy.playerOwn);
			EXPECT_EQ(copy.updateUI, legacy.updateUi);
			EXPECT_EQ(copy.numResults, legacy.numResults);
			EXPECT_EQ(copy.reputationTime, legacy.reputation);
			EXPECT_EQ(copy.sortMethod, legacy.sortMethod);
			EXPECT_EQ(copy.startIndex, legacy.startIndex);
			EXPECT_EQ(copy.filterText, legacy.filterText);
			ExpectTruncatedFails(sync);

			GameMessages::EnterProperty1 enter;
			enter.index = value;
			enter.returnToZone = flag;
			const auto legacyEnter = ReadWithLegacy<LegacyGameMessages::LegacyEnterProperty>(enter, LegacyGameMessages::ReadEnterProperty);
			const auto enterCopy = RoundTrip(enter);
			EXPECT_EQ(static_cast<uint32_t>(enterCopy.index), legacyEnter.index);
			EXPECT_EQ(enterCopy.returnToZone, legacyEnter.returnToZone);
			ExpectTruncatedFails(enter);
		}
	}

	for (const float cost : { 0.0f, 1.5f, -2.25f, 1e9f }) {
		GameMessages::UpdatePropertyPerformanceCost msg;
		msg.performanceCost = cost;
		EXPECT_EQ(ReadWithLegacy<float>(msg, LegacyGameMessages::ReadUpdatePropertyPerformanceCost), cost);
		EXPECT_EQ(RoundTrip(msg).performanceCost, cost);
		ExpectTruncatedFails(msg);
	}
}

// Messages whose payload DLU used to ignore: layouts from the client's Deserialize functions.
TEST_F(PropertyMessagesTests, ClientLayoutsRoundTrip) {
	GameMessages::PropertyEditorBegin begin;
	EXPECT_PACKET_EQ(FromHex("00", 4), Payload(begin));
	begin.distanceType = 2;
	begin.propertyObjectID = 0x11;
	begin.startMode = 3;
	begin.startPaused = true;
	const auto beginCopy = RoundTrip(begin);
	EXPECT_EQ(beginCopy.distanceType, 2);
	EXPECT_EQ(beginCopy.propertyObjectID, 0x11);
	EXPECT_EQ(beginCopy.startMode, 3);
	EXPECT_TRUE(beginCopy.startPaused);
	ExpectTruncatedFails(begin);

	for (const bool queryDB : { false, true }) {
		GameMessages::PropertyContentsFromClient contents;
		contents.queryDB = queryDB;
		EXPECT_EQ(RoundTrip(contents).queryDB, queryDB);
		ExpectTruncatedFails(contents);
	}

	for (const auto id : g_Targets) {
		GameMessages::ZonePropertyModelEquipped equipped;
		equipped.playerID = id;
		equipped.propertyID = ~id;
		const auto equippedCopy = RoundTrip(equipped);
		EXPECT_EQ(equippedCopy.playerID, id);
		EXPECT_EQ(equippedCopy.propertyID, ~id);
		ExpectTruncatedFails(equipped);

		GameMessages::ZonePropertyModelRotated rotated;
		rotated.playerID = ~id;
		rotated.propertyID = id;
		const auto rotatedCopy = RoundTrip(rotated);
		EXPECT_EQ(rotatedCopy.playerID, ~id);
		EXPECT_EQ(rotatedCopy.propertyID, id);
		ExpectTruncatedFails(rotated);
	}
}

TEST_F(PropertyMessagesTests, ControlBehaviorsRoundTrips) {
	GameMessages::ControlBehaviors msg;
	msg.args = std::make_unique<AMFArrayValue>();
	// One key: the associative part is unordered, so more keys may come back in another order.
	msg.args->Insert("BehaviorID", "10446");
	msg.command = "rename";
	RakNet::BitStream bitStream;
	msg.Serialize(bitStream);
	GameMessages::ControlBehaviors copy;
	ASSERT_TRUE(copy.Deserialize(bitStream));
	EXPECT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
	EXPECT_EQ(copy.command, "rename");
	ASSERT_NE(copy.args, nullptr);
	ASSERT_NE(copy.args->Get<std::string>("BehaviorID"), nullptr);
	EXPECT_EQ(copy.args->Get<std::string>("BehaviorID")->GetValue(), "10446");
	RakNet::BitStream again;
	copy.Serialize(again);
	EXPECT_PACKET_EQ(FromBitStream(bitStream), FromBitStream(again));

	// Not an AMF array: dropped.
	RakNet::BitStream notArray;
	notArray.Write<uint8_t>(0x04); // integer marker
	notArray.Write<uint8_t>(0x01);
	GameMessages::ControlBehaviors bad;
	EXPECT_FALSE(bad.Deserialize(notArray));
}

// Independent of the legacy code: hand computed payloads (RakNet writes MSB first).
TEST_F(PropertyMessagesTests, GoldenBytes) {
	GameMessages::HandleUGCEquipPostDeleteBasedOnEditMode post;
	post.invItem = 0x22;
	EXPECT_PACKET_EQ(FromHex("22 00 00 00 00 00 00 00 00", 65), Payload(post));

	GameMessages::PlaceModelResponse response;
	response.response = 16;
	// three flags 0, 0, 1, then 10 00 00 00, then the rotation flag 0
	EXPECT_PACKET_EQ(FromHex("22 00 00 00 00", 36), Payload(response));

	GameMessages::SetPropertyAccess access;
	access.accessType = 2;
	// 1, 02, 0
	EXPECT_PACKET_EQ(FromHex("81 00", 10), Payload(access));

	GameMessages::GetModelsOnProperty models;
	models.models = { { 1, 2 } };
	EXPECT_PACKET_EQ(FromHex("01 00 00 00 01 00 00 00 00 00 00 00 02 00 00 00 00 00 00 00"), Payload(models));

	GameMessages::EnterProperty1 enter;
	enter.index = -1;
	EXPECT_PACKET_EQ(FromHex("ff ff ff ff 80", 33), Payload(enter));
}
