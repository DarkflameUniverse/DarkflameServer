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
#include "ServiceType.h"
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
		SendResponse response;
		auto* character = player->GetCharacter();
		if (character && !(character->HasPermission(ePermissionMap::RestrictedMailAccess) || character->GetParentUser()->GetIsMuted())) {
			mailInfo.recipient = std::regex_replace(mailInfo.recipient, std::regex("[^0-9a-zA-Z]+"), "");
			auto receiverID = Database::Get()->GetCharacterInfo(mailInfo.recipient);

			if (!receiverID) {
				response.status = eSendResponse::RecipientNotFound;
			} else if (GeneralUtils::CaseInsensitiveStringCompare(mailInfo.recipient, character->GetName()) || receiverID->id == character->GetID()) {
				response.status = eSendResponse::CannotMailSelf;
			} else {
				uint32_t mailCost = Game::zoneManager->GetWorldConfig().mailBaseFee;
				uint32_t stackSize = 0;
				
				auto inventoryComponent = player->GetComponent<InventoryComponent>();
				Item* item = nullptr;

				bool hasAttachment = mailInfo.itemID != 0 && mailInfo.itemCount > 0;

				if (hasAttachment) {
					item = inventoryComponent->FindItemById(mailInfo.itemID);
					if (item) {
						mailCost += (item->GetInfo().baseValue * Game::zoneManager->GetWorldConfig().mailPercentAttachmentFee);
						mailInfo.itemLOT = item->GetLot();
					}
				}

				if (hasAttachment && !item) {
					response.status = eSendResponse::AttachmentNotFound;
				} else if (player->GetCharacter()->GetCoins() - mailCost < 0) {
					response.status = eSendResponse::NotEnoughCoins;
				} else {
					bool removeSuccess = true;
					// Remove coins and items from the sender
					player->GetCharacter()->SetCoins(player->GetCharacter()->GetCoins() - mailCost, eLootSourceType::MAIL);
					if (inventoryComponent && hasAttachment && item) {
						removeSuccess = inventoryComponent->RemoveItem(mailInfo.itemLOT, mailInfo.itemCount, ALL, true);
						auto* missionComponent = player->GetComponent<MissionComponent>();
						if (missionComponent && removeSuccess) missionComponent->Progress(eMissionTaskType::GATHER, mailInfo.itemLOT, LWOOBJID_EMPTY, "", -mailInfo.itemCount);
					}

					// we passed all the checks, now we can actully send the mail
					if (removeSuccess) {
						mailInfo.senderId = character->GetID();
						mailInfo.senderUsername = character->GetName();
						mailInfo.receiverId = receiverID->id;
						mailInfo.itemSubkey = LWOOBJID_EMPTY;

						//clear out the attachementID
						mailInfo.itemID = 0;

						Database::Get()->InsertNewMail(mailInfo);
						response.status = eSendResponse::Success;
						character->SaveXMLToDatabase(); 
					} else {
						response.status = eSendResponse::AttachmentNotFound;
					}
				}
			}
		} else {
			response.status = eSendResponse::SenderAccountIsMuted;
		}
		LOG("Finished send with status %s", StringifiedEnum::ToString(response.status).data());
		response.Send(sysAddr);
	}

	void SendResponse::Serialize(RakNet::BitStream& bitStream) const {
		MailLUBitStream::Serialize(bitStream);
		bitStream.Write(status);
	}

	void NotificationResponse::Serialize(RakNet::BitStream& bitStream) const {
		MailLUBitStream::Serialize(bitStream);
		bitStream.Write(status);
		bitStream.Write<uint64_t>(0); // unused
		bitStream.Write<uint64_t>(0); // unused
		bitStream.Write(auctionID);
		bitStream.Write<uint64_t>(0); // unused
		bitStream.Write(mailCount);
		bitStream.Write<uint32_t>(0); // packing
	}

	void DataRequest::Handle() {
		const auto* character = player->GetCharacter();
		if (!character) return;
		auto playerMail = Database::Get()->GetMailForPlayer(character->GetID(), 20);
		DataResponse response;
		response.playerMail = playerMail;
		response.Send(sysAddr);
		LOG("DataRequest");
	}

	void DataResponse::Serialize(RakNet::BitStream& bitStream) const {
		MailLUBitStream::Serialize(bitStream);
		bitStream.Write(this->throttled);
	
		bitStream.Write<uint16_t>(this->playerMail.size());
		bitStream.Write<uint16_t>(0); // packing
		for (const auto& mail : this->playerMail) {
			mail.Serialize(bitStream);
		}
	}

	bool AttachmentCollectRequest::Deserialize(RakNet::BitStream& bitStream) {
		uint32_t unknown;
		VALIDATE_READ(bitStream.Read(unknown));
		VALIDATE_READ(bitStream.Read(mailID));
		VALIDATE_READ(bitStream.Read(playerID));
		return true;
	}

	void AttachmentCollectRequest::Handle() {
		AttachmentCollectResponse response;
		response.mailID = mailID;
		auto inv = player->GetComponent<InventoryComponent>();

		if (mailID > 0 && playerID == player->GetObjectID() && inv) {
			auto playerMail = Database::Get()->GetMail(mailID);
			if (!playerMail) {
				response.status = eAttachmentCollectResponse::MailNotFound;
			} else if (!inv->HasSpaceForLoot({ {playerMail->itemLOT, playerMail->itemCount} })) {
				response.status = eAttachmentCollectResponse::NoSpaceInInventory;
			} else {
				inv->AddItem(playerMail->itemLOT, playerMail->itemCount, eLootSourceType::MAIL);
				Database::Get()->ClaimMailItem(mailID);
				response.status = eAttachmentCollectResponse::Success;
			}
		}
		LOG("AttachmentCollectResponse %s", StringifiedEnum::ToString(response.status).data());
		response.Send(sysAddr);
	}

	void AttachmentCollectResponse::Serialize(RakNet::BitStream& bitStream) const {
		MailLUBitStream::Serialize(bitStream);
		bitStream.Write(status);
		bitStream.Write(mailID);
	}

	bool DeleteRequest::Deserialize(RakNet::BitStream& bitStream) {
		int32_t unknown;
		VALIDATE_READ(bitStream.Read(unknown));
		VALIDATE_READ(bitStream.Read(mailID));
		VALIDATE_READ(bitStream.Read(playerID));
		return true;
	}

	void DeleteRequest::Handle() {
		DeleteResponse response;
		response.mailID = mailID;

		auto mailData = Database::Get()->GetMail(mailID);
		if (mailData && !(mailData->itemLOT > 0 && mailData->itemCount > 0)) {
			Database::Get()->DeleteMail(mailID);
			response.status = eDeleteResponse::Success;
		} else if (mailData && mailData->itemLOT > 0 && mailData->itemCount > 0) {
			response.status = eDeleteResponse::HasAttachments;
		} else {
			response.status = eDeleteResponse::NotFound;
		}
		LOG("DeleteRequest status %s", StringifiedEnum::ToString(response.status).data());
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

		if (Database::Get()->GetMail(mailID)) {
			response.status = eReadResponse::Success;
			Database::Get()->MarkMailRead(mailID);
		}

		LOG("ReadRequest %s", StringifiedEnum::ToString(response.status).data());
		response.Send(sysAddr);
	}

	void ReadResponse::Serialize(RakNet::BitStream& bitStream) const {
		MailLUBitStream::Serialize(bitStream);
		bitStream.Write(status);
		bitStream.Write(mailID);
	}

	void NotificationRequest::Handle() {
		NotificationResponse response;
		auto character = player->GetCharacter();
		if (character) {
			auto unreadMailCount = Database::Get()->GetUnreadMailCount(character->GetID());
			response.status = eNotificationResponse::NewMail;
			response.mailCount = unreadMailCount;
		}

		LOG("NotificationRequest %s", StringifiedEnum::ToString(response.status).data());
		response.Send(sysAddr);
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
		LOG("Received mail message %s", StringifiedEnum::ToString(data.messageID).data());
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
	NotificationResponse response;
	response.status = eNotificationResponse::NewMail;
	response.Send(sysAddr);
}
