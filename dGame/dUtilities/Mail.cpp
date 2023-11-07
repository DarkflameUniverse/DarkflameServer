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
#include "PacketUtils.h"
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

void Mail::SendMail(const Entity* recipient, const std::string& subject, const std::string& body, const LOT attachment,
	const uint16_t attachmentCount) {
	SendMail(
		LWOOBJID_EMPTY,
		ServerName,
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
		ServerName,
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
	DatabaseStructs::MailInsert mailInsert;
	mailInsert.senderUsername = senderName;
	mailInsert.recipient = recipientName;
	mailInsert.subject = subject;
	mailInsert.body = body;
	mailInsert.senderId = sender;
	mailInsert.receiverId = recipient;
	mailInsert.attachmentCount = attachmentCount;
	mailInsert.itemID = 0;
	mailInsert.itemLOT = attachment;
	mailInsert.subkey = LWOOBJID_EMPTY;

	Database::Get()->InsertNewMail(mailInsert);

	if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) return; // TODO: Echo to chat server

	SendNotification(sysAddr, 1); //Show the "one new mail" message
}

//Because we need it:
std::string ReadWStringAsString(RakNet::BitStream* bitStream, uint32_t size) {
	std::string toReturn = "";
	uint8_t buffer;
	bool isFinishedReading = false;

	for (uint32_t i = 0; i < size; ++i) {
		bitStream->Read(buffer);
		if (!isFinishedReading) toReturn.push_back(buffer);
		if (buffer == '\0') isFinishedReading = true; //so we don't continue to read garbage as part of the string.
		bitStream->Read(buffer); //Read the null term
	}

	return toReturn;
}

void WriteStringAsWString(RakNet::BitStream* bitStream, std::string str, uint32_t size) {
	uint32_t sizeToFill = size - str.size();

	for (uint32_t i = 0; i < str.size(); ++i) {
		bitStream->Write(str[i]);
		bitStream->Write(uint8_t(0));
	}

	for (uint32_t i = 0; i < sizeToFill; ++i) {
		bitStream->Write(uint16_t(0));
	}
}

void Mail::HandleMailStuff(RakNet::BitStream* packet, const SystemAddress& sysAddr, Entity* entity) {
	int mailStuffID = 0;
	packet->Read(mailStuffID);

	auto returnVal = std::async(std::launch::async, [packet, &sysAddr, entity, mailStuffID]() {
		Mail::MailMessageID stuffID = MailMessageID(mailStuffID);
		switch (stuffID) {
		case MailMessageID::AttachmentCollect:
			Mail::HandleAttachmentCollect(packet, sysAddr, entity);
			break;
		case MailMessageID::DataRequest:
			Mail::HandleDataRequest(packet, sysAddr, entity);
			break;
		case MailMessageID::MailDelete:
			Mail::HandleMailDelete(packet, sysAddr);
			break;
		case MailMessageID::MailRead:
			Mail::HandleMailRead(packet, sysAddr);
			break;
		case MailMessageID::NotificationRequest:
			Mail::HandleNotificationRequest(sysAddr, entity->GetObjectID());
			break;
		case MailMessageID::Send:
			Mail::HandleSendMail(packet, sysAddr, entity);
			break;
		default:
			LOG("Unhandled and possibly undefined MailStuffID: %i", int(stuffID));
		}
		});
}

