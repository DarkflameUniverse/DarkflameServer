#ifndef PROPERTYMESSAGESLEGACY_H
#define PROPERTYMESSAGESLEGACY_H

// FROZEN ORACLE - DO NOT EDIT.
// Verbatim copies of the hand written code that PropertyMessages.h replaced (dGame/dGameMessages/GameMessages.cpp,
// PropertyDataMessage.{h,cpp} and PropertySelectQueryProperty.{h,cpp}, branched from origin/main 129199e4 plus the
// news screen's hot properties). Only the namespace changed. The Read* functions are the read sequences of the
// replaced GameMessages::Handle* functions (and PlayerReports' report readers).

#include "LegacyPacketMacros.h"
#include "BitStreamUtils.h"
#include "CDClientManager.h"
#include "CDPropertyTemplateTable.h"
#include "dCommonVars.h"
#include "dServer.h"
#include "Entity.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "MessageType/Client.h"
#include "MessageType/Game.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "PropertyManagementComponent.h"
#include "PropertyMessages.h"
#include "ServiceType.h"

#include <map>
#include <string>
#include <vector>

namespace LegacyGameMessages {
	class PropertyDataMessage final
	{
	public:
		explicit PropertyDataMessage(uint32_t mapID);

		void Serialize(RakNet::BitStream& stream) const;

		std::string OwnerName = "";
		LWOOBJID OwnerId = LWOOBJID_EMPTY;

		// Temporary values
		uint32_t TemplateID = 25166;
		uint16_t ZoneId = 1150;
		uint16_t VendorMapId = 1100;
		std::string SpawnName = "AGSmallProperty";

		std::string Name = "";
		std::string Description = "";
		std::string rejectionReason = "";

		bool moderatorRequested = 0;
		LWOCLONEID cloneId = 0;
		uint32_t reputation = 0;
		uint64_t ClaimedTime = 0;
		uint64_t LastUpdatedTime = 0;

		NiPoint3 ZonePosition = { 548.0f, 406.0f, 178.0f };
		char PrivacyOption = 0;
		float MaxBuildHeight = 128.0f;
		std::vector<NiPoint3> Paths = {};
	private:
		enum RejectionStatus : uint32_t {
			REJECTION_STATUS_APPROVED = 0,
			REJECTION_STATUS_PENDING = 1,
			REJECTION_STATUS_REJECTED = 2
		};
	};

