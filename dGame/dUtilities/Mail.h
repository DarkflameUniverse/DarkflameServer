#ifndef __MAIL_H__
#define __MAIL_H__

#include <cstdint>
#include "BitStream.h"
#include "RakNetTypes.h"
#include "dCommonVars.h"
#include "BitStreamUtils.h"
#include "MailInfo.h"
#include "MessageType/Client.h"
#include "ClientPackets.h"

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
		HasAttachments,
		NotFound,
		Throttled,
		UnknownError
	};

	enum class eAttachmentCollectResponse : uint32_t {
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

	enum class eAuctionCreateResponse : uint32_t {
		Success = 0,
		NotEnoughMoney,
		ItemNotFound,
		ItemNotSellable,
		UnknownError
	};

	enum class eAuctionCancelResponse : uint32_t {
		NotFound = 0,
		NotYours,
		HasBid,
		NoLongerExists,
		UnknownError
	};

	struct MailClientLUBitStream : public ClientPackets::ClientLUBitStream {
		eMessageID messageID = eMessageID::UnknownError;
		Entity* player = nullptr;

		MailClientLUBitStream() = default;
		MailClientLUBitStream(eMessageID _messageID) : ClientPackets::ClientLUBitStream(MessageType::Client::MAIL), messageID{_messageID} {};

		virtual void Serialize(RakNet::BitStream& bitStream) const override;
		virtual bool Deserialize(RakNet::BitStream& bitStream) override;
		virtual void Handle() override {};
	};

	struct SendRequest : public MailClientLUBitStream {
		MailInfo mailInfo;

		SendRequest() : MailClientLUBitStream(eMessageID::SendRequest) {}
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;
	};

	struct SendResponse :public MailClientLUBitStream {
		eSendResponse status = eSendResponse::UnknownError;

		SendResponse() : MailClientLUBitStream(eMessageID::SendResponse) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct NotificationResponse : public MailClientLUBitStream {
		eNotificationResponse status = eNotificationResponse::UnknownError;
		LWOOBJID auctionID = LWOOBJID_EMPTY;
		uint32_t mailCount = 1;
		NotificationResponse() : MailClientLUBitStream(eMessageID::NotificationResponse) {};
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct DataRequest : public MailClientLUBitStream {
		DataRequest() : MailClientLUBitStream(eMessageID::DataRequest) {}
		bool Deserialize(RakNet::BitStream& bitStream) override { return true; };
		void Handle() override;
	};

	struct DataResponse : public MailClientLUBitStream {
		uint32_t throttled = 0;
		std::vector<MailInfo> playerMail;

		DataResponse() : MailClientLUBitStream(eMessageID::DataResponse) {};
		void Serialize(RakNet::BitStream& bitStream) const override;

	};

	struct AttachmentCollectRequest : public MailClientLUBitStream {
		uint64_t mailID = 0;
		LWOOBJID playerID = LWOOBJID_EMPTY;

		AttachmentCollectRequest() : MailClientLUBitStream(eMessageID::AttachmentCollectRequest) {};
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;
	};

	struct AttachmentCollectResponse : public MailClientLUBitStream {
		eAttachmentCollectResponse status = eAttachmentCollectResponse::UnknownError;
		uint64_t mailID = 0;
		AttachmentCollectResponse() : MailClientLUBitStream(eMessageID::AttachmentCollectResponse) {};
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct DeleteRequest : public MailClientLUBitStream {
		uint64_t mailID = 0;
		LWOOBJID playerID = LWOOBJID_EMPTY;

		DeleteRequest() : MailClientLUBitStream(eMessageID::DeleteRequest) {};
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;
	};

	struct DeleteResponse : public MailClientLUBitStream {
		eDeleteResponse status = eDeleteResponse::UnknownError;
		uint64_t mailID = 0;
		DeleteResponse() : MailClientLUBitStream(eMessageID::DeleteResponse) {};
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct ReadRequest : public MailClientLUBitStream {
		uint64_t mailID = 0;

		ReadRequest() : MailClientLUBitStream(eMessageID::ReadRequest) {};
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;
	};

	struct ReadResponse : public MailClientLUBitStream {
		uint64_t mailID = 0;
		eReadResponse status = eReadResponse::UnknownError;

		ReadResponse() : MailClientLUBitStream(eMessageID::ReadResponse) {};
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct NotificationRequest : public MailClientLUBitStream {
		NotificationRequest() : MailClientLUBitStream(eMessageID::NotificationRequest) {};
		bool Deserialize(RakNet::BitStream& bitStream) override { return true; };
		void Handle() override;
	};

	void Handle(RakNet::BitStream& inStream, const SystemAddress& sysAddr, Entity* player);

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
