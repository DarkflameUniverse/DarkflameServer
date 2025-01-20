#include "Mail.h"
#include <functional>
#include <string>
#include <algorithm>
#include <regex>
#include <time.h>
#include <future>

#include "GeneralUtils.h"
#include "Database.h"
#include "Game.h"
#include "dServer.h"
#include "Entity.h"
#include "Character.h"
#include "BitStreamUtils.h"
#include "Logger.h"
#include "EntityManager.h"
#include "InventoryComponent.h"
#include "GameMessages.h"
#include "Item.h"
#include "MissionComponent.h"
#include "ChatPackets.h"
#include "Character.h"
#include "dZoneManager.h"
#include "WorldConfig.h"
#include "eMissionTaskType.h"
#include "eReplicaComponentType.h"
#include "eConnectionType.h"
#include "User.h"
#include "StringifiedEnum.h"

namespace {
	const std::string DefaultSender = "%[MAIL_SYSTEM_NOTIFICATION]";
}

namespace Mail {
	std::map<eMessageID, std::function<std::unique_ptr<MailLUBitStream>()>> g_Handlers = {
		{eMessageID::SendRequest, []() {
			return std::make_unique<SendRequest>();
		}},
		{eMessageID::DataRequest, []() {
			return std::make_unique<DataRequest>();
		}},
		{eMessageID::AttachmentCollectRequest, []() {
			return std::make_unique<AttachmentCollectRequest>();
		}},
		{eMessageID::DeleteRequest, []() {
			return std::make_unique<DeleteRequest>();
		}},
		{eMessageID::ReadRequest, []() {
			return std::make_unique<ReadRequest>();
		}},
		{eMessageID::NotificationRequest, []() {
			return std::make_unique<NotificationRequest>();
		}},
	};

	void MailLUBitStream::Serialize(RakNet::BitStream& bitStream) const {
		LOG("Writing %s", StringifiedEnum::ToString(messageID).data());
		bitStream.Write(messageID);
	}

	bool MailLUBitStream::Deserialize(RakNet::BitStream& bitstream) {
		VALIDATE_READ(bitstream.Read(messageID));
		return true;
	}

