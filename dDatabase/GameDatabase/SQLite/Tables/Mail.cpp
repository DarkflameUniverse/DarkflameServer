#include "SQLiteDatabase.h"

void SQLiteDatabase::InsertNewMail(const MailInfo& mail) {
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

std::vector<MailInfo> SQLiteDatabase::GetMailForPlayer(const LWOOBJID characterId, const uint32_t numberOfMail) {
	auto [_, res] = ExecuteSelect(
		"SELECT id, subject, body, sender_name, attachment_id, attachment_lot, attachment_subkey, attachment_count, was_read, time_sent"
		" FROM mail WHERE receiver_id=? AND deleted_at=0 limit ?;",
		characterId, numberOfMail);

	std::vector<MailInfo> toReturn;

	while (!res.eof()) {
		MailInfo mail;
		mail.id = res.getInt64Field("id");
		mail.subject = res.getStringField("subject");
		mail.body = res.getStringField("body");
		mail.senderUsername = res.getStringField("sender_name");
		mail.itemID = res.getInt64Field("attachment_id");
		mail.itemLOT = res.getIntField("attachment_lot");
		mail.itemSubkey = res.getInt64Field("attachment_subkey");
		mail.itemCount = res.getIntField("attachment_count");
		mail.timeSent = res.getInt64Field("time_sent");
		mail.wasRead = res.getIntField("was_read");

		toReturn.push_back(std::move(mail));
		res.nextRow();
	}

	return toReturn;
}

std::optional<MailInfo> SQLiteDatabase::GetMail(const uint64_t mailId) {
	auto [_, res] = ExecuteSelect("SELECT sender_id, attachment_id, attachment_lot, attachment_subkey, attachment_count, attachment_config, receiver_id FROM mail WHERE id=? AND deleted_at=0 LIMIT 1;", mailId);

	if (res.eof()) {
		return std::nullopt;
	}

	MailInfo toReturn;
	toReturn.id = mailId;
	toReturn.senderId = res.getInt64Field("sender_id");
	toReturn.itemID = res.getInt64Field("attachment_id");
	toReturn.itemLOT = res.getIntField("attachment_lot");
	toReturn.itemSubkey = res.getInt64Field("attachment_subkey");
	toReturn.itemCount = res.getIntField("attachment_count");
	toReturn.itemConfig = res.fieldIsNull("attachment_config") ? "" : res.getStringField("attachment_config");
	toReturn.receiverId = res.getInt64Field("receiver_id");

	return toReturn;
}

uint32_t SQLiteDatabase::GetUnreadMailCount(const LWOOBJID characterId) {
	auto [_, res] = ExecuteSelect("SELECT COUNT(*) AS number_unread FROM mail WHERE receiver_id=? AND was_read=0 AND deleted_at=0;", characterId);

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
	// Kept for staff (the dashboard shows it as deleted); every read for the game skips it
	ExecuteUpdate("UPDATE mail SET deleted_at=? WHERE id=? AND deleted_at=0;", static_cast<int64_t>(time(NULL)), mailId);
}
