#include "TrafficHistory.h"

#include <algorithm>

#include "ServiceType.h"

namespace {
	using Sparse = std::vector<std::pair<uint8_t, uint32_t>>;

	void MergeSparse(Sparse& into, const Sparse& from) {
		if (from.empty()) return;
		if (into.empty()) {
			into = from;
			return;
		}
		auto histogram = TrafficStats::Histogram::FromSparse(into, 0);
		histogram.Merge(TrafficStats::Histogram::FromSparse(from, 0));
		into = histogram.Sparse();
	}

	int64_t Floor(int64_t time, int64_t step) {
		return time - ((time % step) + step) % step;
	}

	template<typename Counts>
	void AddCounts(TrafficHistory::MessageCounts& into, const Counts& from, size_t cap = SIZE_MAX) {
		for (const auto& count : from) {
			const auto packed = count.key.Packed();
			auto it = into.find(packed);
			if (it == into.end()) {
				if (into.size() >= cap) continue;
				it = into.emplace(packed, TrafficStats::MessageCount{ count.key }).first;
			}
			it->second.count += count.count;
			it->second.bytes += count.bytes;
		}
	}

	void AddRoutes(std::map<std::string, TrafficStats::RouteStats>& into, const std::vector<TrafficStats::RouteStats>& from, size_t cap) {
		for (const auto& route : from) {
			auto it = into.find(route.route);
			if (it == into.end()) {
				if (into.size() >= cap) continue;
				it = into.emplace(route.route, TrafficStats::RouteStats{ .route = route.route }).first;
			}
			it->second.Merge(route);
		}
	}

	// The entry of `time` at the back of a time-ordered deque, added when it isn't there yet
	template<typename Value>
	Value& Slot(std::deque<std::pair<int64_t, Value>>& slots, int64_t time, size_t keep) {
		if (slots.empty() || slots.back().first < time) {
			slots.emplace_back(time, Value{});
			while (slots.size() > keep) slots.pop_front();
			return slots.back().second;
		}
		for (auto it = slots.rbegin(); it != slots.rend(); ++it) {
			if (it->first == time) return it->second;
		}
		return slots.back().second; // older than everything kept: count it with the newest
	}
}

void TrafficHistory::Point::Add(const TrafficStats::Second& second) {
	packetsIn += second.packetsIn;
	packetsOut += second.packetsOut;
	bytesIn += second.bytesIn;
	bytesOut += second.bytesOut;
	httpRequests += second.httpRequests;
	http4xx += second.httpStatus[3];
	http5xx += second.httpStatus[4];
	httpBytesOut += second.httpBytesOut;
	if (!second.httpLatency.Empty()) {
		MergeSparse(latency, second.httpLatency.Sparse());
		latencySum += second.httpLatency.Sum();
	}
}

void TrafficHistory::Point::Add(const Point& other) {
	packetsIn += other.packetsIn;
	packetsOut += other.packetsOut;
	bytesIn += other.bytesIn;
	bytesOut += other.bytesOut;
	httpRequests += other.httpRequests;
	http4xx += other.http4xx;
	http5xx += other.http5xx;
	httpBytesOut += other.httpBytesOut;
	MergeSparse(latency, other.latency);
	latencySum += other.latencySum;
}

TrafficStats::Histogram TrafficHistory::Point::Latency() const {
	return TrafficStats::Histogram::FromSparse(latency, latencySum);
}

std::string TrafficHistory::KeyFor(uint16_t serviceType, uint32_t zoneId, uint32_t instanceId) {
	switch (static_cast<ServiceType>(serviceType)) {
	case ServiceType::MASTER: return "master";
	case ServiceType::AUTH: return "auth";
	case ServiceType::CHAT: return "chat";
	case ServiceType::DASHBOARD: return "dashboard";
	case ServiceType::UGC: return "ugc";
	case ServiceType::WORLD: return "world:" + std::to_string(zoneId) + ":" + std::to_string(instanceId);
	default: return "service:" + std::to_string(serviceType) + ":" + std::to_string(zoneId) + ":" + std::to_string(instanceId);
	}
}

