#include "PerfHistory.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

#include "ServiceType.h"
#include "TrafficStats.h"

namespace {
	double Ms(uint64_t microseconds) {
		return std::round(static_cast<double>(microseconds) / 10.0) / 100.0;
	}

	double Round2(double value) {
		return std::round(value * 100.0) / 100.0;
	}

	std::string LabelOf(const Profiler::Node& node, const PerfHistory::Label& label) {
		return label ? label(node) : Profiler::DefaultLabel(node);
	}

	// The seconds of a server from `from` (inclusive) to `to` (exclusive)
	template<typename Fn>
	void ForSeconds(const PerfHistory::Server& server, int64_t from, int64_t to, Fn&& fn) {
		auto it = std::lower_bound(server.seconds.begin(), server.seconds.end(), from, [](const Profiler::Second& s, int64_t t) { return s.time < t; });
		for (; it != server.seconds.end() && it->time < to; ++it) fn(*it);
	}
}

bool PerfHistory::ParseKey(const std::string& key, uint16_t& serviceType, uint32_t& zoneId, uint32_t& instanceId) {
	zoneId = 0;
	instanceId = 0;
	if (key == "master") serviceType = static_cast<uint16_t>(ServiceType::MASTER);
	else if (key == "auth") serviceType = static_cast<uint16_t>(ServiceType::AUTH);
	else if (key == "chat") serviceType = static_cast<uint16_t>(ServiceType::CHAT);
	else if (key == "dashboard") serviceType = static_cast<uint16_t>(ServiceType::DASHBOARD);
	else if (key == "ugc") serviceType = static_cast<uint16_t>(ServiceType::UGC);
	else if (key.starts_with("world:")) {
		const auto colon = key.find(':', 6);
		if (colon == std::string::npos) return false;
		const auto zone = key.substr(6, colon - 6), instance = key.substr(colon + 1);
		const auto digits = [](const std::string& s) { return !s.empty() && s.size() <= 9 && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; }); };
		if (!digits(zone) || !digits(instance)) return false;
		serviceType = static_cast<uint16_t>(ServiceType::WORLD);
		zoneId = static_cast<uint32_t>(std::stoul(zone));
		instanceId = static_cast<uint32_t>(std::stoul(instance));
	} else {
		return false;
	}
	return true;
}

void PerfHistory::Ingest(const std::string& key, const Profiler::Report& report, int64_t now) {
	if (!report.present) return;
	auto [it, added] = m_Servers.try_emplace(key);
	auto& server = it->second;
	if (added) server.key = key;
	server.lastSeen = now;
	server.slowThresholdMs = report.slowThresholdMs;

	for (const auto& second : report.seconds) {
		if (server.seconds.empty() || server.seconds.back().time < second.time) {
			server.seconds.push_back(second);
			continue;
		}
		// Late or repeated: added to the one it belongs to
		auto at = std::lower_bound(server.seconds.begin(), server.seconds.end(), second.time, [](const Profiler::Second& s, int64_t t) { return s.time < t; });
		if (at != server.seconds.end() && at->time == second.time) at->Merge(second);
		else server.seconds.insert(at, second);
	}
	if (!report.messages.empty()) server.messages.emplace_back(now, report.messages);
	for (const auto& frame : report.worst) {
		server.worst.push_back(frame);
		if (server.worst.size() > WORST_KEPT) {
			// The shortest one goes
			const auto shortest = std::min_element(server.worst.begin(), server.worst.end(), [](const Profiler::Frame& a, const Profiler::Frame& b) { return a.durationUs < b.durationUs; });
			server.worst.erase(shortest);
		}
	}
	for (const auto& frame : report.slow) {
		m_Slow.push_back({ key, frame });
		if (m_Slow.size() > SLOW_KEPT) m_Slow.pop_front();
	}
}

void PerfHistory::Forget(int64_t now) {
	for (auto it = m_Servers.begin(); it != m_Servers.end();) {
		auto& server = it->second;
		if (now - server.lastSeen > FORGET_AFTER) {
			it = m_Servers.erase(it);
			continue;
		}
		while (!server.seconds.empty() && server.seconds.front().time < now - SECONDS_KEPT) server.seconds.pop_front();
		while (!server.messages.empty() && server.messages.front().first < now - RECENT_SECONDS) server.messages.pop_front();
		std::erase_if(server.worst, [now](const Profiler::Frame& frame) { return frame.timeMs / 1000 < now - RECENT_SECONDS; });
		++it;
	}
}

