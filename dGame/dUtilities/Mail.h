#ifndef __MAIL_H__
#define __MAIL_H__

#include <cstdint>
#include "BitStream.h"
#include "RakNetTypes.h"
#include "dCommonVars.h"

template <typename T> 
struct LUHeader;

class Entity;

namespace Mail {
	enum class MailMessageID : uint32_t {
		Send = 0,
		SendResponse,
		Notification,
		DataRequest,
		MailData,
		AttachmentCollect,
		AttachmentCollectConfirm,
		MailDelete,
		MailDeleteConfirm,
		MailRead,
		MailReadConfirm,
		NotificationRequest,
		AuctionCreate,
		AuctionCreationResponse,
		AuctionCancel,
		AuctionCancelResponse,
		AuctionList,
		AuctionListResponse,
		AuctionBid,
		AuctionBidResponse,
		UnknownError
	};

	enum class MailSendResponse : uint32_t {
		Success = 0,
		NotEnoughCoins,
		AttachmentNotFound,
		ItemCannotBeMailed,
		CannotMailSelf,
		RecipientNotFound,
		RecipientDifferentFaction,
		UnHandled7,
		ModerationFailure,
		SenderAccountIsMuted,
		UnHandled10,
		RecipientIsIgnored,
		UnHandled12,
		RecipientIsFTP,
		UnknownError
	};

	enum class MailDeleteResponse : uint32_t {
		Success = 0,
		HasAttachements,
		NotFoud,
		Throttled,
		UnknownError
	};

	enum class MailRemoveAttachment : uint32_t {
		Success = 0,
		AttachmentNotFound,
		NoSpaceInInventory,
		MailNotFound,
		Throttled,
		UnknownError
	};

	enum class MailNotification : uint32_t {
		NewMail = 0,
		UnHandled,
		AuctionWon,
		AuctionSold,
		AuctionOutbided,
		AuctionExpired,
		AuctionCancelled,
		AuctionUpdated,
		UnknownError
	};

	enum class MailReadResponse : uint32_t {
		Success = 0,
		UnknownError
	};

	struct MailInfo {
		uint64_t id;
		std::string senderUsername;
		std::string recipient;
		std::string subject;
		std::string body;
		uint32_t senderId;
		uint32_t receiverId;
		uint32_t itemCount;
		uint32_t itemID;
		LOT itemLOT;
		LWOOBJID itemSubkey;
	};

	struct MailLUHeader {
		LUHeader<MessageType::Client> header = LUHeader<MessageType::Client>(eConnectionType::CLIENT, MessageType::Client::MAIL);
		MailMessageID messageID;
	};

	struct Data {
		MailLUHeader header;
		uint32_t throttled = 0;
		std::vector<IMail::MailInfo> playerMail;

		Data() = delete;
		Data(std::vector<IMail::MailInfo> mailData) {
			this->header.messageID = MailMessageID::MailData;
			this->playerMail = mailData;
		}
	};

	struct Response {
		MailLUHeader header;
		MailSendResponse response;

		Response() = delete;
		Response(MailSendResponse response) {
			this->header.messageID = MailMessageID::SendResponse;
			this->response = response;
		}
	};

	struct Notification {
		MailLUHeader header;
		MailNotification notification;
		LWOOBJID auctionID = 0;
		uint32_t mailCount = 0;
		
		Notification() = delete;
		Notification(MailNotification notification, uint32_t mailCount, LWOOBJID auctionID = LWOOBJID_EMPTY) {
			this->header.messageID = MailMessageID::Notification;
			this->notification = notification;
			this->auctionID = auctionID;
			this->mailCount = mailCount;
		}
	};

	struct AttachmentCollect {
		MailLUHeader header;
		MailRemoveAttachment status;
		uint64_t mailID;

		AttachmentCollect() = delete;
		AttachmentCollect(MailRemoveAttachment status, uint64_t mailID) {
			this->header.messageID = MailMessageID::AttachmentCollect;
			this->status = status;
			this->mailID = mailID;
		}
	};

	struct DeleteMail {
		MailLUHeader header;
		MailDeleteResponse status;
		uint64_t mailID;

		DeleteMail() = delete;
		DeleteMail(MailDeleteResponse status, uint64_t mailID) {
			this->status = status;
			this->mailID = mailID;
		}
	};

	struct Read {
		MailLUHeader header;
		MailReadResponse status;
		uint64_t mailID;

		Read() = delete;
		Read(MailReadResponse status, uint64_t mailID) {
			this->status = status;
			this->mailID = mailID;
		}
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