	inline void PropertyDataMessage::Serialize(RakNet::BitStream& stream) const {
		stream.Write<int64_t>(0); // - property id

		stream.Write<int32_t>(TemplateID); // - template id
		stream.Write<uint16_t>(ZoneId); // - map id
		stream.Write<uint16_t>(VendorMapId); // - vendor map id
		stream.Write<uint32_t>(cloneId); // clone id

		const auto& name = GeneralUtils::UTF8ToUTF16(Name);
		stream.Write<uint32_t>(name.size());
		for (uint32_t i = 0; i < name.size(); ++i) {
			stream.Write<uint16_t>(name[i]);
		}

		const auto& description = GeneralUtils::UTF8ToUTF16(Description);
		stream.Write<uint32_t>(description.size());
		for (uint32_t i = 0; i < description.size(); ++i) {
			stream.Write<uint16_t>(description[i]);
		}

		const auto& owner = GeneralUtils::UTF8ToUTF16(OwnerName);
		stream.Write<uint32_t>(owner.size());
		for (uint32_t i = 0; i < owner.size(); ++i) {
			stream.Write<uint16_t>(owner[i]);
		}

		stream.Write<LWOOBJID>(OwnerId);

		stream.Write<uint32_t>(0); // - type
		stream.Write<uint32_t>(0); // - zone code
		stream.Write<uint32_t>(0); // - minimum price
		stream.Write<uint32_t>(1); // - rent duration

		stream.Write<uint64_t>(LastUpdatedTime); // - timestamp

		stream.Write<uint32_t>(1);

		stream.Write<uint32_t>(reputation); // Reputation
		stream.Write<uint32_t>(0);

		const auto& spawn = GeneralUtils::ASCIIToUTF16(SpawnName);
		stream.Write<uint32_t>(spawn.size());
		for (uint32_t i = 0; i < spawn.size(); ++i) {
			stream.Write<uint16_t>(spawn[i]);
		}

		stream.Write<uint32_t>(0); // String length
		stream.Write<uint32_t>(0); // String length

		stream.Write<uint32_t>(0); // - duration type
		stream.Write<uint32_t>(1);
		stream.Write<uint32_t>(1);

		stream.Write<char>(PrivacyOption);

		stream.Write<uint64_t>(0);

		if (rejectionReason != "") stream.Write<uint32_t>(REJECTION_STATUS_REJECTED);
		else if (moderatorRequested == true && rejectionReason == "") stream.Write<uint32_t>(REJECTION_STATUS_APPROVED);
		else stream.Write<uint32_t>(REJECTION_STATUS_PENDING);

		// Does this go here???
		// const auto& rejectionReasonConverted = GeneralUtils::UTF8ToUTF16(rejectionReason);
		// stream.Write<uint32_t>(rejectionReasonConverted.size());
		// for (uint32_t i = 0; i < rejectionReasonConverted.size(); ++i) {
		// 	stream.Write<uint16_t>(rejectionReasonConverted[i]);
		// }

		stream.Write<uint32_t>(0);

		stream.Write<uint64_t>(0);

		stream.Write<uint32_t>(1);
		stream.Write<uint32_t>(1);

		stream.Write<float>(ZonePosition.x);
		stream.Write<float>(ZonePosition.y);
		stream.Write<float>(ZonePosition.z);

		stream.Write<float>(MaxBuildHeight);

		stream.Write(ClaimedTime); // - timestamp

		stream.Write<char>(PrivacyOption);

		stream.Write<uint32_t>(Paths.size());

		for (const auto& path : Paths) {
			stream.Write(path.x);
			stream.Write(path.y);
			stream.Write(path.z);
		}
	}

	inline PropertyDataMessage::PropertyDataMessage(uint32_t mapID) {
		const auto propertyTemplate = CDClientManager::GetTable<CDPropertyTemplateTable>()->GetByMapID(mapID);

		TemplateID = propertyTemplate.id;
		ZoneId = propertyTemplate.mapID;
		VendorMapId = propertyTemplate.vendorMapID;
		SpawnName = propertyTemplate.spawnName;
	}

	class PropertySelectQueryProperty final {
	public:
		void Serialize(RakNet::BitStream& stream) const;

		void Deserialize(RakNet::BitStream& stream) const;

		LWOCLONEID CloneId = LWOCLONEID_INVALID;        // The cloneID of the property
		std::string OwnerName = "";                     // The property owners name
		std::string Name = "";                          // The property name
		std::string Description = "";                   // The property description
		float Reputation = 0;                           // The reputation of the property
		bool IsBestFriend = false;                      // Whether or not the property belongs to a best friend
		bool IsFriend = false;                          // Whether or not the property belongs to a friend
		bool IsModeratorApproved = false;               // Whether or not a moderator has approved this property
		bool IsAlt = false;                             // Whether or not the property is owned by an alt of the account owner
		bool IsOwned = false;                           // Whether or not the property is owned
		uint32_t AccessType = 0;                        // The privacy option of the property
		uint64_t DateLastPublished = 0;                 // The last day the property was published
		float PerformanceCost = 0;                      // The performance cost of the property
	};