nlohmann::json PerfHistory::ServersJson(int64_t now, int64_t span, int64_t onlineSeconds, const ServerLabel& label) const {
	nlohmann::json out = nlohmann::json::array();
	for (const auto& [key, server] : m_Servers) {
		uint64_t ticks = 0, totalUs = 0;
		uint32_t maxUs = 0;
		int64_t covered = 0;
		TrafficStats::Histogram frames;
		ForSeconds(server, now - span, now, [&](const Profiler::Second& s) {
			covered++;
			ticks += s.ticks;
			totalUs += s.totalUs;
			maxUs = std::max(maxUs, s.maxUs);
			frames.Merge(s.frames);
		});
		size_t slow = 0;
		for (const auto& entry : m_Slow) if (entry.server == key && entry.frame.timeMs / 1000 >= now - span) slow++;
		nlohmann::json row = {
			{"key", key}, {"label", label ? label(key) : key}, {"online", now - server.lastSeen <= onlineSeconds}, {"last_seen", server.lastSeen},
			{"slow_threshold_ms", server.slowThresholdMs}, {"slow", slow}, {"seconds", covered},
			{"ticks_per_second", covered ? Round2(static_cast<double>(ticks) / static_cast<double>(covered)) : 0.0},
			{"avg_ms", ticks ? nlohmann::json(Ms(totalUs / ticks)) : nlohmann::json(nullptr)},
			{"p95_ms", ticks ? nlohmann::json(Ms(frames.Percentile(0.95))) : nlohmann::json(nullptr)},
			{"max_ms", ticks ? nlohmann::json(Ms(maxUs)) : nlohmann::json(nullptr)},
			{"busy_percent", covered ? Round2(static_cast<double>(totalUs) / (static_cast<double>(covered) * 1e4)) : 0.0},
		};
		out.push_back(std::move(row));
	}
	std::stable_sort(out.begin(), out.end(), [](const nlohmann::json& a, const nlohmann::json& b) { return a["busy_percent"].get<double>() > b["busy_percent"].get<double>(); });
	return out;
}

nlohmann::json PerfHistory::Series(const std::string& key, int64_t from, int64_t to, int64_t step) const {
	step = std::max<int64_t>(step, 1);
	const auto count = static_cast<size_t>(std::max<int64_t>((to - from) / step, 0));
	nlohmann::json times = nlohmann::json::array(), ticks = nlohmann::json::array(), avg = nlohmann::json::array(), p95 = nlohmann::json::array(),
		max = nlohmann::json::array();
	std::vector<nlohmann::json> phases(Profiler::PHASES, nlohmann::json::array());
	const auto it = m_Servers.find(key);
	for (size_t i = 0; i < count; i++) {
		const int64_t start = from + static_cast<int64_t>(i) * step;
		times.push_back(start);
		Profiler::Second sum;
		int64_t covered = 0;
		if (it != m_Servers.end()) {
			ForSeconds(it->second, start, start + step, [&](const Profiler::Second& s) {
				covered++;
				sum.Merge(s);
			});
		}
		// Not reported: gaps. Reported without frames (a stuck loop): no frames.
		if (!covered) {
			ticks.push_back(nullptr);
			avg.push_back(nullptr);
			p95.push_back(nullptr);
			max.push_back(nullptr);
			for (auto& phase : phases) phase.push_back(nullptr);
			continue;
		}
		ticks.push_back(Round2(static_cast<double>(sum.ticks) / static_cast<double>(covered)));
		avg.push_back(sum.ticks ? nlohmann::json(Ms(sum.totalUs / sum.ticks)) : nlohmann::json(nullptr));
		p95.push_back(sum.ticks ? nlohmann::json(Ms(sum.frames.Percentile(0.95))) : nlohmann::json(nullptr));
		max.push_back(sum.ticks ? nlohmann::json(Ms(sum.maxUs)) : nlohmann::json(nullptr));
		for (size_t p = 0; p < Profiler::PHASES; p++) phases[p].push_back(Ms(sum.phaseUs[p] / static_cast<uint64_t>(covered)));
	}
	nlohmann::json phaseJson = nlohmann::json::object();
	for (size_t p = 0; p < Profiler::PHASES; p++) phaseJson[Profiler::PhaseName(p)] = std::move(phases[p]);
	return { {"key", key}, {"from", from}, {"to", to}, {"step", step}, {"times", times}, {"ticks_per_second", ticks}, {"avg_ms", avg}, {"p95_ms", p95},
		{"max_ms", max}, {"phases_ms_per_second", phaseJson}, {"slow_threshold_ms", it != m_Servers.end() ? it->second.slowThresholdMs : 0} };
}