	void HandleMail(RakNet::BitStream& packet, const SystemAddress& sysAddr, Entity* entity);
	void HandleSendMail(RakNet::BitStream& packet, const SystemAddress& sysAddr, Entity* entity);
	void HandleDataRequest(RakNet::BitStream& packet, const SystemAddress& sysAddr, Entity* player);
	void HandleAttachmentCollect(RakNet::BitStream& packet, const SystemAddress& sysAddr, Entity* player);
	void HandleMailDelete(RakNet::BitStream& packet, const SystemAddress& sysAddr);
	void HandleMailRead(RakNet::BitStream& packet, const SystemAddress& sysAddr);
	void HandleNotificationRequest(const SystemAddress& sysAddr, uint32_t objectID);

	void SendSendResponse(const SystemAddress& sysAddr, MailSendResponse response);
	void SendNotification(const SystemAddress& sysAddr, int mailCount);
	void SendAttachmentRemoveConfirm(const SystemAddress& sysAddr, uint64_t mailID);
	void SendDeleteConfirm(const SystemAddress& sysAddr, uint64_t mailID, LWOOBJID playerID);
	void SendReadConfirm(const SystemAddress& sysAddr, uint64_t mailID);
};


namespace RakNet {
	template <>
	inline void RakNet::BitStream::Write<Mail::MailLUHeader>(Mail::MailLUHeader data) {
		this->Write<LUHeader<MessageType::Client>>(data.header);
		this->Write<uint32_t>(static_cast<uint32_t>(data.messageID));
	}

	template <>
	inline void RakNet::BitStream::Write<Mail::Data>(Mail::Data data) {

		this->Write<Mail::MailLUHeader>(data.header);
		this->Write(data.throttled);

		this->Write<uint16_t>(data.playerMail.size());
		this->Write<uint16_t>(0); // packing
		for (auto& mail : data.playerMail) {
			this->Write(mail.id);

			const LUWString subject(mail.subject, 50);
			this->Write(subject);

			const LUWString body(mail.body, 400);
			this->Write(body);

			const LUWString sender(mail.senderUsername, 32);
			this->Write(sender);
			this->Write(uint32_t(0)); // packing

			this->Write(uint64_t(0)); // attachedCurrency
			this->Write(mail.itemID);

			LOT lot = mail.itemLOT;
			if (lot <= 0) this->Write(LOT(-1));
			else this->Write(lot);
			this->Write(uint32_t(0)); // packing

			this->Write(mail.itemSubkey);

			this->Write<uint16_t>(mail.itemCount);
			this->Write(uint8_t(0)); // subject type (used for auction)
			this->Write(uint8_t(0)); // packing
			this->Write(uint32_t(0)); //  packing

			this->Write<uint64_t>(mail.timeSent); // expiration date
			this->Write<uint64_t>(mail.timeSent);// send date
			this->Write<uint8_t>(mail.wasRead); // was read

			this->Write(uint8_t(0)); // isLocalized
			this->Write(uint16_t(0)); // packing
			this->Write(uint32_t(0)); // packing
		}
	}

	template <>
	inline void RakNet::BitStream::Write<Mail::Response>(Mail::Response data) {
		this->Write<Mail::MailLUHeader>(data.header);
		this->Write<uint32_t>(static_cast<uint32_t>(data.response));
	}

	template <>
	inline void RakNet::BitStream::Write<Mail::Notification>(Mail::Notification data) {
		this->Write<Mail::MailLUHeader>(data.header);
		this->Write(data.notification);
		this->Write<uint64_t>(0); // unused
		this->Write<uint64_t>(0); // unused
		this->Write(data.auctionID);
		this->Write<uint64_t>(0); // unused
		this->Write(data.mailCount);
	}

	template <>
	inline void RakNet::BitStream::Write<Mail::AttachmentCollect>(Mail::AttachmentCollect data) {
		this->Write<Mail::MailLUHeader>(data.header);
		this->Write(data.status);
		this->Write(data.mailID);
	}

	template <>
	inline void RakNet::BitStream::Write<Mail::DeleteMail>(Mail::DeleteMail data) {
		this->Write<Mail::MailLUHeader>(data.header);
		this->Write(data.status);
		this->Write(data.mailID);
	}

	template <>
	inline void RakNet::BitStream::Write<Mail::Read>(Mail::Read data) {
		this->Write<Mail::MailLUHeader>(data.header);
		this->Write(data.status);
		this->Write(data.mailID);
	}
}

#endif // !__MAIL_H__
