#ifndef __IMAIL__H__
#define __IMAIL__H__

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "dCommonVars.h"
#include "NiQuaternion.h"
#include "NiPoint3.h"
#include "MailInfo.h"
 
class IMail {
public:
	// A mail row as stored, for the dashboard (deleted mail included)
	struct MailRecord {
		uint64_t id{};
		LWOOBJID senderId{};          // 0: from the game or the dashboard
		std::string senderName;
		uint32_t senderAccountId{};   // 0 when the sender isn't a character (or it was deleted)
		LWOOBJID receiverId{};
		std::string receiverName;
		uint32_t receiverAccountId{};
		int64_t timeSent{};
		std::string subject;
		std::string body;
		LWOOBJID attachmentId{};
		LOT attachmentLot{};          // 0 once the attachment was claimed (attachmentCount stays)
		LWOOBJID attachmentSubkey{};
		int32_t attachmentCount{};
		std::string attachmentConfig;
		bool read{};
		int64_t deletedAt{};          // when the player deleted it; 0: not deleted
	};

	enum class eMailState : uint8_t {
		ANY,
		UNREAD,     // not deleted
		READ,       // not deleted
		ATTACHMENT, // an attachment waits to be claimed
		CLAIMED,    // the attachment was claimed
		DELETED     // deleted by the player
	};

	struct MailQuery {
		LWOOBJID characterId{};   // sent or received by this character (0: anyone)
		uint32_t accountId{};     // sent or received by a character of this account (0: anyone)
		std::string search;       // text in the subject, body or a name
		eMailState state{ eMailState::ANY };
		bool includeDeleted{ true };
		uint32_t offset{};
		uint32_t limit{ 100 };
	};

	// Mail rows matching the query, newest first
	virtual std::vector<MailRecord> GetMailHistory(const MailQuery& query) = 0;
	virtual uint64_t CountMailHistory(const MailQuery& query) = 0;

	// Insert a new mail into the database.
	virtual void InsertNewMail(const MailInfo& mail) = 0;

	// Get the mail for the given character id. Mail the player deleted is left out (as in every read below).
	virtual std::vector<MailInfo> GetMailForPlayer(const LWOOBJID characterId, const uint32_t numberOfMail) = 0;

	// Get the mail for the given mail id.
	virtual std::optional<MailInfo> GetMail(const uint64_t mailId) = 0;

	// Get the number of unread mail for the given character id.
	virtual uint32_t GetUnreadMailCount(const LWOOBJID characterId) = 0;

	// Mark the given mail as read.
	virtual void MarkMailRead(const uint64_t mailId) = 0;

	// Claim the item from the given mail.
	virtual void ClaimMailItem(const uint64_t mailId) = 0;

	// A player deleted the mail: it is marked deleted (deleted_at) and kept for staff, and the game never shows it again.
	virtual void DeleteMail(const uint64_t mailId) = 0;
};

#endif  //!__IMAIL__H__
