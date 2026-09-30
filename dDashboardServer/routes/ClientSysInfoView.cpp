#include "ClientSysInfoView.h"

#include <algorithm>
#include <map>

#include "ClientSysInfo.h"

namespace {
	nlohmann::json Optional(const std::optional<int64_t>& value) {
		return value ? nlohmann::json(*value) : nlohmann::json(nullptr);
	}

	// label -> count, most common first (then by label), the tail folded into "Other"
	nlohmann::json Ranked(const std::map<std::string, uint64_t>& counts, size_t top) {
		std::vector<std::pair<std::string, uint64_t>> sorted(counts.begin(), counts.end());
		std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second != b.second ? a.second > b.second : a.first < b.first; });
		nlohmann::json out = nlohmann::json::array();
		uint64_t other = 0;
		for (size_t i = 0; i < sorted.size(); i++) {
			if (i < top) out.push_back({ {"label", sorted[i].first}, {"count", sorted[i].second} });
			else other += sorted[i].second;
		}
		if (other > 0) out.push_back({ {"label", "Other"}, {"count", other} });
		return out;
	}

	// The adapter part of the video card text: the client adds " (HAL-<vertex processing>)" after it
	std::string Adapter(const std::string& videoCard) {
		const auto paren = videoCard.rfind(" (");
		const auto adapter = paren == std::string::npos ? videoCard : videoCard.substr(0, paren);
		return adapter.empty() ? "(empty)" : adapter;
	}

	std::string ClientOsName(uint32_t clientOs) {
		switch (clientOs) {
		case 1: return "Windows";
		case 2: return "Mac";
		default: return "Unknown (" + std::to_string(clientOs) + ")";
		}
	}
}

const nlohmann::json& ClientSysInfoView::Caveats() {
	static const nlohmann::json caveats = {
		{"ip", "The address the login came from. Only kept while log_login_addresses is on, and only shown with logs_audit."},
		{"clientOs", "1 Windows, 2 Mac: which build of the client this is (from its own settings), not the operating system it runs on."},
		{"memoryStats", "The client's memory text, as sent (the newest login's while the rest stays the same). Physical memory and the commit "
			"limit (pfile) are the system's totals from GlobalMemoryStatusEx; under Wine they are the host's. The vmem figures are the 32-bit "
			"client's own address space (2 or 4 GB), not the system's. p/v bytes are the client process's own use at login. Memory load and "
			"free amounts change every login."},
		{"videoCard", "From Direct3D 9: the adapter description the driver reports, then the device type (HAL, REF or SW) and vertex processing "
			"mode. Usually the real graphics card; under Wine or DXVK it is what the translation layer reports (normally the real card, "
			"sometimes a stand-in). Cut at 127 characters."},
		{"numberOfProcessors", "GetSystemInfo: logical processors a 32-bit program sees (at most 32). Under Wine, the host's count."},
		{"processorType", "GetSystemInfo's old processor type: 586 for every x86 processor seen from a 32-bit program. Says nothing about the CPU."},
		{"processorLevel", "GetSystemInfo: the CPU family number (6 for most Intel CPUs; AMD uses other families, such as 23 or 25 for Ryzen)."},
		{"processorRevision", "GetSystemInfo: model (high byte) and stepping (low byte) within the family."},
		{"osVersionInfoSize", "The size of the structure the client asked GetVersionExW to fill: always 276. Not system info."},
		{"osVersion", "GetVersionExW. The client has no compatibility manifest, so Windows 8.1, 10 and 11 all report 6.2 build 9200. "
			"A compatibility mode reports the version it imitates (XP SP3: 5.1.2600), and Wine or Proton report the Windows version they "
			"are set to. If the call failed the fields hold whatever was in memory."},
		{"platformId", "GetVersionExW: 2 (Windows NT) on every Windows the client runs on."},
	};
	return caveats;
}

const nlohmann::json& ClientSysInfoView::Trust() {
	static const nlohmann::json trust = {
		{"clientOs", "unreliable"}, {"processorType", "unreliable"}, {"osVersionInfoSize", "unreliable"}, {"osVersion", "unreliable"},
		{"platformId", "unreliable"}, {"memoryStats", "approximate"}, {"videoCard", "approximate"}, {"numberOfProcessors", "approximate"},
		{"processorLevel", "approximate"}, {"processorRevision", "approximate"},
	};
	return trust;
}

std::string ClientSysInfoView::OsVersion(const IClientSysInfo::SysInfoRow& row) {
	return std::to_string(row.majorVersion) + "." + std::to_string(row.minorVersion) + "." + std::to_string(row.buildNumber);
}

std::string ClientSysInfoView::OsLabel(const IClientSysInfo::SysInfoRow& row) {
	const auto major = row.majorVersion, minor = row.minorVersion;
	if (major == 5 && minor == 1) return "Windows XP (or XP compatibility mode)";
	if (major == 5 && minor == 2) return "Windows XP x64 / Server 2003 (or compatibility mode)";
	if (major == 6 && minor == 0) return "Windows Vista (or Vista compatibility mode)";
	if (major == 6 && minor == 1) return "Windows 7 (or Windows 7 compatibility mode)";
	if (major == 6 && minor == 2) return "Windows 8 or newer (8.1, 10 and 11 report 6.2 to this client)";
	if (major == 6 && minor == 3) return "Windows 8.1";
	if (major == 10 && minor == 0) return "Windows 10 or 11";
	return "Unknown";
}