	inline void PropertySelectQueryProperty::Serialize(RakNet::BitStream& stream) const {
		stream.Write(CloneId);

		const auto& owner = GeneralUtils::UTF8ToUTF16(OwnerName);
		stream.Write<uint32_t>(owner.size());
		for (uint32_t i = 0; i < owner.size(); ++i) {
			stream.Write<uint16_t>(owner[i]);
		}

		const auto& name = GeneralUtils::UTF8ToUTF16(Name);
		stream.Write<uint32_t>(name.size());
		for (uint32_t i = 0; i < name.size(); ++i) {
			stream.Write<uint16_t>(name[i]);
		}

		const auto& description = GeneralUtils::UTF8ToUTF16(Description);
		stream.Write<uint32_t>(description.size());
		for (uint32_t i = 0; i < description.size(); ++i) {
			stream.Write<uint16_t>(description[i]);
		}

		stream.Write(Reputation);
		stream.Write(IsBestFriend);
		stream.Write(IsFriend);
		stream.Write(IsModeratorApproved);
		stream.Write(IsAlt);
		stream.Write(IsOwned);
		stream.Write(AccessType);
		stream.Write(DateLastPublished);
		stream.Write(PerformanceCost);
	}

	inline void PropertySelectQueryProperty::Deserialize(RakNet::BitStream& stream) const {
		// Do we need this?
		// no
	}

	// GameMessages::NewsSendHotPropertiesInfoToClient::Serialize (was a member function; now takes the entries)
	inline void SerializeHotProperties(RakNet::BitStream& stream, const std::vector<GameMessages::NewsSendHotPropertiesInfoToClient::HotPropertyInfo>& properties) {
		const auto writeWString = [&stream](const std::u16string& text) {
			stream.Write<uint32_t>(text.size());
			for (const auto character : text) stream.Write<uint16_t>(character);
		};
		stream.Write<uint32_t>(properties.size());
		for (const auto& info : properties) {
			stream.Write(info.propertyId);
			stream.Write(info.ownerId);
			writeWString(info.ownerName);
			stream.Write(info.reputation);
			stream.Write(info.templateId);
			writeWString(info.name);
			writeWString(info.description);
			stream.Write(info.performanceCost);
			stream.Write(info.lastPublished);
			stream.Write(info.cloneId);
		}
	}

	inline void SendOpenPropertyVendor(const LWOOBJID objectId, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::OPEN_PROPERTY_VENDOR);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendOpenPropertyManagment(const LWOOBJID objectId, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(PropertyManagementComponent::Instance()->GetParent()->GetObjectID());
		bitStream.Write(MessageType::Game::OPEN_PROPERTY_MANAGEMENT);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendDownloadPropertyData(const LWOOBJID objectId, const PropertyDataMessage& data, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::DOWNLOAD_PROPERTY_DATA);

		data.Serialize(bitStream);

		LOG("(%llu) sending property data (%d)", objectId, sysAddr == UNASSIGNED_SYSTEM_ADDRESS);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendPropertyRentalResponse(const LWOOBJID objectId, const LWOCLONEID cloneId, const uint32_t code, const LWOOBJID propertyId, const int64_t rentDue, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::PROPERTY_RENTAL_RESPONSE);

		bitStream.Write(cloneId);
		bitStream.Write(code);
		bitStream.Write(propertyId);
		bitStream.Write(rentDue);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendGetModelsOnProperty(LWOOBJID objectId, std::map<LWOOBJID, LWOOBJID> models, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::GET_MODELS_ON_PROPERTY);

		bitStream.Write<uint32_t>(models.size());

		for (const auto& pair : models) {
			bitStream.Write(pair.first);
			bitStream.Write(pair.second);
		}

		LOG("Sending property models to (%llu) (%d)", objectId, sysAddr == UNASSIGNED_SYSTEM_ADDRESS);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendPlaceModelResponse(LWOOBJID objectId, const SystemAddress& sysAddr, NiPoint3 position,
		LWOOBJID plaque, int32_t response, NiQuaternion rotation) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::PLACE_MODEL_RESPONSE);

		bitStream.Write(position != NiPoint3Constant::ZERO);
		if (position != NiPoint3Constant::ZERO) {
			bitStream.Write(position);
		}

		bitStream.Write(plaque != LWOOBJID_EMPTY);
		if (plaque != LWOOBJID_EMPTY) {
			bitStream.Write(plaque);
		}

		bitStream.Write(response != 0);
		if (response != 0) {
			bitStream.Write(response);
		}

