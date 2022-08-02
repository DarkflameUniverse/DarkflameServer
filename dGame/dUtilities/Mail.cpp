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
#include "dMessageIdentifiers.h"
#include "dLogger.h"
#include "EntityManager.h"
#include "InventoryComponent.h"
#include "GameMessages.h"
#include "Item.h"
#include "MissionComponent.h"
#include "ChatPackets.h"
#include "Character.h"

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
	auto* ins = Database::CreatePreppedStmt("INSERT INTO `mail`(`sender_id`, `sender_name`, `receiver_id`, `receiver_name`, `time_sent`, `subject`, `body`, `attachment_id`, `attachment_lot`, `attachment_subkey`, `attachment_count`, `was_read`) VALUES (?,?,?,?,?,?,?,?,?,?,?,0)");

	ins->setUInt(1, sender);
	ins->setString(2, senderName);
	ins->setUInt(3, recipient);
	ins->setString(4, recipientName.c_str());
	ins->setUInt64(5, time(nullptr));
	ins->setString(6, subject);
	ins->setString(7, body);
	ins->setUInt(8, 0);
	ins->setInt(9, attachment);
	ins->setInt(10, 0);
	ins->setInt(11, attachmentCount);
	ins->execute();

	delete ins;

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

	std::async(std::launch::async, [packet, &sysAddr, entity, mailStuffID]() {
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
			Game::logger->Log("Mail", "Unhandled and possibly undefined MailStuffID: %i", int(stuffID));
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

	if (character->HasPermission(PermissionMap::RestrictedMailAccess)) {
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
	int mailCost = 25;
	int stackSize = 0;
	auto inv = static_cast<InventoryComponent*>(entity->GetComponent(COMPONENT_TYPE_INVENTORY));
	Item* item = nullptr;

	if (itemID > 0 && attachmentCount > 0 && inv) {
		item = inv->FindItemById(attachmentID);
		if (item) {
			mailCost += (item->GetInfo().baseValue * 0.1f);
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
	sql::PreparedStatement* stmt = Database::CreatePreppedStmt("SELECT id from charinfo WHERE name=? LIMIT 1;");
	stmt->setString(1, recipient);
	sql::ResultSet* res = stmt->executeQuery();
	uint32_t receiverID = 0;

	if (res->rowsCount() > 0) {
		while (res->next()) receiverID = res->getUInt(1);
	} else {
		Mail::SendSendResponse(sysAddr, Mail::MailSendResponse::RecipientNotFound);
		delete stmt;
		delete res;
		return;
	}

	delete stmt;
	delete res;

	//Check if we have a valid receiver:
	if (GeneralUtils::CaseInsensitiveStringCompare(recipient, character->GetName()) || receiverID == character->GetObjectID()) {
		Mail::SendSendResponse(sysAddr, Mail::MailSendResponse::CannotMailSelf);
		return;
	} else {
		uint64_t currentTime = time(NULL);
		sql::PreparedStatement* ins = Database::CreatePreppedStmt("INSERT INTO `mail`(`sender_id`, `sender_name`, `receiver_id`, `receiver_name`, `time_sent`, `subject`, `body`, `attachment_id`, `attachment_lot`, `attachment_subkey`, `attachment_count`, `was_read`) VALUES (?,?,?,?,?,?,?,?,?,?,?,0)");
		ins->setUInt(1, character->GetObjectID());
		ins->setString(2, character->GetName());
		ins->setUInt(3, receiverID);
		ins->setString(4, recipient);
		ins->setUInt64(5, currentTime);
		ins->setString(6, subject);
		ins->setString(7, body);
		ins->setUInt(8, itemID);
		ins->setInt(9, itemLOT);
		ins->setInt(10, 0);
		ins->setInt(11, attachmentCount);
		ins->execute();
		delete ins;
	}

	Mail::SendSendResponse(sysAddr, Mail::MailSendResponse::Success);
	entity->GetCharacter()->SetCoins(entity->GetCharacter()->GetCoins() - mailCost, eLootSourceType::LOOT_SOURCE_MAIL);

	Game::logger->Log("Mail", "Seeing if we need to remove item with ID/count/LOT: %i %i %i", itemID, attachmentCount, itemLOT);

	if (inv && itemLOT != 0 && attachmentCount > 0 && item) {
		Game::logger->Log("Mail", "Trying to remove item with ID/count/LOT: %i %i %i", itemID, attachmentCount, itemLOT);
		inv->RemoveItem(itemLOT, attachmentCount, INVALID, true);

		auto* missionCompoent = entity->GetComponent<MissionComponent>();

		if (missionCompoent != nullptr) {
			missionCompoent->Progress(MissionTaskType::MISSION_TASK_TYPE_ITEM_COLLECTION, itemLOT, LWOOBJID_EMPTY, "", -attachmentCount);
		}
	}

	character->SaveXMLToDatabase();
}

void Mail::HandleDataRequest(RakNet::BitStream* packet, const SystemAddress& sysAddr, Entity* player) {
	sql::PreparedStatement* stmt = Database::CreatePreppedStmt("SELECT * FROM mail WHERE receiver_id=? limit 20;");
	stmt->setUInt(1, player->GetCharacter()->GetObjectID());
	sql::ResultSet* res = stmt->executeQuery();

	RakNet::BitStream bitStream;
	PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_MAIL);
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
	PacketUtils::SavePacket("Max_Mail_Data.bin", (const char*)bitStream.GetData(), bitStream.GetNumberOfBytesUsed());
}

void Mail::HandleAttachmentCollect(RakNet::BitStream* packet, const SystemAddress& sysAddr, Entity* player) {
	int unknown;
	uint64_t mailID;
	LWOOBJID playerID;
	packet->Read(unknown);
	packet->Read(mailID);
	packet->Read(playerID);

	if (mailID > 0 && playerID == player->GetObjectID()) {
		sql::PreparedStatement* stmt = Database::CreatePreppedStmt("SELECT attachment_lot, attachment_count FROM mail WHERE id=? LIMIT 1;");
		stmt->setUInt64(1, mailID);
		sql::ResultSet* res = stmt->executeQuery();

		LOT attachmentLOT = 0;
		uint32_t attachmentCount = 0;

		while (res->next()) {
			attachmentLOT = res->getInt(1);
			attachmentCount = res->getInt(2);
		}

		auto inv = static_cast<InventoryComponent*>(player->GetComponent(COMPONENT_TYPE_INVENTORY));
		if (!inv) return;

		inv->AddItem(attachmentLOT, attachmentCount, eLootSourceType::LOOT_SOURCE_MAIL);

		Mail::SendAttachmentRemoveConfirm(sysAddr, mailID);

		sql::PreparedStatement* up = Database::CreatePreppedStmt("UPDATE mail SET attachment_lot=0 WHERE id=?;");
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
	std::async(std::launch::async, [&]() {
		sql::PreparedStatement* stmt = Database::CreatePreppedStmt("SELECT id FROM mail WHERE receiver_id=? AND was_read=0");
		stmt->setUInt(1, objectID);
		sql::ResultSet* res = stmt->executeQuery();

		if (res->rowsCount() > 0) Mail::SendNotification(sysAddr, res->rowsCount());
		delete res;
		delete stmt;
		});
}

void Mail::SendSendResponse(const SystemAddress& sysAddr, MailSendResponse response) {
	RakNet::BitStream bitStream;
	PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_MAIL);
	bitStream.Write(int(MailMessageID::SendResponse));
	bitStream.Write(int(response));
	Game::server->Send(&bitStream, sysAddr, false);
}

void Mail::SendNotification(const SystemAddress& sysAddr, int mailCount) {
	RakNet::BitStream bitStream;
	PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_MAIL);
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
	PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_MAIL);
	bitStream.Write(int(MailMessageID::AttachmentCollectConfirm));
	bitStream.Write(int(0)); //unknown
	bitStream.Write(mailID);
	Game::server->Send(&bitStream, sysAddr, false);
}

void Mail::SendDeleteConfirm(const SystemAddress& sysAddr, uint64_t mailID, LWOOBJID playerID) {
	RakNet::BitStream bitStream;
	PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_MAIL);
	bitStream.Write(int(MailMessageID::MailDeleteConfirm));
	bitStream.Write(int(0)); //unknown
	bitStream.Write(mailID);
	Game::server->Send(&bitStream, sysAddr, false);

	sql::PreparedStatement* stmt = Database::CreatePreppedStmt("DELETE FROM mail WHERE id=? LIMIT 1;");
	stmt->setUInt64(1, mailID);
	stmt->execute();
	delete stmt;
}

void Mail::SendReadConfirm(const SystemAddress& sysAddr, uint64_t mailID) {
	RakNet::BitStream bitStream;
	PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_MAIL);
	bitStream.Write(int(MailMessageID::MailReadConfirm));
	bitStream.Write(int(0)); //unknown
	bitStream.Write(mailID);
	Game::server->Send(&bitStream, sysAddr, false);

	sql::PreparedStatement* stmt = Database::CreatePreppedStmt("UPDATE mail SET was_read=1 WHERE id=?");
	stmt->setUInt64(1, mailID);
	stmt->execute();
	delete stmt;
}
