#ifndef __IMAIL__H__
#define __IMAIL__H__

#include <cstdint>
#include <optional>
#include <string_view>

#include "dCommonVars.h"
#include "NiQuaternion.h"
#include "NiPoint3.h"

class IMail {
public:
	struct MailInfo {
		std::string senderUsername;
		std::string recipient;
		std::string subject;
		std::string body;
		uint64_t id{};
		uint32_t senderId{};
		uint32_t receiverId{};
		uint64_t timeSent{};
		bool wasRead{};
		struct {
			LWOOBJID itemID{};
			int32_t itemCount{};
			LOT itemLOT{};
			LWOOBJID itemSubkey{};
		};
	};

	// Insert a new mail into the database.
	virtual void InsertNewMail(const MailInfo& mail) = 0;

	// Get the mail for the given character id.
	virtual std::vector<MailInfo> GetMailForPlayer(const uint32_t characterId, const uint32_t numberOfMail) = 0;

	// Get the mail for the given mail id.
	virtual std::optional<MailInfo> GetMail(const uint64_t mailId) = 0;

	// Get the number of unread mail for the given character id.
	virtual uint32_t GetUnreadMailCount(const uint32_t characterId) = 0;

	// Mark the given mail as read.
	virtual void MarkMailRead(const uint64_t mailId) = 0;

	// Claim the item from the given mail.
	virtual void ClaimMailItem(const uint64_t mailId) = 0;

	// Delete the given mail.
	virtual void DeleteMail(const uint64_t mailId) = 0;
};

#endif  //!__IMAIL__H__