void TrafficHistory::Ingest(uint16_t serviceType, uint32_t zoneId, uint32_t instanceId, const TrafficStats::Report& report, int64_t now) {
	const auto key = KeyFor(serviceType, zoneId, instanceId);
	auto [it, added] = m_Servers.try_emplace(key);
	auto& server = it->second;
	if (added) {
		server.key = key;
		server.type = serviceType;
		server.zoneId = zoneId;
		server.instanceId = instanceId;
		server.firstSeen = now;
		// Minutes before the first report were never seen, so nothing before it is written
		server.writtenUntil = Floor(now, 60) - 60;
	}
	server.lastSeen = now;
	server.link = report.link;
	server.gauges = report.gauges;

	auto& totals = server.totals;
	int64_t lastSecond = now;
	for (const auto& second : report.seconds) {
		// A server whose clock is far off is still shown, at the dashboard's time
		if (second.time < now - SECONDS_KEPT || second.time > now + 60) continue;
		lastSecond = second.time;
		totals.packetsIn += second.packetsIn;
		totals.packetsOut += second.packetsOut;
		totals.bytesIn += second.bytesIn;
		totals.bytesOut += second.bytesOut;

		if (server.seconds.empty() || server.seconds.back().time < second.time) {
			server.seconds.push_back(Point{ .time = second.time });
			server.seconds.back().Add(second);
		} else {
			const auto at = std::lower_bound(server.seconds.begin(), server.seconds.end(), second.time, [](const Point& p, int64_t t) { return p.time < t; });
			if (at != server.seconds.end() && at->time == second.time) at->Add(second);
			else server.seconds.insert(at, Point{ .time = second.time })->Add(second);
		}

		const auto minute = Floor(second.time, 60);
		if (minute < server.writtenUntil) continue; // that minute is in the database already
		auto slot = std::find_if(server.minutes.rbegin(), server.minutes.rend(), [minute](const Minute& m) { return m.point.time <= minute; });
		if (slot == server.minutes.rend() || slot->point.time != minute) {
			const auto at = server.minutes.insert(slot.base(), Minute{ .point = Point{ .time = minute } });
			at->point.Add(second);
		} else {
			slot->point.Add(second);
		}
	}
	while (!server.seconds.empty() && server.seconds.front().time < now - SECONDS_KEPT) server.seconds.pop_front();

	const auto& link = report.link;
	totals.datagramsSent += link.datagramsSent;
	totals.datagramsReceived += link.datagramsReceived;
	totals.linkBytesSent += link.bytesSent;
	totals.linkBytesReceived += link.bytesReceived;
	totals.resends += link.resends;
	if (link.resends && !server.minutes.empty()) {
		const auto minute = Floor(lastSecond, 60);
		auto slot = std::find_if(server.minutes.rbegin(), server.minutes.rend(), [minute](const Minute& m) { return m.point.time == minute; });
		(slot != server.minutes.rend() ? *slot : server.minutes.back()).resends += link.resends;
	}

	AddCounts(totals.messages, report.messages, MAX_TOTAL_MESSAGES);
	AddCounts(Slot(server.messageMinutes, Floor(now, 60), MESSAGE_MINUTES), report.messages);
	AddCounts(Slot(server.messageHours, Floor(now, 3600), MESSAGE_HOURS), report.messages);

	AddRoutes(totals.routes, report.routes, MAX_TOTAL_ROUTES);
	if (!report.routes.empty()) AddRoutes(Slot(server.routeMinutes, Floor(now, 60), ROUTE_MINUTES), report.routes, MAX_TOTAL_ROUTES);
}

