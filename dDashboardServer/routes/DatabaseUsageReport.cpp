#include "DatabaseUsageReport.h"

#include <algorithm>
#include <map>

namespace DatabaseUsageReport {
	bool IsLocalHost(const std::string& mysqlHost) {
		std::string host = mysqlHost;
		std::ranges::transform(host, host.begin(), ::tolower);
		if (host.starts_with("unix://") || host.starts_with("pipe://")) return true;
		if (host.starts_with("tcp://")) host = host.substr(6);
		if (host.starts_with("[")) host = host.substr(1, host.find(']') == std::string::npos ? std::string::npos : host.find(']') - 1);
		else if (std::ranges::count(host, ':') == 1) host = host.substr(0, host.find(':')); // host:port (an IPv6 address has more colons)
		return host.empty() || host == "localhost" || host == "::1" || host.starts_with("127.");
	}

	Disk DiskState(uint64_t free, uint64_t total, double warnPercent) {
		Disk disk;
		if (total == 0) return disk;
		disk.known = true;
		disk.free = std::min(free, total);
		disk.total = total;
		disk.freePercent = 100.0 * static_cast<double>(disk.free) / static_cast<double>(total);
		disk.low = warnPercent > 0 && disk.freePercent < warnPercent;
		return disk;
	}

	nlohmann::json TotalsJson(const IDatabaseUsage::UsageTotals& totals, double warnPercent) {
		const auto disk = DiskState(totals.diskFree, totals.diskTotal, warnPercent);
		return {
			{"time", totals.time}, {"total_bytes", totals.totalBytes}, {"free_bytes", totals.freeBytes}, {"data_bytes", totals.dataBytes},
			{"index_bytes", totals.indexBytes}, {"rows", totals.rows},
			{"disk", { {"known", disk.known}, {"free", disk.free}, {"total", disk.total}, {"free_percent", disk.freePercent}, {"low", disk.low} }},
		};
	}

	nlohmann::json Series(const std::vector<IDatabaseUsage::UsageTotals>& samples, size_t maxPoints) {
		nlohmann::json out = { {"times", nlohmann::json::array()}, {"total", nlohmann::json::array()}, {"data", nlohmann::json::array()},
			{"index", nlohmann::json::array()}, {"free", nlohmann::json::array()}, {"rows", nlohmann::json::array()}, {"disk_free", nlohmann::json::array()} };
		if (samples.empty() || maxPoints == 0) return out;
		const auto add = [&out](const IDatabaseUsage::UsageTotals& s) {
			out["times"].push_back(s.time);
			out["total"].push_back(s.totalBytes);
			out["data"].push_back(s.dataBytes);
			out["index"].push_back(s.indexBytes);
			out["free"].push_back(s.freeBytes);
			out["rows"].push_back(s.rows);
			out["disk_free"].push_back(s.diskTotal ? nlohmann::json(s.diskFree) : nlohmann::json(nullptr));
		};
		if (samples.size() <= maxPoints) {
			for (const auto& s : samples) add(s);
			return out;
		}
		const auto from = samples.front().time, to = samples.back().time;
		const auto span = std::max<int64_t>(to - from, 1);
		for (size_t i = 0; i < samples.size(); i++) {
			// The last sample of each slice (always the newest one)
			const auto here = std::min<size_t>(static_cast<size_t>((samples[i].time - from) * static_cast<int64_t>(maxPoints) / (span + 1)), maxPoints - 1);
			const bool last = i + 1 == samples.size();
			const auto next = last ? maxPoints : std::min<size_t>(static_cast<size_t>((samples[i + 1].time - from) * static_cast<int64_t>(maxPoints) / (span + 1)), maxPoints - 1);
			if (last || next != here) add(samples[i]);
		}
		return out;
	}

	nlohmann::json Tables(const std::vector<IDatabaseUsage::TableUsage>& now, const std::vector<IDatabaseUsage::TableUsage>& before, int64_t seconds) {
		std::map<std::string, const IDatabaseUsage::TableUsage*> then;
		for (const auto& table : before) then[table.name] = &table;
		const double days = seconds > 0 ? static_cast<double>(seconds) / 86400.0 : 0.0;
		auto out = nlohmann::json::array();
		for (const auto& table : now) {
			const auto it = then.find(table.name);
			const auto total = table.dataBytes + table.indexBytes;
			const auto oldTotal = it == then.end() ? 0 : it->second->dataBytes + it->second->indexBytes;
			const auto oldRows = it == then.end() ? 0 : it->second->rows;
			const auto growth = static_cast<int64_t>(total) - static_cast<int64_t>(oldTotal);
			out.push_back({ {"name", table.name}, {"data_bytes", table.dataBytes}, {"index_bytes", table.indexBytes}, {"total_bytes", total},
				{"rows", table.rows}, {"growth_bytes", growth}, {"growth_rows", static_cast<int64_t>(table.rows) - static_cast<int64_t>(oldRows)},
				{"growth_per_day", days > 0 ? static_cast<double>(growth) / days : 0.0} });
		}
		std::stable_sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a["total_bytes"].template get<uint64_t>() > b["total_bytes"].template get<uint64_t>(); });
		return out;
	}

	nlohmann::json Fastest(const nlohmann::json& tables, size_t limit) {
		auto out = nlohmann::json::array();
		for (const auto& table : tables) if (table.value("growth_bytes", int64_t{}) > 0) out.push_back(table);
		std::stable_sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a["growth_bytes"].template get<int64_t>() > b["growth_bytes"].template get<int64_t>(); });
		if (out.size() > limit) out.erase(out.begin() + static_cast<std::ptrdiff_t>(limit), out.end());
		return out;
	}
}
