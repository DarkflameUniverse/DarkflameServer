#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Profiler.h"
#include "json.hpp"

/**
 * Every server's frame timing (the frames section of its traffic reports, see Profiler.h), kept by the dashboard in
 * memory: the last hour at one second, the worst frames and packet handling times of the last minutes, and the last
 * slow frames of all servers. Pure (no database, network or clock), so it is unit tested; Performance.cpp feeds it.
 */
class PerfHistory {
public:
	static constexpr int64_t SECONDS_KEPT = 3600;
	static constexpr int64_t RECENT_SECONDS = 600;  // worst frames and packet times kept
	static constexpr size_t WORST_KEPT = 40;        // per server
	static constexpr size_t SLOW_KEPT = 50;         // all servers together
	static constexpr int64_t FORGET_AFTER = 86400;  // a server silent this long is dropped

	struct Server {
		std::string key;
		int64_t lastSeen{};
		uint32_t slowThresholdMs{};
		std::deque<Profiler::Second> seconds;                                    // oldest first
		std::deque<std::pair<int64_t, std::vector<Profiler::MessageTime>>> messages; // per report
		std::deque<Profiler::Frame> worst;                                       // oldest first
	};

	struct SlowFrame {
		std::string server;
		Profiler::Frame frame;
	};

	// Names a scope ("Packet GAME_MSG RequestUse", "Component INVENTORY"); DefaultLabel when not set
	using Label = std::function<std::string(const Profiler::Node&)>;
	// Names a packet type: {service, packet, game_message}
	using MessageNames = std::function<nlohmann::json(uint64_t key)>;
	// The label of a server key ("World 1200 Nimbus Station #3")
	using ServerLabel = std::function<std::string(const std::string& key)>;

	void Ingest(const std::string& key, const Profiler::Report& report, int64_t now);
	void Forget(int64_t now);

	const std::map<std::string, Server>& Servers() const { return m_Servers; }
	const std::deque<SlowFrame>& Slow() const { return m_Slow; }

	// Each server over the last `span` seconds: frames per second, average, p95 and longest frame, how busy its main
	// loop was, its slow frames; busiest first
	nlohmann::json ServersJson(int64_t now, int64_t span, int64_t onlineSeconds, const ServerLabel& label) const;

	// One server's seconds from `from` to `to` in steps of `step`: frames per second, average / p95 / longest frame
	// (ms), and each phase's milliseconds per second
	nlohmann::json Series(const std::string& key, int64_t from, int64_t to, int64_t step) const;

	// One server's longest frames since `since`, longest first
	nlohmann::json Worst(const std::string& key, int64_t since, size_t limit, const Label& label) const;

	// One server's packet types by handling time since `since`, longest total first
	nlohmann::json Messages(const std::string& key, int64_t since, size_t limit, const MessageNames& names) const;

	// The slow frames of every server (or one), newest first
	nlohmann::json SlowJson(const std::string& key, const Label& label, const ServerLabel& serverLabel) const;

	static nlohmann::json FrameJson(const Profiler::Frame& frame, const Label& label);
	// A profiling session's tree for the flame graph, and its folded stacks
	static nlohmann::json ProfileJson(const Profiler::Profile& profile, const Label& label);

	// "world:1200:3" -> (WORLD, 1200, 3); false for keys that aren't a server's
	static bool ParseKey(const std::string& key, uint16_t& serviceType, uint32_t& zoneId, uint32_t& instanceId);

private:
	std::map<std::string, Server> m_Servers;
	std::deque<SlowFrame> m_Slow; // oldest first
};
