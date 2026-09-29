#ifndef MAILSQL_H
#define MAILSQL_H

#include <string>

#include "IMail.h"

// The dashboard's mail history query, shared by the MySQL and SQLite backends (they differ only in how LIKE patterns are
// built). Every filter binds its parameters whether it is on or off, so one argument list fits every query:
// characterId x3, accountId x3, search, then the search pattern x4.
namespace MailSql {
	inline std::string Select() {
		return "SELECT m.id, m.sender_id, m.sender_name, m.receiver_id, m.receiver_name, m.time_sent, m.subject, m.body, m.attachment_id, "
			"m.attachment_lot, m.attachment_subkey, m.attachment_count, m.attachment_config, m.was_read, m.deleted_at, "
			"COALESCE(s.account_id, 0) AS sender_account, COALESCE(r.account_id, 0) AS receiver_account "
			"FROM mail AS m LEFT JOIN charinfo AS s ON m.sender_id <> 0 AND s.id = m.sender_id LEFT JOIN charinfo AS r ON r.id = m.receiver_id";
	}

	// `like` is the pattern for "contains ?", e.g. CONCAT('%', ?, '%') or '%' || ? || '%'
	inline std::string Where(const IMail::MailQuery& q, const std::string& like) {
		using eMailState = IMail::eMailState;
		std::string where = " WHERE 1 = 1";
		where += q.characterId == 0 ? " AND (? = 0 AND ? = 0 AND ? = 0)" : " AND (? <> 0 AND (m.sender_id = ? OR m.receiver_id = ?))";
		where += q.accountId == 0 ? " AND (? = 0 AND ? = 0 AND ? = 0)"
			: " AND (? <> 0 AND (m.receiver_id IN (SELECT id FROM charinfo WHERE account_id = ?) OR m.sender_id IN (SELECT id FROM charinfo WHERE account_id = ?)))";
		const auto has = [&like](const char* column) { return std::string(column) + " LIKE " + like + " ESCAPE '!'"; };
		where += q.search.empty() ? " AND (? = '' AND ? = '' AND ? = '' AND ? = '' AND ? = '')"
			: " AND (? <> '' AND (" + has("m.subject") + " OR " + has("m.body") + " OR " + has("m.sender_name") + " OR " + has("m.receiver_name") + "))";
		switch (q.state) {
		case eMailState::UNREAD: where += " AND m.was_read = 0 AND m.deleted_at = 0"; break;
		case eMailState::READ: where += " AND m.was_read <> 0 AND m.deleted_at = 0"; break;
		case eMailState::ATTACHMENT: where += " AND m.attachment_lot > 0 AND m.attachment_count > 0"; break;
		case eMailState::CLAIMED: where += " AND m.attachment_lot = 0 AND m.attachment_count > 0"; break;
		case eMailState::DELETED: where += " AND m.deleted_at <> 0"; break;
		case eMailState::ANY: break;
		}
		if (!q.includeDeleted) where += " AND m.deleted_at = 0";
		return where;
	}
}

#endif  //!MAILSQL_H
