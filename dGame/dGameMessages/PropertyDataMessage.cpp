#include "PropertyDataMessage.h"

#include "GeneralUtils.h"

#include "Game.h"
#include "dLogger.h"
#include "CDClientManager.h"

void GameMessages::PropertyDataMessage::Serialize(RakNet::BitStream& stream) const {
	stream.Write<int64_t>(0); // - property id

	stream.Write<int32_t>(TemplateID); // - template id
	stream.Write<uint16_t>(ZoneId); // - map id
	stream.Write<uint16_t>(VendorMapId); // - vendor map id
	stream.Write<uint32_t>(cloneId); // clone id

	const auto& name = GeneralUtils::UTF8ToUTF16(Name);
	stream.Write(uint32_t(name.size()));
	for (uint32_t i = 0; i < name.size(); ++i) {
		stream.Write(uint16_t(name[i]));
	}

	const auto& description = GeneralUtils::UTF8ToUTF16(Description);
	stream.Write(uint32_t(description.size()));
	for (uint32_t i = 0; i < description.size(); ++i) {
		stream.Write(uint16_t(description[i]));
	}

	const auto& owner = GeneralUtils::UTF8ToUTF16(OwnerName);
	stream.Write(uint32_t(owner.size()));
	for (uint32_t i = 0; i < owner.size(); ++i) {
		stream.Write(uint16_t(owner[i]));
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
	stream.Write(uint32_t(spawn.size()));
	for (uint32_t i = 0; i < spawn.size(); ++i) {
		stream.Write(uint16_t(spawn[i]));
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
	// stream.Write(uint32_t(rejectionReasonConverted.size()));
	// for (uint32_t i = 0; i < rejectionReasonConverted.size(); ++i) {
	// 	stream.Write(uint16_t(rejectionReasonConverted[i]));
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

	stream.Write(uint32_t(Paths.size()));

	for (const auto& path : Paths) {
		stream.Write(path.x);
		stream.Write(path.y);
		stream.Write(path.z);
	}
}

GameMessages::PropertyDataMessage::PropertyDataMessage(uint32_t mapID) {
	const auto propertyTemplate = CDClientManager::Instance()->
		GetTable<CDPropertyTemplateTable>("PropertyTemplate")->GetByMapID(mapID);

	TemplateID = propertyTemplate.id;
	ZoneId = propertyTemplate.mapID;
	VendorMapId = propertyTemplate.vendorMapID;
	SpawnName = propertyTemplate.spawnName;
}