nlohmann::json PerfHistory::FrameJson(const Profiler::Frame& frame, const Label& label) {
	nlohmann::json phases = nlohmann::json::object();
	for (size_t p = 0; p < Profiler::PHASES; p++) {
		if (frame.phaseUs[p]) phases[Profiler::PhaseName(p)] = Ms(frame.phaseUs[p]);
	}
	nlohmann::json scopes = nlohmann::json::array();
	for (size_t i = 0; i < frame.scopes.size(); i++) {
		const auto& node = frame.scopes[i];
		uint64_t children = 0;
		for (size_t j = i + 1; j < frame.scopes.size() && frame.scopes[j].depth > node.depth; j++) {
			if (frame.scopes[j].depth == node.depth + 1) children += frame.scopes[j].totalUs;
		}
		scopes.push_back({ {"label", LabelOf(node, label)}, {"name", node.name}, {"arg", node.arg}, {"depth", node.depth}, {"count", node.count},
			{"total_ms", Ms(node.totalUs)}, {"self_ms", Ms(node.totalUs > children ? node.totalUs - children : 0)}, {"start_ms", Ms(node.startUs)} });
	}
	return { {"time_ms", frame.timeMs}, {"duration_ms", Ms(frame.durationUs)}, {"implicit", frame.implicit}, {"phases", phases},
		{"path", frame.Path([&label](const Profiler::Node& node) { return LabelOf(node, label); })}, {"scopes", scopes} };
}

nlohmann::json PerfHistory::Worst(const std::string& key, int64_t since, size_t limit, const Label& label) const {
	nlohmann::json out = nlohmann::json::array();
	const auto it = m_Servers.find(key);
	if (it == m_Servers.end()) return out;
	std::vector<const Profiler::Frame*> frames;
	for (const auto& frame : it->second.worst) if (frame.timeMs / 1000 >= since) frames.push_back(&frame);
	std::stable_sort(frames.begin(), frames.end(), [](const Profiler::Frame* a, const Profiler::Frame* b) { return a->durationUs > b->durationUs; });
	if (frames.size() > limit) frames.resize(limit);
	for (const auto* frame : frames) out.push_back(FrameJson(*frame, label));
	return out;
}

nlohmann::json PerfHistory::Messages(const std::string& key, int64_t since, size_t limit, const MessageNames& names) const {
	nlohmann::json out = nlohmann::json::array();
	const auto it = m_Servers.find(key);
	if (it == m_Servers.end()) return out;
	std::unordered_map<uint64_t, Profiler::MessageTime> merged;
	for (const auto& [time, messages] : it->second.messages) {
		if (time < since) continue;
		for (const auto& m : messages) {
			auto& sum = merged[m.key];
			sum.key = m.key;
			sum.count += m.count;
			sum.totalUs += m.totalUs;
			sum.maxUs = std::max(sum.maxUs, m.maxUs);
		}
	}
	std::vector<Profiler::MessageTime> list;
	for (const auto& [_, m] : merged) list.push_back(m);
	std::sort(list.begin(), list.end(), [](const Profiler::MessageTime& a, const Profiler::MessageTime& b) { return a.totalUs != b.totalUs ? a.totalUs > b.totalUs : a.key < b.key; });
	if (list.size() > limit) list.resize(limit);
	for (const auto& m : list) {
		nlohmann::json row = names ? names(m.key) : nlohmann::json::object();
		row["count"] = m.count;
		row["total_ms"] = Ms(m.totalUs);
		row["avg_ms"] = m.count ? Ms(m.totalUs / m.count) : 0.0;
		row["max_ms"] = Ms(m.maxUs);
		out.push_back(std::move(row));
	}
	return out;
}

nlohmann::json PerfHistory::SlowJson(const std::string& key, const Label& label, const ServerLabel& serverLabel) const {
	nlohmann::json out = nlohmann::json::array();
	for (auto it = m_Slow.rbegin(); it != m_Slow.rend(); ++it) {
		if (!key.empty() && it->server != key) continue;
		auto frame = FrameJson(it->frame, label);
		frame["server"] = it->server;
		frame["server_label"] = serverLabel ? serverLabel(it->server) : it->server;
		out.push_back(std::move(frame));
	}
	return out;
}

nlohmann::json PerfHistory::ProfileJson(const Profiler::Profile& profile, const Label& label) {
	nlohmann::json nodes = nlohmann::json::array();
	for (size_t i = 0; i < profile.nodes.size(); i++) {
		const auto& node = profile.nodes[i];
		uint64_t children = 0;
		for (size_t j = i + 1; j < profile.nodes.size() && profile.nodes[j].depth > node.depth; j++) {
			if (profile.nodes[j].depth == node.depth + 1) children += profile.nodes[j].totalUs;
		}
		nodes.push_back({ {"label", LabelOf(node, label)}, {"depth", node.depth}, {"count", node.count}, {"total_us", node.totalUs},
			{"self_us", node.totalUs > children ? node.totalUs - children : 0} });
	}
	return { {"duration_ms", profile.durationMs}, {"frames", profile.frames}, {"total_ms", Ms(profile.totalUs)}, {"truncated", profile.truncated},
		{"nodes", nodes}, {"folded", Profiler::Folded(profile.nodes, [&label](const Profiler::Node& node) { return LabelOf(node, label); })} };
}