void Mail::HandleSendMail(RakNet::BitStream* packet, const SystemAddress& sysAddr, Entity* entity) {
	//std::string subject = GeneralUtils::WStringToString(ReadFromPacket(packet, 50));
	//std::string body = GeneralUtils::WStringToString(ReadFromPacket(packet, 400));
	//std::string recipient = GeneralUtils::WStringToString(ReadFromPacket(packet, 32));

	// Check if the player has restricted mail access
	auto* character = entity->GetCharacter();

	if (!character) return;

	if (character->HasPermission(ePermissionMap::RestrictedMailAccess)) {
		// Send a message to the player
		ChatPackets::SendSystemMessage(
			sysAddr,
			u"This character has restricted mail access."
		);

		Mail::SendSendResponse(sysAddr, Mail::MailSendResponse::AccountIsMuted);

		return;
	}

	std::string subject = ReadWStringAsString(packet, 50);
	std::string body = ReadWStringAsString(packet, 400);
	std::string recipient = ReadWStringAsString(packet, 32);
	//Cleanse recipient:
	recipient = std::regex_replace(recipient, std::regex("[^0-9a-zA-Z]+"), "");

	uint64_t unknown64 = 0;
	LWOOBJID attachmentID;
	uint16_t attachmentCount;

	packet->Read(unknown64);
	packet->Read(attachmentID);
	packet->Read(attachmentCount); //We don't care about the rest of the packet.
	uint32_t itemID = static_cast<uint32_t>(attachmentID);
	LOT itemLOT = 0;
	//Inventory::InventoryType itemType;
	int mailCost = Game::zoneManager->GetWorldConfig()->mailBaseFee;
	int stackSize = 0;
	auto inv = static_cast<InventoryComponent*>(entity->GetComponent(eReplicaComponentType::INVENTORY));
	Item* item = nullptr;

	if (itemID > 0 && attachmentCount > 0 && inv) {
		item = inv->FindItemById(attachmentID);
		if (item) {
			mailCost += (item->GetInfo().baseValue * Game::zoneManager->GetWorldConfig()->mailPercentAttachmentFee);
			stackSize = item->GetCount();
			itemLOT = item->GetLot();
		} else {
			Mail::SendSendResponse(sysAddr, MailSendResponse::AttachmentNotFound);
			return;
		}
	}

	//Check if we can even send this mail (negative coins bug):
	if (entity->GetCharacter()->GetCoins() - mailCost < 0) {
		Mail::SendSendResponse(sysAddr, MailSendResponse::NotEnoughCoins);
		return;
	}

	//Get the receiver's id:
	auto receiverID = Database::Get()->DoesCharacterExist(recipient);

	if (!receiverID) {
		Mail::SendSendResponse(sysAddr, Mail::MailSendResponse::RecipientNotFound);
		return;
	}

	//Check if we have a valid receiver:
	if (GeneralUtils::CaseInsensitiveStringCompare(recipient, character->GetName()) || receiverID == character->GetObjectID()) {
		Mail::SendSendResponse(sysAddr, Mail::MailSendResponse::CannotMailSelf);
		return;
	} else {
		DatabaseStructs::MailInsert mailInsert;
		mailInsert.senderUsername = character->GetName();
		mailInsert.recipient = recipient;
		mailInsert.subject = subject;
		mailInsert.body = body;
		mailInsert.senderId = character->GetID();
		mailInsert.receiverId = receiverID.value();
		mailInsert.attachmentCount = attachmentCount;
		mailInsert.itemID = itemID;
		mailInsert.itemLOT = itemLOT;
		mailInsert.subkey = LWOOBJID_EMPTY;
		Database::Get()->InsertNewMail(mailInsert);
	}

	Mail::SendSendResponse(sysAddr, Mail::MailSendResponse::Success);
	entity->GetCharacter()->SetCoins(entity->GetCharacter()->GetCoins() - mailCost, eLootSourceType::MAIL);

	LOG("Seeing if we need to remove item with ID/count/LOT: %i %i %i", itemID, attachmentCount, itemLOT);

	if (inv && itemLOT != 0 && attachmentCount > 0 && item) {
		LOG("Trying to remove item with ID/count/LOT: %i %i %i", itemID, attachmentCount, itemLOT);
		inv->RemoveItem(itemLOT, attachmentCount, INVALID, true);

		auto* missionCompoent = entity->GetComponent<MissionComponent>();

		if (missionCompoent != nullptr) {
			missionCompoent->Progress(eMissionTaskType::GATHER, itemLOT, LWOOBJID_EMPTY, "", -attachmentCount);
		}
	}

	character->SaveXMLToDatabase();
}

