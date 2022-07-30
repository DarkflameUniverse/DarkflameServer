#pragma once
#include "BitStream.h"
#include "RakNetTypes.h"
#include "dCommonVars.h"

class Entity;

namespace Mail {
	enum class MailMessageID {
		Send = 0x00,
		SendResponse = 0x01,
		DataRequest = 0x03,
		MailData = 0x04,
		AttachmentCollect = 0x05,
		AttachmentCollectConfirm = 0x06,
		MailDelete = 0x07,
		MailDeleteConfirm = 0x08,
		MailRead = 0x09,
		MailReadConfirm = 0x0a,
		NotificationRequest = 0x0b
	};

	enum class MailSendResponse {
		Success = 0,
		NotEnoughCoins,
		AttachmentNotFound,
		ItemCannotBeMailed,
		CannotMailSelf,
		RecipientNotFound,
		DifferentFaction,
		Unknown,
		ModerationFailure,
		AccountIsMuted,
		UnknownFailure,
		RecipientIsIgnored,
		UnknownFailure3,
		RecipientIsFTP
	};

	const std::string ServerName = "Darkflame Universe";

	void SendMail(
		const Entity* recipient,
		const std::string& subject,
		const std::string& body,
		LOT attachment,
		uint16_t attachmentCount
	);

	void SendMail(
		LWOOBJID recipient,
		const std::string& recipientName,
		const std::string& subject,
		const std::string& body,
		LOT attachment,
		uint16_t attachmentCount,
		const SystemAddress& sysAddr
	);

	void SendMail(
		LWOOBJID sender,
		const std::string& senderName,
		const Entity* recipient,
		const std::string& subject,
		const std::string& body,
		LOT attachment,
		uint16_t attachmentCount
	);

	void SendMail(
		LWOOBJID sender,
		const std::string& senderName,
		LWOOBJID recipient,
		const std::string& recipientName,
		const std::string& subject,
		const std::string& body,
		LOT attachment,
		uint16_t attachmentCount,
		const SystemAddress& sysAddr
	);

	void HandleMailStuff(RakNet::BitStream* packet, const SystemAddress& sysAddr, Entity* entity);
	void HandleSendMail(RakNet::BitStream* packet, const SystemAddress& sysAddr, Entity* entity);
	void HandleDataRequest(RakNet::BitStream* packet, const SystemAddress& sysAddr, Entity* player);
	void HandleAttachmentCollect(RakNet::BitStream* packet, const SystemAddress& sysAddr, Entity* player);
	void HandleMailDelete(RakNet::BitStream* packet, const SystemAddress& sysAddr);
	void HandleMailRead(RakNet::BitStream* packet, const SystemAddress& sysAddr);
	void HandleNotificationRequest(const SystemAddress& sysAddr, uint32_t objectID);

	void SendSendResponse(const SystemAddress& sysAddr, MailSendResponse response);
	void SendNotification(const SystemAddress& sysAddr, int mailCount);
	void SendAttachmentRemoveConfirm(const SystemAddress& sysAddr, uint64_t mailID);
	void SendDeleteConfirm(const SystemAddress& sysAddr, uint64_t mailID, LWOOBJID playerID);
	void SendReadConfirm(const SystemAddress& sysAddr, uint64_t mailID);
};