std::vector<IServerTraffic::TrafficMinute> TrafficHistory::TakeFinishedMinutes(int64_t now) {
	std::vector<IServerTraffic::TrafficMinute> rows;
	for (auto& [key, server] : m_Servers) {
		while (!server.minutes.empty() && server.minutes.front().point.time + 60 + MINUTE_SLACK <= now) {
			const auto& minute = server.minutes.front();
			const auto& p = minute.point;
			const auto latency = p.Latency();
			rows.push_back({ .time = p.time, .server = key, .packetsIn = p.packetsIn, .packetsOut = p.packetsOut, .bytesIn = p.bytesIn, .bytesOut = p.bytesOut,
				.resends = minute.resends, .httpRequests = p.httpRequests, .http4xx = p.http4xx, .http5xx = p.http5xx, .httpBytesOut = p.httpBytesOut,
				.latencyP50Us = static_cast<uint32_t>(std::min<uint64_t>(latency.Percentile(0.50), UINT32_MAX)),
				.latencyP95Us = static_cast<uint32_t>(std::min<uint64_t>(latency.Percentile(0.95), UINT32_MAX)),
				.latencyP99Us = static_cast<uint32_t>(std::min<uint64_t>(latency.Percentile(0.99), UINT32_MAX)) });
			server.writtenUntil = std::max(server.writtenUntil, p.time + 60);
			server.minutes.pop_front();
		}
	}
	return rows;
}

void TrafficHistory::Forget(int64_t now) {
	for (auto it = m_Servers.begin(); it != m_Servers.end();) {
		auto& seconds = it->second.seconds;
		while (!seconds.empty() && seconds.front().time < now - SECONDS_KEPT) seconds.pop_front();
		if (it->second.lastSeen < now - FORGET_AFTER && it->second.minutes.empty()) it = m_Servers.erase(it);
		else ++it;
	}
}

std::map<std::string, std::vector<TrafficHistory::Point>> TrafficHistory::Buckets(int64_t from, int64_t to, int64_t step) const {
	std::map<std::string, std::vector<Point>> out;
	if (step <= 0 || to <= from) return out;
	const auto count = static_cast<size_t>((to - from + step - 1) / step);
	for (const auto& [key, server] : m_Servers) {
		auto it = std::lower_bound(server.seconds.begin(), server.seconds.end(), from, [](const Point& p, int64_t t) { return p.time < t; });
		if (it == server.seconds.end() || it->time >= to) continue;
		auto& points = out[key];
		points.resize(count);
		for (size_t i = 0; i < count; i++) points[i].time = from + static_cast<int64_t>(i) * step;
		for (; it != server.seconds.end() && it->time < to; ++it) points[static_cast<size_t>((it->time - from) / step)].Add(*it);
	}
	return out;
}

std::vector<TrafficStats::MessageCount> TrafficHistory::TopMessages(const std::string& serverKey, int64_t since, size_t limit) const {
	MessageCounts counts;
	for (const auto& [key, server] : m_Servers) {
		if (!serverKey.empty() && key != serverKey) continue;
		const bool minutes = !server.messageMinutes.empty() && server.messageMinutes.front().first <= Floor(since, 60);
		const auto& slots = minutes || server.messageHours.empty() ? server.messageMinutes : server.messageHours;
		const auto start = minutes ? Floor(since, 60) : Floor(since, 3600);
		for (const auto& [time, slot] : slots) {
			if (time < start) continue;
			for (const auto& [packed, count] : slot) {
				auto& total = counts[packed];
				total.key = count.key;
				total.count += count.count;
				total.bytes += count.bytes;
			}
		}
	}
	std::vector<TrafficStats::MessageCount> list;
	list.reserve(counts.size());
	for (const auto& [_, count] : counts) list.push_back(count);
	return TrafficStats::Top(list, limit);
}

std::vector<std::pair<std::string, TrafficStats::RouteStats>> TrafficHistory::Routes(int64_t since) const {
	std::vector<std::pair<std::string, TrafficStats::RouteStats>> out;
	for (const auto& [key, server] : m_Servers) {
		std::map<std::string, TrafficStats::RouteStats> routes;
		for (const auto& [time, slot] : server.routeMinutes) {
			if (time < Floor(since, 60)) continue;
			for (const auto& [name, stats] : slot) {
				auto& total = routes[name];
				total.route = name;
				total.Merge(stats);
			}
		}
		for (auto& [_, stats] : routes) out.emplace_back(key, std::move(stats));
	}
	std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.second.count != b.second.count ? a.second.count > b.second.count : a.second.route < b.second.route; });
	return out;
}