void Mail::HandleDataRequest(RakNet::BitStream* packet, const SystemAddress& sysAddr, Entity* player) {
	sql::PreparedStatement* stmt = Database::Get()->CreatePreppedStmt("SELECT * FROM mail WHERE receiver_id=? limit 20;");
	stmt->setUInt(1, player->GetCharacter()->GetObjectID());
	sql::ResultSet* res = stmt->executeQuery();

	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::MAIL);
	bitStream.Write(int(MailMessageID::MailData));
	bitStream.Write(int(0));

	bitStream.Write(uint16_t(res->rowsCount()));
	bitStream.Write(uint16_t(0));

	if (res->rowsCount() > 0) {
		while (res->next()) {
			bitStream.Write(res->getUInt64(1)); //MailID

			/*std::u16string subject = GeneralUtils::UTF8ToUTF16(res->getString(7));
			std::u16string body = GeneralUtils::UTF8ToUTF16(res->getString(8));
			std::u16string sender = GeneralUtils::UTF8ToUTF16(res->getString(3));

			WriteToPacket(&bitStream, subject, 50);
			WriteToPacket(&bitStream, body, 400);
			WriteToPacket(&bitStream, sender, 32);*/

			WriteStringAsWString(&bitStream, res->getString(7).c_str(), 50); //subject
			WriteStringAsWString(&bitStream, res->getString(8).c_str(), 400); //body
			WriteStringAsWString(&bitStream, res->getString(3).c_str(), 32); //sender

			bitStream.Write(uint32_t(0));
			bitStream.Write(uint64_t(0));

			bitStream.Write(res->getUInt64(9)); //Attachment ID
			LOT lot = res->getInt(10);
			if (lot <= 0) bitStream.Write(LOT(-1));
			else bitStream.Write(lot);
			bitStream.Write(uint32_t(0));

			bitStream.Write(res->getInt64(11)); //Attachment subKey
			bitStream.Write(uint16_t(res->getInt(12))); //Attachment count

			bitStream.Write(uint32_t(0));
			bitStream.Write(uint16_t(0));

			bitStream.Write(uint64_t(res->getUInt64(6))); //time sent (twice?)
			bitStream.Write(uint64_t(res->getUInt64(6)));
			bitStream.Write(uint8_t(res->getBoolean(13))); //was read

			bitStream.Write(uint8_t(0));
			bitStream.Write(uint16_t(0));
			bitStream.Write(uint32_t(0));
		}
	}

	Game::server->Send(&bitStream, sysAddr, false);
	// PacketUtils::SavePacket("Max_Mail_Data.bin", (const char*)bitStream.GetData(), bitStream.GetNumberOfBytesUsed());
}

void Mail::HandleAttachmentCollect(RakNet::BitStream* packet, const SystemAddress& sysAddr, Entity* player) {
	int unknown;
	uint64_t mailID;
	LWOOBJID playerID;
	packet->Read(unknown);
	packet->Read(mailID);
	packet->Read(playerID);

	if (mailID > 0 && playerID == player->GetObjectID()) {
		sql::PreparedStatement* stmt = Database::Get()->CreatePreppedStmt("SELECT attachment_lot, attachment_count FROM mail WHERE id=? LIMIT 1;");
		stmt->setUInt64(1, mailID);
		sql::ResultSet* res = stmt->executeQuery();

		LOT attachmentLOT = 0;
		uint32_t attachmentCount = 0;

		while (res->next()) {
			attachmentLOT = res->getInt(1);
			attachmentCount = res->getInt(2);
		}

		auto inv = static_cast<InventoryComponent*>(player->GetComponent(eReplicaComponentType::INVENTORY));
		if (!inv) return;

		inv->AddItem(attachmentLOT, attachmentCount, eLootSourceType::MAIL);

		Mail::SendAttachmentRemoveConfirm(sysAddr, mailID);

		sql::PreparedStatement* up = Database::Get()->CreatePreppedStmt("UPDATE mail SET attachment_lot=0 WHERE id=?;");
		up->setUInt64(1, mailID);
		up->execute();
		delete up;
		delete res;
		delete stmt;
	}
}

