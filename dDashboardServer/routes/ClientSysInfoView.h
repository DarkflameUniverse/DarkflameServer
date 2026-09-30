#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "IClientSysInfo.h"
#include "json.hpp"

/**
 * How the dashboard shows the system info clients send at login (client_sysinfo), without the database or web server.
 * Every value is as reported by the client: old Windows calls in a 32-bit program without a compatibility manifest,
 * so several fields are compatibility values rather than the player's real hardware. Caveats() says what each field
 * is worth; the routes are in ClientSysInfoRoutes.cpp.
 */
namespace ClientSysInfoView {
	// field -> what it is and how far to trust it, for the page and the API
	const nlohmann::json& Caveats();

	// One row as the dashboard sends it: the raw values, plus labels and the memory text split into numbers. The
	// address is only included when showIp (logs_audit).
	nlohmann::json RowJson(const IClientSysInfo::SysInfoRow& row, bool showIp);

	// "6.2.9200", and what Windows version that is reported as
	std::string OsVersion(const IClientSysInfo::SysInfoRow& row);
	std::string OsLabel(const IClientSysInfo::SysInfoRow& row);

	// A physical memory bucket by the reported total: "Under 2 GB", "2-4 GB", ... "64 GB or more", "Not read"
	std::string MemoryBucket(uint64_t totalKb);

	/**
	 * The spread across players from each account's newest row: {accounts, os: [{label, count}], video: [...],
	 * memory: [...], processors: [...], clientOs: [...]}, most common first, at most top entries each (the rest are
	 * one "Other" entry).
	 */
	nlohmann::json Spread(const std::vector<IClientSysInfo::SysInfoRow>& latest, size_t top = 20);
}
