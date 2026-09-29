#include "MySQLDatabase.h"


void MySQLDatabase::InsertNewMail(const MailInfo& mail) {
	ExecuteInsert(
		"INSERT INTO `mail` "
		"(`sender_id`, `sender_name`, `receiver_id`, `receiver_name`, `time_sent`, `subject`, `body`, `attachment_id`, `attachment_lot`, `attachment_subkey`, `attachment_count`, `attachment_config`, `was_read`)"
		" VALUES (?,?,?,?,?,?,?,?,?,?,?,?,0)",
		mail.senderId,
		mail.senderUsername,
		mail.receiverId,
		mail.recipient,
		static_cast<uint32_t>(time(NULL)),
		mail.subject,
		mail.body,
		mail.itemID,
		mail.itemLOT,
		mail.itemSubkey,
		mail.itemCount,
		mail.itemConfig);
}

std::vector<MailInfo> MySQLDatabase::GetMailForPlayer(const LWOOBJID characterId, const uint32_t numberOfMail) {
	auto res = ExecuteSelect(
		"SELECT id, subject, body, sender_name, attachment_id, attachment_lot, attachment_subkey, attachment_count, was_read, time_sent"
		" FROM mail WHERE receiver_id=? AND deleted_at=0 limit ?;",
		characterId, numberOfMail);

	std::vector<MailInfo> toReturn;
	toReturn.reserve(res->rowsCount());

	while (res->next()) {
		MailInfo mail;
		mail.id = res->getUInt64("id");
		mail.subject = res->getString("subject").c_str();
		mail.body = res->getString("body").c_str();
		mail.senderUsername = res->getString("sender_name").c_str();
		mail.itemID = res->getInt64("attachment_id");
		mail.itemLOT = res->getInt("attachment_lot");
		mail.itemSubkey = res->getInt64("attachment_subkey");
		mail.itemCount = res->getInt("attachment_count");
		mail.timeSent = res->getUInt64("time_sent");
		mail.wasRead = res->getBoolean("was_read");

		toReturn.push_back(std::move(mail));
	}

	return toReturn;
}

std::optional<MailInfo> MySQLDatabase::GetMail(const uint64_t mailId) {
	auto res = ExecuteSelect("SELECT sender_id, attachment_id, attachment_lot, attachment_subkey, attachment_count, attachment_config, receiver_id FROM mail WHERE id=? AND deleted_at=0 LIMIT 1;", mailId);

	if (!res->next()) {
		return std::nullopt;
	}

	MailInfo toReturn;
	toReturn.id = mailId;
	toReturn.senderId = res->getInt64("sender_id");
	toReturn.itemID = res->getInt64("attachment_id");
	toReturn.itemLOT = res->getInt("attachment_lot");
	toReturn.itemSubkey = res->getInt64("attachment_subkey");
	toReturn.itemCount = res->getInt("attachment_count");
	toReturn.itemConfig = res->isNull("attachment_config") ? "" : std::string(res->getString("attachment_config").c_str());
	toReturn.receiverId = res->getUInt64("receiver_id");

	return toReturn;
}

uint32_t MySQLDatabase::GetUnreadMailCount(const LWOOBJID characterId) {
	auto res = ExecuteSelect("SELECT COUNT(*) AS number_unread FROM mail WHERE receiver_id=? AND was_read=0 AND deleted_at=0;", characterId);

	if (!res->next()) {
		return 0;
	}

	return res->getInt("number_unread");
}

void MySQLDatabase::MarkMailRead(const uint64_t mailId) {
	ExecuteUpdate("UPDATE mail SET was_read=1 WHERE id=? LIMIT 1;", mailId);
}

void MySQLDatabase::ClaimMailItem(const uint64_t mailId) {
	ExecuteUpdate("UPDATE mail SET attachment_lot=0 WHERE id=? LIMIT 1;", mailId);
}

void MySQLDatabase::DeleteMail(const uint64_t mailId) {
	// Kept for staff (the dashboard shows it as deleted); every read for the game skips it
	ExecuteUpdate("UPDATE mail SET deleted_at=? WHERE id=? AND deleted_at=0 LIMIT 1;", static_cast<int64_t>(time(NULL)), mailId);
}
