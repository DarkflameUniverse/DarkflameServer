#include "TradeMessages.h"

#include "BitStreamUtils.h"
#include "Character.h"
#include "ChatPackets.h"
#include "dCommonVars.h"
#include "Entity.h"
#include "EntityManager.h"
#include "ePermissionMap.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "TradingManager.h"
#include "User.h"
#include "UserManager.h"

namespace {
	template<typename T>
	void WriteFlagged(RakNet::BitStream& bitStream, const std::optional<T>& value) {
		bitStream.Write(value.has_value());
		if (value) bitStream.Write(*value);
	}

	template<typename T>
	bool ReadFlagged(RakNet::BitStream& bitStream, std::optional<T>& value) {
		bool present{};
		if (!bitStream.Read(present)) return false;
		if (!present) {
			value.reset();
			return true;
		}
		T read{};
		if (!bitStream.Read(read)) return false;
		value = read;
		return true;
	}

	void WriteEntries(RakNet::BitStream& bitStream, const std::vector<GameMessages::TradeItemEntry>& entries) {
		bitStream.Write<uint32_t>(entries.size());
		for (const auto& entry : entries) entry.Serialize(bitStream);
	}

	bool ReadEntries(RakNet::BitStream& bitStream, std::vector<GameMessages::TradeItemEntry>& entries) {
		uint32_t count{};
		if (!bitStream.Read(count)) return false;
		if (count > MAX_MESSAGE_LENGTH) return false;
		entries.clear();
		for (uint32_t i = 0; i < count; i++) {
			GameMessages::TradeItemEntry entry;
			if (!entry.Deserialize(bitStream)) return false;
			entries.push_back(std::move(entry));
		}
		return true;
	}
}