		bitStream.Write(rotation != QuatUtils::IDENTITY);
		if (rotation != QuatUtils::IDENTITY) {
			bitStream.Write(response);
		}

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;

	}

	inline void SendUGCEquipPreCreateBasedOnEditMode(LWOOBJID objectId, const SystemAddress& sysAddr, int modelCount, LWOOBJID model) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::HANDLE_UGC_POST_CREATE_BASED_ON_EDIT_MODE);

		bitStream.Write(modelCount);
		bitStream.Write(model);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendUGCEquipPostDeleteBasedOnEditMode(LWOOBJID objectId, const SystemAddress& sysAddr, LWOOBJID inventoryItem, int itemTotal) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::HANDLE_UGC_POST_DELETE_BASED_ON_EDIT_MODE);

		bitStream.Write(inventoryItem);

		bitStream.Write(itemTotal != 0);
		if (itemTotal != 0) {
			bitStream.Write(itemTotal);
		}

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendPropertyEntranceBegin(LWOOBJID objectId, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::PROPERTY_ENTRANCE_BEGIN);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendPropertySelectQuery(LWOOBJID objectId, int32_t navOffset, bool thereAreMore, int32_t cloneId, bool hasFeaturedProperty, bool wasFriends, const std::vector<PropertySelectQueryProperty>& entries, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::PROPERTY_SELECT_QUERY);

		bitStream.Write(navOffset);
		bitStream.Write(thereAreMore);
		bitStream.Write(cloneId);
		bitStream.Write(hasFeaturedProperty);
		bitStream.Write(wasFriends);

		bitStream.Write<uint32_t>(entries.size());

		for (auto& entry : entries) {
			entry.Serialize(bitStream);
		}

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	// GameMessages::HandleSetPropertyAccess
	struct LegacySetPropertyAccess { uint8_t accessType{}; int32_t renew{}; };
	inline LegacySetPropertyAccess ReadSetPropertyAccess(RakNet::BitStream& inStream) {
		uint8_t accessType{};
		int32_t renew{};

		bool accessTypeIsDefault{};
		inStream.Read(accessTypeIsDefault);
		if (accessTypeIsDefault != 0) inStream.Read(accessType);

		bool renewIsDefault{};
		inStream.Read(renewIsDefault);
		if (renewIsDefault != 0) inStream.Read(renew);
		return { accessType, renew };
	}

	// GameMessages::HandleUpdatePropertyOrModelForFilterCheck
	struct LegacyFilterCheck { bool isProperty{}; LWOOBJID objectId{}; LWOOBJID playerId{}; LWOOBJID worldId{}; std::u16string name; std::u16string description; bool rejected{}; };
	inline LegacyFilterCheck ReadUpdatePropertyOrModelForFilterCheck(RakNet::BitStream& inStream) {
		bool isProperty{};
		LWOOBJID objectId{};
		LWOOBJID playerId{};
		LWOOBJID worldId{};
		uint32_t nameLength{};
		std::u16string name{};
		uint32_t descriptionLength{};
		std::u16string description{};

		inStream.Read(isProperty);
		inStream.Read(objectId);
		inStream.Read(playerId);
		inStream.Read(worldId);

		inStream.Read(descriptionLength);
		if (descriptionLength > MAX_MESSAGE_LENGTH) return { .rejected = true };
		for (uint32_t i = 0; i < descriptionLength; ++i) {
			uint16_t character;
			inStream.Read(character);
			description.push_back(character);
		}

		inStream.Read(nameLength);
		if (nameLength > MAX_MESSAGE_LENGTH) return { .rejected = true };
		for (uint32_t i = 0; i < nameLength; ++i) {
			uint16_t character;
			inStream.Read(character);
			name.push_back(character);
		}
		return { isProperty, objectId, playerId, worldId, name, description, false };
	}

	// GameMessages::HandlePlacePropertyModel
	inline LWOOBJID ReadPlacePropertyModel(RakNet::BitStream& inStream) {
		LWOOBJID model;

		inStream.Read(model);
		return model;
	}

	// GameMessages::HandleUpdatePropertyModel
	struct LegacyUpdatePropertyModel { LWOOBJID model{}; NiPoint3 position; NiQuaternion rotation; };
	inline LegacyUpdatePropertyModel ReadUpdatePropertyModel(RakNet::BitStream& inStream) {
		LWOOBJID model;
		NiPoint3 position;
		NiQuaternion rotation = QuatUtils::IDENTITY;

		inStream.Read(model);
		inStream.Read(position);

		if (inStream.ReadBit()) {
			inStream.Read(rotation);
		}
		return { model, position, rotation };
	}

	// GameMessages::HandleDeletePropertyModel
	struct LegacyDeletePropertyModel { LWOOBJID model{}; int deleteReason{}; };
	inline LegacyDeletePropertyModel ReadDeletePropertyModel(RakNet::BitStream& inStream) {
		LWOOBJID model = LWOOBJID_EMPTY;
		int deleteReason = 0;

		if (inStream.ReadBit()) {
			inStream.Read(model);
		}

		if (inStream.ReadBit()) {
			inStream.Read(deleteReason);

		}
		return { model, deleteReason };
	}

	// GameMessages::HandlePropertyEntranceSync
	struct LegacyPropertyEntranceSync {
		bool includeNullAddress{}; bool includeNullDescription{}; bool playerOwn{}; bool updateUi{};
		int32_t numResults{}; int32_t reputation{}; int32_t sortMethod{}; int32_t startIndex{}; std::string filterText{}; bool rejected{};
	};
	inline LegacyPropertyEntranceSync ReadPropertyEntranceSync(RakNet::BitStream& inStream) {
		bool includeNullAddress{};
		bool includeNullDescription{};
		bool playerOwn{};
		bool updateUi{};
		int32_t numResults{};
		int32_t reputation{};
		int32_t sortMethod{};
		int32_t startIndex{};
		uint32_t filterTextLength{};
		std::string filterText{};

		inStream.Read(includeNullAddress);
		inStream.Read(includeNullDescription);
		inStream.Read(playerOwn);
		inStream.Read(updateUi);
		inStream.Read(numResults);
		inStream.Read(reputation);
		inStream.Read(sortMethod);
		inStream.Read(startIndex);
		inStream.Read(filterTextLength);

		if (filterTextLength > MAX_MESSAGE_LENGTH) return { .rejected = true };
		for (auto i = 0u; i < filterTextLength; i++) {
			char c;
			inStream.Read(c);
			filterText.push_back(c);
		}
		return { includeNullAddress, includeNullDescription, playerOwn, updateUi, numResults, reputation, sortMethod, startIndex, filterText, false };
	}

	// GameMessages::HandleEnterProperty
	struct LegacyEnterProperty { uint32_t index{}; bool returnToZone{}; };
	inline LegacyEnterProperty ReadEnterProperty(RakNet::BitStream& inStream) {
		uint32_t index{};
		bool returnToZone{};

		inStream.Read(index);
		inStream.Read(returnToZone);
		return { index, returnToZone };
	}

	// GameMessages::HandleUpdatePropertyPerformanceCost
	inline float ReadUpdatePropertyPerformanceCost(RakNet::BitStream& inStream) {
		float performanceCost = 0.0f;

		if (inStream.ReadBit()) inStream.Read(performanceCost);
		return performanceCost;
	}

	// PlayerReports' ReadDescriptionAndObject (ReportOffensiveModel / ReportOffensiveProperty), MAX_BODY = 2000
	constexpr uint32_t MAX_BODY = 2000;
	// Description (u32 length + UTF-16) followed by an object ID
	inline bool ReadDescriptionAndObject(RakNet::BitStream& inStream, std::string& description, LWOOBJID& objectId) {
		uint32_t length{};
		if (!inStream.Read(length) || length > MAX_BODY) return false;
		std::u16string text;
		text.reserve(length);
		for (uint32_t i = 0; i < length; i++) {
			char16_t character{};
			if (!inStream.Read(character)) return false;
			text.push_back(character);
		}
		description = GeneralUtils::UTF16ToWTF8(text);
		return inStream.Read(objectId);
	}
}

#endif // PROPERTYMESSAGESLEGACY_H
