#include "MySQLDatabase.h"

void MySQLDatabase::InsertNewMail(const IMail::MailInfo& mail) {
	ExecuteInsert(
		"INSERT INTO `mail` "
		"(`sender_id`, `sender_name`, `receiver_id`, `receiver_name`, `time_sent`, `subject`, `body`, `attachment_id`, `attachment_lot`, `attachment_subkey`, `attachment_count`, `was_read`)"
		" VALUES (?,?,?,?,?,?,?,?,?,?,?,0)",
		mail.senderId,
		mail.senderUsername,
		mail.receiverId,
		mail.recipient,
		static_cast<uint32_t>(time(NULL)),
		mail.subject,
		mail.body,
		mail.itemID,
		mail.itemLOT,
		0,
		mail.itemCount);
}

std::vector<IMail::MailInfo> MySQLDatabase::GetMailForPlayer(const uint32_t characterId, const uint32_t numberOfMail) {
	auto res = ExecuteSelect(
		"SELECT id, subject, body, sender_name, attachment_id, attachment_lot, attachment_subkey, attachment_count, was_read, time_sent"
		" FROM mail WHERE receiver_id=? limit ?;",
		characterId, numberOfMail);

	std::vector<IMail::MailInfo> toReturn;
	toReturn.reserve(res->rowsCount());

	while (res->next()) {
		IMail::MailInfo mail;
		mail.id = res->getUInt64("id");
		mail.subject = res->getString("subject").c_str();
		mail.body = res->getString("body").c_str();
		mail.senderUsername = res->getString("sender_name").c_str();
		mail.itemID = res->getUInt("attachment_id");
		mail.itemLOT = res->getInt("attachment_lot");
		mail.itemSubkey = res->getInt("attachment_subkey");
		mail.itemCount = res->getInt("attachment_count");
		mail.timeSent = res->getUInt64("time_sent");
		mail.wasRead = res->getBoolean("was_read");

		toReturn.push_back(std::move(mail));
	}

	return toReturn;
}

std::optional<IMail::MailInfo> MySQLDatabase::GetMail(const uint64_t mailId) {
	auto res = ExecuteSelect("SELECT attachment_lot, attachment_count FROM mail WHERE id=? LIMIT 1;", mailId);

	if (!res->next()) {
		return std::nullopt;
	}

	IMail::MailInfo toReturn;
	toReturn.itemLOT = res->getInt("attachment_lot");
	toReturn.itemCount = res->getInt("attachment_count");

	return toReturn;
}

uint32_t MySQLDatabase::GetUnreadMailCount(const uint32_t characterId) {
	auto res = ExecuteSelect("SELECT COUNT(*) AS number_unread FROM mail WHERE receiver_id=? AND was_read=0;", characterId);

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
	ExecuteDelete("DELETE FROM mail WHERE id=? LIMIT 1;", mailId);
}
