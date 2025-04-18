#include "SQLiteDatabase.h"

void SQLiteDatabase::InsertNewMail(const MailInfo& mail) {
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

std::vector<MailInfo> SQLiteDatabase::GetMailForPlayer(const uint32_t characterId, const uint32_t numberOfMail) {
	auto [_, res] = ExecuteSelect(
		"SELECT id, subject, body, sender_name, attachment_id, attachment_lot, attachment_subkey, attachment_count, was_read, time_sent"
		" FROM mail WHERE receiver_id=? limit ?;",
		characterId, numberOfMail);

	std::vector<MailInfo> toReturn;

	while (!res.eof()) {
		MailInfo mail;
		mail.id = res.getInt64Field("id");
		mail.subject = res.getStringField("subject");
		mail.body = res.getStringField("body");
		mail.senderUsername = res.getStringField("sender_name");
		mail.itemID = res.getIntField("attachment_id");
		mail.itemLOT = res.getIntField("attachment_lot");
		mail.itemSubkey = res.getIntField("attachment_subkey");
		mail.itemCount = res.getIntField("attachment_count");
		mail.timeSent = res.getInt64Field("time_sent");
		mail.wasRead = res.getIntField("was_read");

		toReturn.push_back(std::move(mail));
		res.nextRow();
	}

	return toReturn;
}

std::optional<MailInfo> SQLiteDatabase::GetMail(const uint64_t mailId) {
	auto [_, res] = ExecuteSelect("SELECT attachment_lot, attachment_count FROM mail WHERE id=? LIMIT 1;", mailId);

	if (res.eof()) {
		return std::nullopt;
	}

	MailInfo toReturn;
	toReturn.itemLOT = res.getIntField("attachment_lot");
	toReturn.itemCount = res.getIntField("attachment_count");

	return toReturn;
}

uint32_t SQLiteDatabase::GetUnreadMailCount(const uint32_t characterId) {
	auto [_, res] = ExecuteSelect("SELECT COUNT(*) AS number_unread FROM mail WHERE receiver_id=? AND was_read=0;", characterId);

	if (res.eof()) {
		return 0;
	}

	return res.getIntField("number_unread");
}

void SQLiteDatabase::MarkMailRead(const uint64_t mailId) {
	ExecuteUpdate("UPDATE mail SET was_read=1 WHERE id=?;", mailId);
}

void SQLiteDatabase::ClaimMailItem(const uint64_t mailId) {
	ExecuteUpdate("UPDATE mail SET attachment_lot=0 WHERE id=?;", mailId);
}

void SQLiteDatabase::DeleteMail(const uint64_t mailId) {
	ExecuteDelete("DELETE FROM mail WHERE id=?;", mailId);
}
