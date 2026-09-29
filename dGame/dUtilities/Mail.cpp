#include "Mail.h"
#include "DashboardNotify.h"
#include <functional>
#include <string>
#include <algorithm>
#include <regex>
#include <time.h>
#include <future>

#include "GeneralUtils.h"
#include "Database.h"
#include "Game.h"
#include "dConfig.h"
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
#include "ChatServerLink.h"
#include "eObjectBits.h"
#include "Character.h"
#include "dZoneManager.h"
#include "WorldConfig.h"
#include "eMissionTaskType.h"
#include "eReplicaComponentType.h"
#include "ServiceType.h"
#include "User.h"
#include "EconomyLedger.h"
#include "ObjectIDManager.h"
#include "StringifiedEnum.h"
#include "UserManager.h"

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
		if (!character) {
			response.status = eSendResponse::UnknownError;
		} else {
			const bool restrictMailOnMute = UserManager::Instance()->GetMuteRestrictMail() && character->GetParentUser()->GetIsMuted();
			const bool restrictedMailAccess = character->HasPermission(ePermissionMap::RestrictedMailAccess);

			if (character && !(restrictedMailAccess || restrictMailOnMute)) {
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

					if (hasAttachment && (!item || static_cast<uint32_t>(mailInfo.itemCount) > item->GetCount())) {
						response.status = eSendResponse::AttachmentNotFound;
					} else if (player->GetCharacter()->GetCoins() - mailCost < 0) {
						response.status = eSendResponse::NotEnoughCoins;
					} else {
						bool removeSuccess = true;
						// Remove coins and items from the sender
						player->GetCharacter()->SetCoins(player->GetCharacter()->GetCoins() - mailCost, eLootSourceType::MAIL);
						const auto sentItemId = mailInfo.itemID;
						mailInfo.itemSubkey = LWOOBJID_EMPTY;
						mailInfo.itemConfig.clear();
						if (inventoryComponent && hasAttachment && item) {
							// The attached item waits in the mailbox rather than being destroyed, so the ledger records a transfer
							EconomyLedger::ScopedItemTransfer transfer;
							const auto count = static_cast<uint32_t>(mailInfo.itemCount);
							if (count == item->GetCount()) {
								// The whole item goes with its data (subkey, config)
								mailInfo.itemSubkey = item->GetSubKey();
								mailInfo.itemConfig = item->GetConfig().ToLines();
							}
							// Always a new object id while it waits in the mailbox: the sender is saved after the mail is
							// written, so reusing the id could leave two objects with it if the server stopped in between.
							// (Only mail from the dashboard's item restore brings back original ids, for objects that no
							// longer exist.)
							mailInfo.itemID = ObjectIDManager::GetPersistentID();
							// Take the attached item itself, not whichever stack of that LOT comes first
							item->SetCount(item->GetCount() - count);
							auto* missionComponent = player->GetComponent<MissionComponent>();
							if (missionComponent) missionComponent->Progress(eMissionTaskType::GATHER, mailInfo.itemLOT, LWOOBJID_EMPTY, "", -mailInfo.itemCount);
						} else {
							mailInfo.itemID = LWOOBJID_EMPTY;
						}

						// we passed all the checks, now we can actully send the mail
						if (removeSuccess) {
							mailInfo.senderId = character->GetID();
							mailInfo.senderUsername = character->GetName();
							mailInfo.receiverId = receiverID->id;

							Database::Get()->InsertNewMail(mailInfo);
							DashboardNotify::Changed("mail", mailInfo.receiverId);
							NotifyNewMail(mailInfo.receiverId);
							if (hasAttachment) {
								EconomyLedger::RecordTransfer({ .method = IEconomyLedger::eTransferMethod::MAIL_SENT, .itemId = sentItemId,
									.newItemId = mailInfo.itemID, .lot = mailInfo.itemLOT, .count = static_cast<uint32_t>(mailInfo.itemCount),
									.fromCharacter = character->GetID(), .toCharacter = receiverID->id, .zone = Game::server->GetZoneID() });
							}
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
			if (!playerMail || playerMail->receiverId != player->GetObjectID()) {
				response.status = eAttachmentCollectResponse::MailNotFound;
			} else if (!inv->HasSpaceForLoot({ {playerMail->itemLOT, playerMail->itemCount} })) {
				response.status = eAttachmentCollectResponse::NoSpaceInInventory;
			} else {
				// Mail from players only moves items between them; mail from the game (sender 0) hands out new items
				const bool fromPlayer = playerMail->senderId != LWOOBJID_EMPTY;
				std::optional<EconomyLedger::ScopedItemTransfer> transfer;
				if (fromPlayer) transfer.emplace();

				LwoNameValue config;
				config.InsertLines(playerMail->itemConfig);
				const auto claimed = inv->ReceiveItem(playerMail->itemID, playerMail->itemLOT, playerMail->itemCount, eLootSourceType::MAIL, config, playerMail->itemSubkey);
				Database::Get()->ClaimMailItem(mailID);
				DashboardNotify::Changed("mail", playerMail->receiverId);

				if (fromPlayer) {
					EconomyLedger::RecordTransfer({ .method = IEconomyLedger::eTransferMethod::MAIL_CLAIMED, .itemId = playerMail->itemID,
						.newItemId = claimed.id, .lot = playerMail->itemLOT, .count = static_cast<uint32_t>(playerMail->itemCount),
						.fromCharacter = playerMail->senderId, .toCharacter = playerMail->receiverId, .zone = Game::server->GetZoneID(),
						.merged = claimed.merged });
				}
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

		const auto mailData = Database::Get()->GetMail(mailID);
		response.status = eDeleteResponse::NotFound;
		if (mailData) {
			if (mailData->receiverId != playerID) {
				LOG("Player %llu attempted to delete mail owned by %llu. Possible spoof?", playerID, mailData->receiverId);
			} else {
				if (!(mailData->itemLOT > 0 && mailData->itemCount > 0)) {
					Database::Get()->DeleteMail(mailID);
					DashboardNotify::Changed("mail", playerID);
					response.status = eDeleteResponse::Success;
				} else if (mailData->itemLOT > 0 && mailData->itemCount > 0) {
					response.status = eDeleteResponse::HasAttachments;
				}
			}
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
		response.status = eReadResponse::UnknownError;
		response.mailID = mailID;

		const auto mail = Database::Get()->GetMail(mailID);
		if (mail) {
			if (mail->receiverId == player->GetObjectID()) {
				response.status = eReadResponse::Success;
				Database::Get()->MarkMailRead(mailID);
				DashboardNotify::Changed("mail", mail->receiverId);
			} else {
				LOG("Player %llu tried to mark mail read for player %llu", mail->receiverId, player->GetObjectID());
			}
		} else {
			LOG("No mail by ID %llu found to mark as read.", mailID);
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
	DashboardNotify::Changed("mail", mailInsert.receiverId);

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) {
		NotifyNewMail(recipient);
		return;
	}
	NotificationResponse response;
	response.status = eNotificationResponse::NewMail;
	response.Send(sysAddr);
}

void Mail::NotifyUnreadMailOnLoad(const uint32_t unreadCount, const SystemAddress& sysAddr) {
	if (unreadCount == 0) return;
	NotificationResponse response;
	response.status = eNotificationResponse::NewMail;
	response.mailCount = unreadCount;
	response.Send(sysAddr);
}

bool Mail::NotifyNewMailHere(LWOOBJID receiver) {
	// Mail stores the character ID; the player's object ID also carries the character bit
	GeneralUtils::SetBit(receiver, eObjectBits::CHARACTER);
	auto* const player = Game::entityManager->GetEntity(receiver);
	if (!player || !player->IsPlayer()) return false;
	auto* const character = player->GetCharacter();
	if (!character) return false;

	NotificationResponse response;
	response.status = eNotificationResponse::NewMail;
	response.mailCount = Database::Get()->GetUnreadMailCount(character->GetID());
	response.Send(player->GetSystemAddress());
	return true;
}

void Mail::NotifyNewMail(const LWOOBJID receiver) {
	if (NotifyNewMailHere(receiver)) return;

	ChatPackets::MailNotify notify;
	notify.receiverID = receiver;
	GeneralUtils::SetBit(notify.receiverID, eObjectBits::CHARACTER);
	ChatServerLink::Send(notify, MEDIUM_PRIORITY);
}