namespace GameMessages {
	void TradeItemEntry::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(key);
		bitStream.Write(itemID);
		bitStream.Write(templateID);
		WriteFlagged(bitStream, subkey);
		WriteFlagged(bitStream, count);
		WriteFlagged(bitStream, slot);
		WriteFlagged(bitStream, inventoryType);
		bitStream.Write(config.has_value());
		if (config) {
			bitStream.Write(config->size);
			bitStream.Write(config->compressed);
			if (config->compressed) bitStream.Write(config->compressedSize);
			bitStream.WriteBits(config->data.data(), BYTES_TO_BITS(config->data.size()));
		}
		bitStream.Write(unknownFlag);
	}

	bool TradeItemEntry::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(key));
		VALIDATE_READ(bitStream.Read(itemID));
		VALIDATE_READ(bitStream.Read(templateID));
		VALIDATE_READ(ReadFlagged(bitStream, subkey));
		VALIDATE_READ(ReadFlagged(bitStream, count));
		VALIDATE_READ(ReadFlagged(bitStream, slot));
		VALIDATE_READ(ReadFlagged(bitStream, inventoryType));
		bool hasConfig{};
		VALIDATE_READ(bitStream.Read(hasConfig));
		if (hasConfig) {
			Config readConfig;
			VALIDATE_READ(bitStream.Read(readConfig.size));
			VALIDATE_READ(bitStream.Read(readConfig.compressed));
			if (readConfig.compressed) VALIDATE_READ(bitStream.Read(readConfig.compressedSize));
			const uint32_t dataSize = readConfig.compressed ? readConfig.compressedSize : readConfig.size;
			if (BYTES_TO_BITS(static_cast<uint64_t>(dataSize)) > bitStream.GetNumberOfUnreadBits()) return false;
			readConfig.data.resize(dataSize);
			if (dataSize > 0) VALIDATE_READ(bitStream.ReadBits(readConfig.data.data(), BYTES_TO_BITS(dataSize), true));
			config = std::move(readConfig);
		} else {
			config.reset();
		}
		VALIDATE_READ(bitStream.Read(unknownFlag));
		return true;
	}

	void ClientTradeRequest::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bNeedInvitePopUp);
		bitStream.Write(i64Invitee);
	}

	bool ClientTradeRequest::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bNeedInvitePopUp));
		VALIDATE_READ(bitStream.Read(i64Invitee));
		return true;
	}

	void ClientTradeRequest::Handle(Entity& entity, const SystemAddress& sysAddr) {
		// Check if the player has restricted trade access
		auto* character = entity.GetCharacter();
		const bool restrictTradeOnMute = UserManager::Instance()->GetMuteRestrictTrade();

		if (character->HasPermission(ePermissionMap::RestrictedTradeAccess) || (restrictTradeOnMute && character->GetParentUser()->GetIsMuted())) {
			// Send a message to the player
			ChatPackets::SendSystemMessage(
				sysAddr,
				u"Your character has restricted trade access."
			);

			return;
		}

		auto* invitee = Game::entityManager->GetEntity(i64Invitee);

		if (invitee != nullptr && invitee->IsPlayer()) {
			character = invitee->GetCharacter();

			if (character->HasPermission(ePermissionMap::RestrictedTradeAccess) || (restrictTradeOnMute && character->GetParentUser()->GetIsMuted())) {
				// Send a message to the player
				ChatPackets::SendSystemMessage(
					sysAddr,
					u"The character you are trying to trade with has restricted trade access."
				);

				return;
			}

			LOG("Trade request to (%llu)", i64Invitee);

			const auto& trade = TradingManager::Instance()->GetPlayerTrade(entity.GetObjectID());

			if (trade != nullptr) {
				if (!trade->IsParticipant(i64Invitee)) {
					TradingManager::Instance()->CancelTrade(entity.GetObjectID(), trade->GetTradeId());

					TradingManager::Instance()->NewTrade(entity.GetObjectID(), i64Invitee);
				}
			} else {
				TradingManager::Instance()->NewTrade(entity.GetObjectID(), i64Invitee);
			}

			ServerTradeInvite invite;
			invite.target = i64Invitee;
			invite.bNeedInvitePopUp = bNeedInvitePopUp;
			invite.i64Requestor = entity.GetObjectID();
			invite.wsName = GeneralUtils::UTF8ToUTF16(entity.GetCharacter()->GetName());
			invite.Send(invitee->GetSystemAddress());
		}
	}

	void ServerTradeInvite::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bNeedInvitePopUp);
		bitStream.Write(i64Requestor);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, wsName);
	}

	bool ServerTradeInvite::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bNeedInvitePopUp));
		VALIDATE_READ(bitStream.Read(i64Requestor));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, wsName));
		return true;
	}

	void ServerTradeInitialReply::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(i64Invitee);
		bitStream.Write(resultType);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, wsName);
	}

	bool ServerTradeInitialReply::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(i64Invitee));
		VALIDATE_READ(bitStream.Read(resultType));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, wsName));
		return true;
	}

	void ServerTradeFinalReply::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bResult);
		bitStream.Write(i64Invitee);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, wsName);
	}

	bool ServerTradeFinalReply::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bResult));
		VALIDATE_READ(bitStream.Read(i64Invitee));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, wsName));
		return true;
	}

	void ClientTradeUpdate::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(i64Currency);
		WriteEntries(bitStream, inventoryMap);
	}

	bool ClientTradeUpdate::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(i64Currency));
		VALIDATE_READ(ReadEntries(bitStream, inventoryMap));
		return true;
	}

	void ClientTradeUpdate::Handle(Entity& entity, const SystemAddress& sysAddr) {
		LOG("Trade update from (%llu) -> (%llu), (%i)", entity.GetObjectID(), i64Currency, static_cast<uint32_t>(inventoryMap.size()));

		std::vector<TradeItem> items{};

		for (const auto& entry : inventoryMap) {
			const auto unknown1 = entry.subkey.value_or(0);
			const auto unknown2 = entry.count.value_or(0);
			const auto unknown3 = entry.inventoryType.value_or(0);

			items.push_back({ entry.key, entry.templateID, unknown2 });

			LOG("Trade item from (%llu) -> (%llu)/(%llu), (%i), (%llu), (%i), (%i)", entity.GetObjectID(), entry.key, entry.itemID, entry.templateID, unknown1, unknown2, unknown3);
		}

		const auto& trade = TradingManager::Instance()->GetPlayerTrade(entity.GetObjectID());

		if (trade == nullptr) return;

		trade->SetCoins(entity.GetObjectID(), i64Currency);
		trade->SetItems(entity.GetObjectID(), items);
		trade->SendUpdateToOther(entity.GetObjectID());
	}

	void ServerTradeUpdate::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bAboutToPerform);
		bitStream.Write(i64Currency);
		WriteEntries(bitStream, inventoryMap);
	}

	bool ServerTradeUpdate::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bAboutToPerform));
		VALIDATE_READ(bitStream.Read(i64Currency));
		VALIDATE_READ(ReadEntries(bitStream, inventoryMap));
		return true;
	}

	void ClientTradeCancel::Serialize(RakNet::BitStream& bitStream) const {}

	bool ClientTradeCancel::Deserialize(RakNet::BitStream& bitStream) {
		return true;
	}

	void ClientTradeCancel::Handle(Entity& entity, const SystemAddress& sysAddr) {
		const auto& trade = TradingManager::Instance()->GetPlayerTrade(entity.GetObjectID());

		if (trade == nullptr) return;

		LOG("Trade canceled from (%llu)", entity.GetObjectID());

		TradingManager::Instance()->CancelTrade(entity.GetObjectID(), trade->GetTradeId());
	}

	void ClientTradeAccept::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bFirst);
	}

	bool ClientTradeAccept::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bFirst));
		return true;
	}

	void ClientTradeAccept::Handle(Entity& entity, const SystemAddress& sysAddr) {
		LOG("Trade accepted from (%llu) -> (%d)", entity.GetObjectID(), bFirst);

		const auto& trade = TradingManager::Instance()->GetPlayerTrade(entity.GetObjectID());

		if (trade == nullptr) return;

		trade->SetAccepted(entity.GetObjectID(), bFirst);
	}

	void ServerTradeCancel::Serialize(RakNet::BitStream& bitStream) const {}

	bool ServerTradeCancel::Deserialize(RakNet::BitStream& bitStream) {
		return true;
	}

	void ServerTradeAccept::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bFirst);
	}

	bool ServerTradeAccept::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bFirst));
		return true;
	}
}
