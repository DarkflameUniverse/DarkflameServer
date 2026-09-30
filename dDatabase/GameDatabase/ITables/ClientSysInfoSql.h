#pragma once

#include <string>

#include "IClientSysInfo.h"

// The SQL both databases use to browse client_sysinfo; `like` is the pattern for "contains ?". Parameters, in order:
// accountId, accountId, search, pattern, pattern.
namespace ClientSysInfoSql {
	inline std::string From(const IClientSysInfo::SysInfoQuery& q, const std::string& like) {
		std::string from = " FROM client_sysinfo AS c LEFT JOIN accounts AS a ON a.id = c.account_id";
		if (q.latestOnly) from += " JOIN (SELECT MAX(id) AS id FROM client_sysinfo GROUP BY account_id) AS n ON n.id = c.id";
		return from + " WHERE (? = 0 OR c.account_id = ?) AND (? = '' OR a.name LIKE " + like + " ESCAPE '!' OR c.video_card LIKE " + like + " ESCAPE '!')";
	}

	inline std::string Order(const IClientSysInfo::SysInfoQuery& q) {
		using eSysInfoOrder = IClientSysInfo::eSysInfoOrder;
		const std::string dir = q.ascending ? " ASC" : " DESC";
		std::string columns;
		switch (q.order) {
		case eSysInfoOrder::LAST_SEEN: columns = "c.last_seen" + dir; break;
		case eSysInfoOrder::ACCOUNT: columns = "a.name" + dir; break;
		case eSysInfoOrder::LOGINS: columns = "c.logins" + dir; break;
		case eSysInfoOrder::OS_VERSION: columns = "c.os_major_version" + dir + ", c.os_minor_version" + dir + ", c.os_build_number" + dir; break;
		case eSysInfoOrder::VIDEO_CARD: columns = "c.video_card" + dir; break;
		case eSysInfoOrder::PROCESSORS: columns = "c.number_of_processors" + dir; break;
		case eSysInfoOrder::MEMORY: columns = "c.memory_total_kb" + dir; break;
		case eSysInfoOrder::CLIENT_OS: columns = "c.client_os" + dir; break;
		case eSysInfoOrder::FIRST_SEEN: columns = "c.first_seen" + dir; break;
		}
		// Equal values come in row order, the same on every database
		return " ORDER BY " + columns + ", c.id" + dir;
	}
}
