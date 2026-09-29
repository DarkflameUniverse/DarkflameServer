#include "PlayerMessages.h"

#include "BitStreamUtils.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "DashboardNotify.h"
#include "Database.h"
#include "Entity.h"
#include "GeneralUtils.h"
#include "IBugReports.h"
#include "PlayerReports.h"
#include "SlashCommandHandler.h"

namespace GameMessages {
	void UpdateChatMode::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(level);
	}

	bool UpdateChatMode::Deserialize(RakNet::BitStream& bitStream) {
		return bitStream.Read(level);
	}

	void SetGMLevel::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bOverride);
		bitStream.Write(level);
	}

	bool SetGMLevel::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bOverride));
		VALIDATE_READ(bitStream.Read(level));
		return true;
	}

	void ModifyLEGOScore::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(score);
		BitStreamUtils::WriteOptional(bitStream, sourceType, eLootSourceType::NONE);
	}

	bool ModifyLEGOScore::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(score));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, sourceType, eLootSourceType::NONE));
		return true;
	}

	void SetCurrency::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(currency);
		BitStreamUtils::WriteOptional(bitStream, lootType, LOOTTYPE_NONE);
		bitStream.Write(position.x);
		bitStream.Write(position.y);
		bitStream.Write(position.z);
		BitStreamUtils::WriteOptional(bitStream, sourceLOT, LOT_NULL);
		BitStreamUtils::WriteOptional(bitStream, sourceID, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(bitStream, sourceTradeID, 0);
		BitStreamUtils::WriteOptional(bitStream, sourceType, eLootSourceType::NONE);
	}

	bool SetCurrency::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(currency));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, lootType, LOOTTYPE_NONE));
		VALIDATE_READ(bitStream.Read(position.x));
		VALIDATE_READ(bitStream.Read(position.y));
		VALIDATE_READ(bitStream.Read(position.z));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, sourceLOT, LOT_NULL));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, sourceID, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, sourceTradeID, 0));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, sourceType, eLootSourceType::NONE));
		return true;
	}

	void UpdateReputation::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(reputation);
	}

	bool UpdateReputation::Deserialize(RakNet::BitStream& bitStream) {
		return bitStream.Read(reputation);
	}

	void SetTooltipFlag::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bFlag);
		bitStream.Write(iToolTip);
	}

	bool SetTooltipFlag::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bFlag));
		VALIDATE_READ(bitStream.Read(iToolTip));
		return true;
	}

	void SetTooltipFlag::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* const characterComponent = entity.GetComponent<CharacterComponent>();
		if (characterComponent) characterComponent->SetTooltipFlag(iToolTip, bFlag);
	}

	void ToggleGMInvis::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bStateOut);
	}

	bool ToggleGMInvis::Deserialize(RakNet::BitStream& bitStream) {
		return bitStream.Read(bStateOut);
	}

	void PickupCurrency::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(currency);
	}

	bool PickupCurrency::Deserialize(RakNet::BitStream& bitStream) {
		return bitStream.Read(currency);
	}

	void PickupCurrency::Handle(Entity& entity, const SystemAddress& sysAddr) {
		if (currency == 0) return;

		auto* ch = entity.GetCharacter();
		if (ch && entity.PickupCoins(currency)) {
			ch->SetCoins(ch->GetCoins() + currency, eLootSourceType::PICKUP);
		}
	}

	void ModifyPlayerZoneStatistic::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bSet);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, statName);
		BitStreamUtils::WriteOptional(bitStream, statValue, 0);
		BitStreamUtils::WriteOptional(bitStream, zoneID, LWOMAPID_INVALID);
	}

	bool ModifyPlayerZoneStatistic::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bSet));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, statName));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, statValue, 0));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, zoneID, LWOMAPID_INVALID));
		return true;
	}

	void ModifyPlayerZoneStatistic::Handle(Entity& entity, const SystemAddress& sysAddr) {
		// Notify the character component that something's changed
		auto* characterComponent = entity.GetComponent<CharacterComponent>();
		if (characterComponent != nullptr) {
			characterComponent->HandleZoneStatisticsUpdate(zoneID, statName, statValue);
		}
	}

	void UpdatePlayerStatistic::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(updateID);
		BitStreamUtils::WriteOptional<int64_t>(bitStream, updateValue, 1);
	}

	bool UpdatePlayerStatistic::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(updateID));
		VALIDATE_READ(BitStreamUtils::ReadOptional<int64_t>(bitStream, updateValue, 1));
		return true;
	}

	void UpdatePlayerStatistic::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* characterComponent = entity.GetComponent<CharacterComponent>();
		if (characterComponent != nullptr) {
			characterComponent->UpdatePlayerStatistic(static_cast<StatisticID>(updateID), static_cast<uint64_t>(std::max(updateValue, static_cast<int64_t>(0))), true);
		}
	}

	void ParseChatMessage::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(iClientState);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, wsString);
	}

	bool ParseChatMessage::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(iClientState));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, wsString));
		return true;
	}

	void ParseChatMessage::Handle(Entity& entity, const SystemAddress& sysAddr) {
		if (!wsString.empty() && wsString[0] == L'/') {
			SlashCommandHandler::HandleChatCommand(wsString, &entity, sysAddr);
		}
	}

	void ReportBug::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, body);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, clientVersion);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, otherPlayerID);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, selection);
	}

	bool ReportBug::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, body));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, clientVersion));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, otherPlayerID));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, selection));
		return true;
	}

	void ReportBug::Handle(Entity& entity, const SystemAddress& sysAddr) {
		IBugReports::Info reportInfo;
		// The body is stored narrowed to one byte per character, as DLU always did.
		for (const auto character : body) reportInfo.body.push_back(static_cast<char>(character));
		reportInfo.clientVersion = clientVersion;
		reportInfo.otherPlayer = otherPlayerID;
		reportInfo.selection = selection;

		auto character = entity.GetCharacter();
		if (character) reportInfo.characterId = character->GetID();

		// Report Abuse about another player sends their ID here (and "0" otherwise); that's a player report, not a bug
		if (const auto reportedId = GeneralUtils::TryParse<LWOOBJID>(reportInfo.otherPlayer).value_or(LWOOBJID_EMPTY); reportedId != LWOOBJID_EMPTY) {
			PlayerReports::ReportPlayer(&entity, reportedId, reportInfo.body);
			return;
		}

		Database::Get()->InsertNewBugReport(reportInfo);
		DashboardNotify::Changed("bug_reports");
	}

	void VerifyAck::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bDifferent);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, sBitStream);
		BitStreamUtils::WriteOptional<uint32_t>(bitStream, uiHandle, 0);
	}

	bool VerifyAck::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bDifferent));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, sBitStream));
		VALIDATE_READ(BitStreamUtils::ReadOptional<uint32_t>(bitStream, uiHandle, 0));
		return true;
	}

	void Help::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(helpId);
	}

	bool Help::Deserialize(RakNet::BitStream& bitStream) {
		return bitStream.Read(helpId);
	}
}