	bool SendRequest::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(mailInfo.Deserialize(bitStream));
		return true;
	}

	void SendRequest::Handle() {
		//std::string subject = GeneralUtils::WStringToString(ReadFromPacket(packet, 50));
		//std::string body = GeneralUtils::WStringToString(ReadFromPacket(packet, 400));
		//std::string recipient = GeneralUtils::WStringToString(ReadFromPacket(packet, 32));

		// Check if the player has restricted mail access
		auto* character = player->GetCharacter();

		if (!character) return;

		if (character->HasPermission(ePermissionMap::RestrictedMailAccess) || character->GetParentUser()->GetIsMuted()) {
			// Send a message to the player
			ChatPackets::SendSystemMessage(
				sysAddr,
				u"This character has restricted mail access."
			);

			SendResponse(Mail::eSendResponse::SenderAccountIsMuted).Send(sysAddr);

			return;
		}

		//Cleanse recipient:
		mailInfo.recipient = std::regex_replace(mailInfo.recipient, std::regex("[^0-9a-zA-Z]+"), "");

		//Inventory::InventoryType itemType;
		int mailCost = Game::zoneManager->GetWorldConfig()->mailBaseFee;
		int stackSize = 0;
		auto inv = static_cast<InventoryComponent*>(player->GetComponent(eReplicaComponentType::INVENTORY));
		Item* item = nullptr;

		if (mailInfo.itemID > 0 && mailInfo.itemCount > 0 && inv) {
			item = inv->FindItemById(mailInfo.itemID);
			if (item) {
				mailCost += (item->GetInfo().baseValue * Game::zoneManager->GetWorldConfig()->mailPercentAttachmentFee);
				stackSize = item->GetCount();
				mailInfo.itemLOT = item->GetLot();
			} else {
				SendResponse(eSendResponse::AttachmentNotFound).Send(sysAddr);
				return;
			}
		}

		//Check if we can even send this mail (negative coins bug):
		if (player->GetCharacter()->GetCoins() - mailCost < 0) {
			SendResponse(eSendResponse::NotEnoughCoins).Send(sysAddr);
			return;
		}

		//Get the receiver's id:
		auto receiverID = Database::Get()->GetCharacterInfo(mailInfo.recipient);

		if (!receiverID) {
			SendResponse(Mail::eSendResponse::RecipientNotFound).Send(sysAddr);
			return;
		}

		//Check if we have a valid receiver:
		if (GeneralUtils::CaseInsensitiveStringCompare(mailInfo.recipient, character->GetName()) || receiverID->id == character->GetID()) {
			SendResponse(Mail::eSendResponse::CannotMailSelf).Send(sysAddr);
			return;
		} else {
			Database::Get()->InsertNewMail(mailInfo);
		}

		SendResponse(Mail::eSendResponse::Success).Send(sysAddr);
		player->GetCharacter()->SetCoins(player->GetCharacter()->GetCoins() - mailCost, eLootSourceType::MAIL);

		LOG("Seeing if we need to remove item with ID/count/LOT: %i %i %i", mailInfo.itemID, mailInfo.itemCount, mailInfo.itemLOT);

		if (inv && mailInfo.itemLOT != 0 && mailInfo.itemCount > 0 && item) {
			LOG("Trying to remove item with ID/count/LOT: %i %i %i", mailInfo.itemID, mailInfo.itemCount, mailInfo.itemLOT);
			inv->RemoveItem(mailInfo.itemLOT, mailInfo.itemCount, INVALID, true);

			auto* missionCompoent = player->GetComponent<MissionComponent>();

			if (missionCompoent != nullptr) {
				missionCompoent->Progress(eMissionTaskType::GATHER, mailInfo.itemLOT, LWOOBJID_EMPTY, "", -mailInfo.itemCount);
			}
		}

		character->SaveXMLToDatabase();
	}

	void SendResponse::Serialize(RakNet::BitStream& bitStream) const {
		MailLUBitStream::Serialize(bitStream);
		bitStream.Write(response);
	}

	void NotificationResponse::Serialize(RakNet::BitStream& bitStream) const {
		MailLUBitStream::Serialize(bitStream);
		LOG("notification: %s", StringifiedEnum::ToString(notification).data());
		bitStream.Write(notification);
		bitStream.Write<uint64_t>(0); // unused
		bitStream.Write<uint64_t>(0); // unused
		LOG("auctionID: %llu", auctionID);
		bitStream.Write(auctionID);
		bitStream.Write<uint64_t>(0); // unused
		LOG("mailCount: %i", mailCount);
		bitStream.Write(mailCount);
	}

	void DataRequest::Handle() {
		LOG("DataRequest::Handle()");
		auto playerMail = Database::Get()->GetMailForPlayer(static_cast<uint32_t>(player->GetObjectID()), 20);
		LOG("DataRequest::Handle() - Got %i mail", playerMail.size());
		DataResponse response;
		response.playerMail = playerMail;
		response.Send(sysAddr);
	}

	void DataResponse::Serialize(RakNet::BitStream& bitStream) const {
		MailLUBitStream::Serialize(bitStream);
		LOG("throtttled: %i", throttled);
		bitStream.Write(this->throttled);
	
		LOG("playerMail.size(): %i", this->playerMail.size());
		bitStream.Write<uint16_t>(this->playerMail.size());
		bitStream.Write<uint16_t>(0); // packing
		for (const auto& mail : this->playerMail) {
			mail.Serialize(bitStream);
		}
	}

	bool AttachmentCollectRequest::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(mailID));
		VALIDATE_READ(bitStream.Read(playerID));
		return true;
	}

	void AttachmentCollectRequest::Handle() {
		if (mailID > 0 && playerID == player->GetObjectID()) {
			auto playerMail = Database::Get()->GetMail(mailID);

			LOT attachmentLOT = 0;
			uint32_t attachmentCount = 0;

			if (playerMail) {
				attachmentLOT = playerMail->itemLOT;
				attachmentCount = playerMail->itemCount;
			}

			auto inv = player->GetComponent<InventoryComponent>();
			if (!inv) return;

			inv->AddItem(attachmentLOT, attachmentCount, eLootSourceType::MAIL);

			Database::Get()->ClaimMailItem(mailID);

			AttachmentCollectResponse(eRemoveAttachmentResponse::Success, mailID).Send(sysAddr);
		}
	}

	void AttachmentCollectResponse::Serialize(RakNet::BitStream& bitStream) const {
		MailLUBitStream::Serialize(bitStream);
		bitStream.Write(status);
		bitStream.Write(mailID);
	}

	bool DeleteRequest::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(mailID));
		VALIDATE_READ(bitStream.Read(playerID));
		return true;
	}

	void DeleteRequest::Handle() {
		DeleteResponse response(mailID);
		if (mailID > 0) {
			Database::Get()->DeleteMail(mailID);
			response.status = eDeleteResponse::Success;
		}
		response.Send(sysAddr);
	}

	void DeleteResponse::Serialize(RakNet::BitStream& bitStream) const {
		MailLUBitStream::Serialize(bitStream);
		bitStream.Write(status);
		bitStream.Write(mailID);
	}

	bool ReadRequest::Deserialize(RakNet::BitStream& bitStream) {
		int32_t unknown;
		VALIDATE_READ(bitStream.Read(unknown));
		VALIDATE_READ(bitStream.Read(mailID));
		return true;
	}

	void ReadRequest::Handle() {
		ReadResponse response;
		response.mailID = mailID;
		response.status = eReadResponse::Success;

		if (mailID > 0) Database::Get()->MarkMailRead(mailID);
		else response.status  = eReadResponse::UnknownError;
		response.Send(sysAddr);
	}

	void ReadResponse::Serialize(RakNet::BitStream& bitStream) const {
		MailLUBitStream::Serialize(bitStream);
		bitStream.Write(status);
		bitStream.Write(mailID);
	}

	void NotificationRequest::Handle() {
		auto character = player->GetCharacter();
		if (!character) {
			NotificationResponse(eNotificationResponse::UnknownError, 0).Send(sysAddr);
			return;
		}

		auto unreadMailCount = Database::Get()->GetUnreadMailCount(character->GetID());
		if (unreadMailCount > 0) NotificationResponse(eNotificationResponse::NewMail, unreadMailCount).Send(sysAddr);
	}
}