void Mail::HandleMailDelete(RakNet::BitStream* packet, const SystemAddress& sysAddr) {
	int unknown;
	uint64_t mailID;
	LWOOBJID playerID;
	packet->Read(unknown);
	packet->Read(mailID);
	packet->Read(playerID);

	if (mailID > 0) Mail::SendDeleteConfirm(sysAddr, mailID, playerID);
}

void Mail::HandleMailRead(RakNet::BitStream* packet, const SystemAddress& sysAddr) {
	int unknown;
	uint64_t mailID;
	packet->Read(unknown);
	packet->Read(mailID);

	if (mailID > 0) Mail::SendReadConfirm(sysAddr, mailID);
}

void Mail::HandleNotificationRequest(const SystemAddress& sysAddr, uint32_t objectID) {
	auto returnVal = std::async(std::launch::async, [&]() {
		sql::PreparedStatement* stmt = Database::Get()->CreatePreppedStmt("SELECT id FROM mail WHERE receiver_id=? AND was_read=0");
		stmt->setUInt(1, objectID);
		sql::ResultSet* res = stmt->executeQuery();

		if (res->rowsCount() > 0) Mail::SendNotification(sysAddr, res->rowsCount());
		delete res;
		delete stmt;
		});
}

void Mail::SendSendResponse(const SystemAddress& sysAddr, MailSendResponse response) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::MAIL);
	bitStream.Write(int(MailMessageID::SendResponse));
	bitStream.Write(int(response));
	Game::server->Send(&bitStream, sysAddr, false);
}

void Mail::SendNotification(const SystemAddress& sysAddr, int mailCount) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::MAIL);
	uint64_t messageType = 2;
	uint64_t s1 = 0;
	uint64_t s2 = 0;
	uint64_t s3 = 0;
	uint64_t s4 = 0;

	bitStream.Write(messageType);
	bitStream.Write(s1);
	bitStream.Write(s2);
	bitStream.Write(s3);
	bitStream.Write(s4);
	bitStream.Write(mailCount);
	bitStream.Write(int(0)); //Unknown
	Game::server->Send(&bitStream, sysAddr, false);
}

void Mail::SendAttachmentRemoveConfirm(const SystemAddress& sysAddr, uint64_t mailID) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::MAIL);
	bitStream.Write(int(MailMessageID::AttachmentCollectConfirm));
	bitStream.Write(int(0)); //unknown
	bitStream.Write(mailID);
	Game::server->Send(&bitStream, sysAddr, false);
}

void Mail::SendDeleteConfirm(const SystemAddress& sysAddr, uint64_t mailID, LWOOBJID playerID) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::MAIL);
	bitStream.Write(int(MailMessageID::MailDeleteConfirm));
	bitStream.Write(int(0)); //unknown
	bitStream.Write(mailID);
	Game::server->Send(&bitStream, sysAddr, false);

	sql::PreparedStatement* stmt = Database::Get()->CreatePreppedStmt("DELETE FROM mail WHERE id=? LIMIT 1;");
	stmt->setUInt64(1, mailID);
	stmt->execute();
	delete stmt;
}

void Mail::SendReadConfirm(const SystemAddress& sysAddr, uint64_t mailID) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::MAIL);
	bitStream.Write(int(MailMessageID::MailReadConfirm));
	bitStream.Write(int(0)); //unknown
	bitStream.Write(mailID);
	Game::server->Send(&bitStream, sysAddr, false);

	sql::PreparedStatement* stmt = Database::Get()->CreatePreppedStmt("UPDATE mail SET was_read=1 WHERE id=?");
	stmt->setUInt64(1, mailID);
	stmt->execute();
	delete stmt;
}
