#ifndef __SERVERTRAFFIC__H__
#define __SERVERTRAFFIC__H__

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "BitStream.h"
#include "BitStreamUtils.h"
#include "MessageType/Master.h"
#include "ServiceType.h"
#include "TrafficStats.h"

/**
 * SERVER_TRAFFIC (any server -> master -> dashboard): what a server sent and received over the last few seconds, one
 * entry per second, plus the busiest message types, its HTTP routes (dashboard, UGC), RakNet's connection statistics
 * and a few gauges. Sent every REPORT_SECONDS; master sends its own straight to the dashboard.
 */
struct ServerTraffic : public LUBitStream {
	ServerTraffic() : LUBitStream(ServiceType::MASTER, MessageType::Master::SERVER_TRAFFIC) {}

	static constexpr int64_t REPORT_SECONDS = 5;
	static constexpr uint16_t MAX_SECONDS = 180;
	static constexpr uint16_t MAX_MESSAGES = 128;
	static constexpr uint16_t MAX_ROUTES = 128;
	static constexpr uint16_t MAX_GAUGES = 32;
	static constexpr uint16_t MAX_TEXT = 200;

	ServiceType serverType{};
	uint32_t zoneId{};
	uint32_t instanceId{};
	TrafficStats::Report report;

	static void WriteHistogram(RakNet::BitStream& stream, const TrafficStats::Histogram& histogram) {
		const auto sparse = histogram.Sparse();
		stream.Write(static_cast<uint8_t>(sparse.size())); // at most BUCKETS (58)
		for (const auto& [bucket, count] : sparse) {
			stream.Write(bucket);
			stream.Write(count);
		}
		stream.Write(histogram.Sum());
	}

	static bool ReadHistogram(RakNet::BitStream& stream, TrafficStats::Histogram& histogram) {
		uint8_t count{};
		if (!stream.Read(count) || count > TrafficStats::Histogram::BUCKETS) return false;
		std::vector<std::pair<uint8_t, uint32_t>> sparse(count);
		for (auto& [bucket, n] : sparse) {
			if (!stream.Read(bucket) || !stream.Read(n) || bucket >= TrafficStats::Histogram::BUCKETS) return false;
		}
		uint64_t sum{};
		if (!stream.Read(sum)) return false;
		histogram = TrafficStats::Histogram::FromSparse(sparse, sum);
		return true;
	}

	static void WriteText(RakNet::BitStream& stream, const std::string& text) {
		const auto length = static_cast<uint16_t>(std::min<size_t>(text.size(), MAX_TEXT));
		stream.Write(length);
		stream.Write(text.data(), length);
	}

	static bool ReadText(RakNet::BitStream& stream, std::string& text) {
		uint16_t length{};
		if (!stream.Read(length) || length > MAX_TEXT) return false;
		text.resize(length);
		return length == 0 || stream.Read(text.data(), length);
	}

	void Serialize(RakNet::BitStream& stream) const override {
		stream.Write(serverType);
		stream.Write(zoneId);
		stream.Write(instanceId);

		const auto seconds = std::min<size_t>(report.seconds.size(), MAX_SECONDS);
		stream.Write(static_cast<uint16_t>(seconds));
		// The newest seconds when there are too many
		for (size_t i = report.seconds.size() - seconds; i < report.seconds.size(); i++) {
			const auto& s = report.seconds[i];
			stream.Write(s.time);
			stream.Write(s.packetsIn);
			stream.Write(s.packetsOut);
			stream.Write(s.bytesIn);
			stream.Write(s.bytesOut);
			stream.Write(s.httpRequests);
			for (const auto status : s.httpStatus) stream.Write(status);
			stream.Write(s.httpBytesOut);
			WriteHistogram(stream, s.httpLatency);
		}

		const auto messages = std::min<size_t>(report.messages.size(), MAX_MESSAGES);
		stream.Write(static_cast<uint16_t>(messages));
		for (size_t i = 0; i < messages; i++) {
			const auto& m = report.messages[i];
			stream.Write(m.key.Packed());
			stream.Write(m.count);
			stream.Write(m.bytes);
		}

		const auto routes = std::min<size_t>(report.routes.size(), MAX_ROUTES);
		stream.Write(static_cast<uint16_t>(routes));
		for (size_t i = 0; i < routes; i++) {
			const auto& r = report.routes[i];
			WriteText(stream, r.route);
			stream.Write(r.count);
			for (const auto status : r.status) stream.Write(status);
			stream.Write(r.bytesOut);
			WriteHistogram(stream, r.latency);
		}

		const auto& l = report.link;
		stream.Write(l.connections);
		stream.Write(l.datagramsSent);
		stream.Write(l.datagramsReceived);
		stream.Write(l.bytesSent);
		stream.Write(l.bytesReceived);
		stream.Write(l.resends);
		stream.Write(l.resendQueue);
		stream.Write(l.averagePingMs);

		const auto gauges = std::min<size_t>(report.gauges.size(), MAX_GAUGES);
		stream.Write(static_cast<uint16_t>(gauges));
		for (size_t i = 0; i < gauges; i++) {
			WriteText(stream, report.gauges[i].first);
			stream.Write(report.gauges[i].second);
		}
	}

	bool Deserialize(RakNet::BitStream& stream) override {
		if (!stream.Read(serverType) || !stream.Read(zoneId) || !stream.Read(instanceId)) return false;

		uint16_t count{};
		if (!stream.Read(count) || count > MAX_SECONDS) return false;
		report.seconds.resize(count);
		for (auto& s : report.seconds) {
			if (!stream.Read(s.time) || !stream.Read(s.packetsIn) || !stream.Read(s.packetsOut) || !stream.Read(s.bytesIn) || !stream.Read(s.bytesOut) ||
				!stream.Read(s.httpRequests)) return false;
			for (auto& status : s.httpStatus) if (!stream.Read(status)) return false;
			if (!stream.Read(s.httpBytesOut) || !ReadHistogram(stream, s.httpLatency)) return false;
		}

		if (!stream.Read(count) || count > MAX_MESSAGES) return false;
		report.messages.resize(count);
		for (auto& m : report.messages) {
			uint64_t packed{};
			if (!stream.Read(packed) || !stream.Read(m.count) || !stream.Read(m.bytes)) return false;
			m.key = TrafficStats::MessageKey::Unpack(packed);
		}

		if (!stream.Read(count) || count > MAX_ROUTES) return false;
		report.routes.resize(count);
		for (auto& r : report.routes) {
			if (!ReadText(stream, r.route) || !stream.Read(r.count)) return false;
			for (auto& status : r.status) if (!stream.Read(status)) return false;
			if (!stream.Read(r.bytesOut) || !ReadHistogram(stream, r.latency)) return false;
		}

		auto& l = report.link;
		if (!stream.Read(l.connections) || !stream.Read(l.datagramsSent) || !stream.Read(l.datagramsReceived) || !stream.Read(l.bytesSent) ||
			!stream.Read(l.bytesReceived) || !stream.Read(l.resends) || !stream.Read(l.resendQueue) || !stream.Read(l.averagePingMs)) return false;

		if (!stream.Read(count) || count > MAX_GAUGES) return false;
		report.gauges.resize(count);
		for (auto& [name, value] : report.gauges) {
			if (!ReadText(stream, name) || !stream.Read(value)) return false;
		}
		return true;
	}
};

#endif  //!__SERVERTRAFFIC__H__