// Non Stuct Functions
void Mail::HandleMail(RakNet::BitStream& inStream, const SystemAddress& sysAddr, Entity* player) {
	MailLUBitStream data;
	if (!data.Deserialize(inStream)) {
		LOG_DEBUG("Error Reading Mail header");
		return;
	}

	auto it = g_Handlers.find(data.messageID);
	if (it != g_Handlers.end()) {
		auto request = it->second();
		request->sysAddr = sysAddr;
		request->player = player;
		if (!request->Deserialize(inStream)) {
			LOG_DEBUG("Error Reading Mail Request: %s", StringifiedEnum::ToString(data.messageID).data());
			return;
		}
		request->Handle();
	} else {
		LOG_DEBUG("Unhandled Mail Request with ID: %i", data.messageID);
	}
}

void Mail::SendMail(const Entity* recipient, const std::string& subject, const std::string& body, const LOT attachment,
	const uint16_t attachmentCount) {
	SendMail(
		LWOOBJID_EMPTY,
		DefaultSender,
		recipient->GetObjectID(),
		recipient->GetCharacter()->GetName(),
		subject,
		body,
		attachment,
		attachmentCount,
		recipient->GetSystemAddress()
	);
}

void Mail::SendMail(const LWOOBJID recipient, const std::string& recipientName, const std::string& subject,
	const std::string& body, const LOT attachment, const uint16_t attachmentCount, const SystemAddress& sysAddr) {
	SendMail(
		LWOOBJID_EMPTY,
		DefaultSender,
		recipient,
		recipientName,
		subject,
		body,
		attachment,
		attachmentCount,
		sysAddr
	);
}

void Mail::SendMail(const LWOOBJID sender, const std::string& senderName, const Entity* recipient, const std::string& subject,
	const std::string& body, const LOT attachment, const uint16_t attachmentCount) {
	SendMail(
		sender,
		senderName,
		recipient->GetObjectID(),
		recipient->GetCharacter()->GetName(),
		subject,
		body,
		attachment,
		attachmentCount,
		recipient->GetSystemAddress()
	);
}

void Mail::SendMail(const LWOOBJID sender, const std::string& senderName, LWOOBJID recipient,
	const std::string& recipientName, const std::string& subject, const std::string& body, const LOT attachment,
	const uint16_t attachmentCount, const SystemAddress& sysAddr) {
	MailInfo mailInsert;
	mailInsert.senderUsername = senderName;
	mailInsert.recipient = recipientName;
	mailInsert.subject = subject;
	mailInsert.body = body;
	mailInsert.senderId = sender;
	mailInsert.receiverId = recipient;
	mailInsert.itemCount = attachmentCount;
	mailInsert.itemID = LWOOBJID_EMPTY;
	mailInsert.itemLOT = attachment;
	mailInsert.itemSubkey = LWOOBJID_EMPTY;

	Database::Get()->InsertNewMail(mailInsert);

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) return; // TODO: Echo to chat server

	NotificationResponse(eNotificationResponse::NewMail).Send(sysAddr);
}