std::string ClientSysInfoView::MemoryBucket(const uint64_t totalKb) {
	if (totalKb == 0) return "Not read";
	constexpr uint64_t GB = 1024 * 1024;
	// Totals are a little under the installed amount (memory the firmware keeps), so each bucket ends just above it
	constexpr std::pair<uint64_t, const char*> BUCKETS[]{ { 2, "Under 2 GB" }, { 4, "2-4 GB" }, { 8, "4-8 GB" }, { 16, "8-16 GB" }, { 32, "16-32 GB" }, { 64, "32-64 GB" } };
	for (const auto& [limit, label] : BUCKETS) {
		if (totalKb <= limit * GB) return label;
	}
	return "Over 64 GB";
}

nlohmann::json ClientSysInfoView::RowJson(const IClientSysInfo::SysInfoRow& row, const bool showIp) {
	const auto memory = ClientSysInfo::ParseMemoryStats(row.memoryStats);
	nlohmann::json out = {
		{"id", row.id}, {"account_id", row.accountId}, {"first_seen", row.firstSeen}, {"last_seen", row.lastSeen}, {"logins", row.logins},
		{"client_os", row.clientOs}, {"client_os_name", ClientOsName(row.clientOs)},
		{"memory_stats", row.memoryStats},
		{"memory", {
			{"complete", memory.complete},
			{"working_set_bytes", Optional(memory.workingSetBytes)}, {"pagefile_usage_bytes", Optional(memory.pagefileUsageBytes)},
			{"memory_load_percent", Optional(memory.memoryLoadPercent)},
			{"total_phys_kb", Optional(memory.totalPhysKb)}, {"avail_phys_kb", Optional(memory.availPhysKb)},
			{"total_pagefile_kb", Optional(memory.totalPageFileKb)}, {"avail_pagefile_kb", Optional(memory.availPageFileKb)},
			{"total_virtual_kb", Optional(memory.totalVirtualKb)}, {"avail_virtual_kb", Optional(memory.availVirtualKb)},
			{"peak_working_set_bytes", Optional(memory.peakWorkingSetBytes)}, {"peak_pagefile_usage_bytes", Optional(memory.peakPagefileUsageBytes)},
		}},
		{"memory_total_kb", row.memoryTotalKb}, {"memory_bucket", MemoryBucket(row.memoryTotalKb)},
		{"video_card", row.videoCard},
		{"number_of_processors", row.numberOfProcessors}, {"processor_type", row.processorType},
		{"processor_level", row.processorLevel}, {"processor_revision", row.processorRevision},
		{"processor_model", row.processorRevision >> 8}, {"processor_stepping", row.processorRevision & 0xFF},
		{"os_version_info_size", row.osVersionInfoSize}, {"os_major_version", row.majorVersion}, {"os_minor_version", row.minorVersion},
		{"os_build_number", row.buildNumber}, {"os_platform_id", row.platformId},
		{"os_version", OsVersion(row)}, {"os_label", OsLabel(row)},
	};
	if (showIp) out["ip"] = row.ip;
	if (!row.accountName.empty()) out["account_name"] = row.accountName;
	return out;
}

nlohmann::json ClientSysInfoView::Spread(const std::vector<IClientSysInfo::SysInfoRow>& latest, const size_t top) {
	std::map<std::string, uint64_t> os, video, memory, processors, clientOs;
	for (const auto& row : latest) {
		os[OsVersion(row) + " - " + OsLabel(row)]++;
		video[Adapter(row.videoCard)]++;
		memory[MemoryBucket(row.memoryTotalKb)]++;
		processors[std::to_string(row.numberOfProcessors)]++;
		clientOs[ClientOsName(row.clientOs)]++;
	}
	// Memory buckets and processor counts read best in their own order
	auto ordered = [](const std::map<std::string, uint64_t>& counts, const std::vector<std::string>& order) {
		nlohmann::json out = nlohmann::json::array();
		for (const auto& label : order) {
			const auto it = counts.find(label);
			if (it != counts.end()) out.push_back({ {"label", label}, {"count", it->second} });
		}
		return out;
	};
	std::vector<std::string> cpuOrder;
	for (const auto& [label, _] : processors) cpuOrder.push_back(label);
	std::sort(cpuOrder.begin(), cpuOrder.end(), [](const std::string& a, const std::string& b) { return a.size() != b.size() ? a.size() < b.size() : a < b; });
	return {
		{"accounts", latest.size()},
		{"os", Ranked(os, top)},
		{"video", Ranked(video, top)},
		{"memory", ordered(memory, { "Under 2 GB", "2-4 GB", "4-8 GB", "8-16 GB", "16-32 GB", "32-64 GB", "Over 64 GB", "Not read" })},
		{"processors", ordered(processors, cpuOrder)},
		{"clientOs", Ranked(clientOs, top)},
	};
}
