#ifndef __MAIL_H__
#define __MAIL_H__

#include <cstdint>
#include "BitStream.h"
#include "RakNetTypes.h"
#include "dCommonVars.h"
#include "BitStreamUtils.h"
#include "MailInfo.h"

class Entity;

namespace Mail {
	enum class eMessageID : uint32_t {
		SendRequest = 0,
		SendResponse,
		NotificationResponse,
		DataRequest,
		DataResponse,
		AttachmentCollectRequest,
		AttachmentCollectResponse,
		DeleteRequest,
		DeleteResponse,
		ReadRequest,
		ReadResponse,
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

	enum class eSendResponse : uint32_t {
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

	enum class eDeleteResponse : uint32_t {
		Success = 0,
		HasAttachements,
		NotFound,
		Throttled,
		UnknownError
	};

	enum class eRemoveAttachmentResponse : uint32_t {
		Success = 0,
		AttachmentNotFound,
		NoSpaceInInventory,
		MailNotFound,
		Throttled,
		UnknownError
	};

	enum class eNotificationResponse : uint32_t {
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

	enum class eReadResponse : uint32_t {
		Success = 0,
		UnknownError
	};

	struct MailLUBitStream : public LUBitStream {
		eMessageID messageID = eMessageID::UnknownError;
		SystemAddress sysAddr = UNASSIGNED_SYSTEM_ADDRESS;
		Entity* player = nullptr;

		MailLUBitStream() = default;
		MailLUBitStream(eMessageID _messageID) : LUBitStream(eConnectionType::CLIENT, MessageType::Client::MAIL), messageID{_messageID} {};

		virtual void Serialize(RakNet::BitStream& bitStream) const override;
		virtual bool Deserialize(RakNet::BitStream& bitStream) override;
		virtual void Handle() override {};
	};

	struct SendRequest : public MailLUBitStream {
		MailInfo mailInfo;

		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;
	};

	struct SendResponse :public MailLUBitStream {
		eSendResponse response = eSendResponse::UnknownError;
		
		SendResponse(eSendResponse _response) : MailLUBitStream(eMessageID::SendResponse), response{_response} {};
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct NotificationResponse : public MailLUBitStream {
		eNotificationResponse notification = eNotificationResponse::UnknownError;
		LWOOBJID auctionID = LWOOBJID_EMPTY;
		uint32_t mailCount = 1;
		NotificationResponse(eNotificationResponse _notification) : MailLUBitStream(eMessageID::NotificationResponse), notification{_notification} {};
		NotificationResponse(eNotificationResponse _notification, uint32_t _mailCount) : MailLUBitStream(eMessageID::NotificationResponse), notification{_notification}, mailCount{_mailCount} {};
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct DataRequest : public MailLUBitStream {
		bool Deserialize(RakNet::BitStream& bitStream) override { return true; };
		void Handle() override;
	};

	struct DataResponse : public MailLUBitStream {
		uint32_t throttled = 0;
		std::vector<MailInfo> playerMail;

		DataResponse() : MailLUBitStream(eMessageID::DataRequest) {};
		void Serialize(RakNet::BitStream& bitStream) const override;

	};

	struct AttachmentCollectRequest : public MailLUBitStream {
		uint64_t mailID = 0;
		LWOOBJID playerID = LWOOBJID_EMPTY;

		AttachmentCollectRequest() : MailLUBitStream(eMessageID::AttachmentCollectRequest) {};
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;
	};

	struct AttachmentCollectResponse : public MailLUBitStream {
		eRemoveAttachmentResponse status = eRemoveAttachmentResponse::UnknownError;
		uint64_t mailID = 0;

		AttachmentCollectResponse(eRemoveAttachmentResponse _status, uint64_t _mailID) : MailLUBitStream(eMessageID::AttachmentCollectResponse), status{_status}, mailID{_mailID} {};
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct DeleteRequest : public MailLUBitStream {
		uint64_t mailID = 0;
		LWOOBJID playerID = LWOOBJID_EMPTY;


		DeleteRequest() : MailLUBitStream(eMessageID::DeleteRequest) {};
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;
	};

	struct DeleteResponse : public MailLUBitStream {
		eDeleteResponse status = eDeleteResponse::UnknownError;
		uint64_t mailID = 0;
		DeleteResponse(uint64_t _mailID) : MailLUBitStream(eMessageID::DeleteResponse), mailID{_mailID} {};
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct ReadRequest : public MailLUBitStream {
		uint64_t mailID = 0;

		ReadRequest() : MailLUBitStream(eMessageID::ReadRequest) {};
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;
	};

	struct ReadResponse : public MailLUBitStream {
		uint64_t mailID = 0;
		eReadResponse status = eReadResponse::UnknownError;

		ReadResponse() : MailLUBitStream(eMessageID::ReadResponse) {};
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct NotificationRequest : public MailLUBitStream {
		NotificationRequest() : MailLUBitStream(eMessageID::NotificationRequest) {};
		bool Deserialize(RakNet::BitStream& bitStream) override { return true; };
		void Handle() override;
	};

	void HandleMail(RakNet::BitStream& inStream, const SystemAddress& sysAddr, Entity* player);

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
};

#endif // !__MAIL_H__
