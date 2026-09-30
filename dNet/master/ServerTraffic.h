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
#include "Profiler.h"
#include "TrafficStats.h"

/**
 * SERVER_TRAFFIC (any server -> master -> dashboard): what a server sent and received over the last few seconds, one
 * entry per second, plus the busiest message types, its HTTP routes (dashboard, UGC), RakNet's connection statistics
 * and a few gauges. Sent every REPORT_SECONDS; master sends its own straight to the dashboard.
 *
 * Newer servers append optional sections at the end, each after a marker byte, so readers that don't know them stop
 * before them and reports without them still read: PEER_SPLIT_MARKER, each second's packets by peer (clients, master,
 * other servers) and its HTTP requests from and to other servers (peerSplit); CONNECTIONS_MARKER, the busiest remote
 * ends with the rest summed (hasConnections); FRAMES_MARKER, the main loop's frame timing (see Profiler.h): per second
 * frames, frame times and time per phase, the packet types that took longest to handle, the worst frames and the slow
 * frames with their scopes (frames.present).
 */
struct ServerTraffic : public LUBitStream {
	ServerTraffic() : LUBitStream(ServiceType::MASTER, MessageType::Master::SERVER_TRAFFIC) {}

	static constexpr int64_t REPORT_SECONDS = 5;
	static constexpr uint16_t MAX_SECONDS = 180;
	static constexpr uint16_t MAX_MESSAGES = 128;
	static constexpr uint16_t MAX_ROUTES = 128;
	static constexpr uint16_t MAX_GAUGES = 32;
	static constexpr uint16_t MAX_TEXT = 200;
	static constexpr uint8_t PEER_SPLIT_MARKER = 1;
	static constexpr uint8_t HTTP_SPLIT_BIT = 0x80; // in a second's mask: the HTTP split follows the peers
	static constexpr uint8_t CONNECTIONS_MARKER = 2;
	static constexpr uint8_t MAX_CONNECTIONS = 64;
	static constexpr uint8_t FRAMES_MARKER = 3;
	static constexpr uint8_t MAX_FRAME_MESSAGES = 32;
	static constexpr uint8_t MAX_FRAMES = 16;   // worst or slow frames per report
	static constexpr uint16_t MAX_SCOPES = 256; // per frame

	ServiceType serverType{};
	uint32_t zoneId{};
	uint32_t instanceId{};
	TrafficStats::Report report;
	Profiler::Report frames;

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

		if (report.peerSplit) WritePeerSplit(stream, report.seconds.size() - seconds);
		if (report.hasConnections) WriteConnections(stream);
		if (frames.present) WriteFrames(stream, frames);
	}

	// Scopes in pre-order: name, argument, depth, count, total and first start
	static void WriteScopes(RakNet::BitStream& stream, const std::vector<Profiler::Node>& nodes, size_t max) {
		const auto count = std::min(nodes.size(), max);
		stream.Write(static_cast<uint32_t>(count));
		for (size_t i = 0; i < count; i++) {
			const auto& n = nodes[i];
			WriteText(stream, n.name);
			stream.Write(n.arg);
			stream.Write(n.depth);
			stream.Write(n.count);
			stream.Write(n.totalUs);
			stream.Write(n.startUs);
		}
	}

	static bool ReadScopes(RakNet::BitStream& stream, std::vector<Profiler::Node>& nodes, size_t max) {
		uint32_t count{};
		if (!stream.Read(count) || count > max) return false;
		nodes.resize(count);
		for (auto& n : nodes) {
			if (!ReadText(stream, n.name) || !stream.Read(n.arg) || !stream.Read(n.depth) || !stream.Read(n.count) || !stream.Read(n.totalUs) ||
				!stream.Read(n.startUs)) return false;
		}
		return true;
	}

	// Phase times with their count first, so a reader that knows fewer phases skips the rest and one that knows more
	// leaves them 0
	template<typename T>
	static void WritePhases(RakNet::BitStream& stream, const std::array<T, Profiler::PHASES>& phases) {
		stream.Write(static_cast<uint8_t>(Profiler::PHASES));
		for (const auto value : phases) stream.Write(Clamp(value));
	}

	template<typename T>
	static bool ReadPhases(RakNet::BitStream& stream, std::array<T, Profiler::PHASES>& phases) {
		uint8_t count{};
		if (!stream.Read(count)) return false;
		phases.fill(0);
		for (uint8_t i = 0; i < count; i++) {
			uint32_t value{};
			if (!stream.Read(value)) return false;
			if (i < Profiler::PHASES) phases[i] = value;
		}
		return true;
	}

	static void WriteFrame(RakNet::BitStream& stream, const Profiler::Frame& frame) {
		stream.Write(frame.timeMs);
		stream.Write(frame.durationUs);
		stream.Write(static_cast<uint8_t>(frame.implicit ? 1 : 0));
		WritePhases(stream, frame.phaseUs);
		WriteScopes(stream, frame.scopes, MAX_SCOPES);
	}

	static bool ReadFrame(RakNet::BitStream& stream, Profiler::Frame& frame) {
		uint8_t flags{};
		if (!stream.Read(frame.timeMs) || !stream.Read(frame.durationUs) || !stream.Read(flags) || !ReadPhases(stream, frame.phaseUs)) return false;
		frame.implicit = (flags & 1) != 0;
		return ReadScopes(stream, frame.scopes, MAX_SCOPES);
	}

	static void WriteFrames(RakNet::BitStream& stream, const Profiler::Report& frames) {
		stream.Write(FRAMES_MARKER);
		stream.Write(frames.slowThresholdMs);
		const auto seconds = std::min<size_t>(frames.seconds.size(), MAX_SECONDS);
		stream.Write(static_cast<uint16_t>(seconds));
		for (size_t i = frames.seconds.size() - seconds; i < frames.seconds.size(); i++) {
			const auto& s = frames.seconds[i];
			stream.Write(s.time);
			stream.Write(s.ticks);
			stream.Write(s.totalUs);
			stream.Write(s.maxUs);
			WriteHistogram(stream, s.frames);
			WritePhases(stream, s.phaseUs);
		}
		const auto messages = std::min<size_t>(frames.messages.size(), MAX_FRAME_MESSAGES);
		stream.Write(static_cast<uint8_t>(messages));
		for (size_t i = 0; i < messages; i++) {
			const auto& m = frames.messages[i];
			stream.Write(m.key);
			stream.Write(m.count);
			stream.Write(m.totalUs);
			stream.Write(m.maxUs);
		}
		for (const auto* list : { &frames.worst, &frames.slow }) {
			const auto count = std::min<size_t>(list->size(), MAX_FRAMES);
			stream.Write(static_cast<uint8_t>(count));
			for (size_t i = 0; i < count; i++) WriteFrame(stream, (*list)[i]);
		}
	}

	static bool ReadFrames(RakNet::BitStream& stream, Profiler::Report& frames) {
		uint16_t seconds{};
		if (!stream.Read(frames.slowThresholdMs) || !stream.Read(seconds) || seconds > MAX_SECONDS) return false;
		frames.seconds.resize(seconds);
		for (auto& s : frames.seconds) {
			if (!stream.Read(s.time) || !stream.Read(s.ticks) || !stream.Read(s.totalUs) || !stream.Read(s.maxUs) || !ReadHistogram(stream, s.frames) ||
				!ReadPhases(stream, s.phaseUs)) return false;
		}
		uint8_t count{};
		if (!stream.Read(count) || count > MAX_FRAME_MESSAGES) return false;
		frames.messages.resize(count);
		for (auto& m : frames.messages) {
			if (!stream.Read(m.key) || !stream.Read(m.count) || !stream.Read(m.totalUs) || !stream.Read(m.maxUs)) return false;
		}
		for (auto* list : { &frames.worst, &frames.slow }) {
			if (!stream.Read(count) || count > MAX_FRAMES) return false;
			list->resize(count);
			for (auto& frame : *list) if (!ReadFrame(stream, frame)) return false;
		}
		frames.present = true;
		return true;
	}

	static uint32_t Clamp(uint64_t value) { return static_cast<uint32_t>(std::min<uint64_t>(value, UINT32_MAX)); }

	// Per second (from `first`, the same ones as above): a bit per peer class with traffic, then its counts
	void WritePeerSplit(RakNet::BitStream& stream, size_t first) const {
		stream.Write(PEER_SPLIT_MARKER);
		for (size_t i = first; i < report.seconds.size(); i++) {
			const auto& second = report.seconds[i];
			const auto& peers = second.peers;
			uint8_t mask = 0;
			for (size_t p = 0; p < peers.size(); p++) if (!peers[p].Empty()) mask |= static_cast<uint8_t>(1u << p);
			const bool http = second.httpFromServers || second.httpOutRequests;
			if (http) mask |= HTTP_SPLIT_BIT;
			stream.Write(mask);
			for (size_t p = 0; p < peers.size(); p++) {
				if (!(mask & (1u << p))) continue;
				stream.Write(Clamp(peers[p].packetsIn));
				stream.Write(Clamp(peers[p].packetsOut));
				stream.Write(Clamp(peers[p].bytesIn));
				stream.Write(Clamp(peers[p].bytesOut));
			}
			if (http) {
				stream.Write(Clamp(second.httpFromServers));
				stream.Write(Clamp(second.httpFromServersBytesOut));
				stream.Write(Clamp(second.httpOutRequests));
				stream.Write(Clamp(second.httpOutBytesIn));
			}
		}
	}

	bool ReadPeerSplit(RakNet::BitStream& stream) {
		for (auto& s : report.seconds) {
			uint8_t mask{};
			if (!stream.Read(mask)) return false;
			for (size_t p = 0; p < s.peers.size(); p++) {
				if (!(mask & (1u << p))) continue;
				uint32_t pin{}, pout{}, bin{}, bout{};
				if (!stream.Read(pin) || !stream.Read(pout) || !stream.Read(bin) || !stream.Read(bout)) return false;
				s.peers[p] = { pin, pout, bin, bout };
			}
			if (mask & HTTP_SPLIT_BIT) {
				uint32_t from{}, fromBytes{}, out{}, outBytes{};
				if (!stream.Read(from) || !stream.Read(fromBytes) || !stream.Read(out) || !stream.Read(outBytes)) return false;
				s.httpFromServers = from;
				s.httpFromServersBytesOut = fromBytes;
				s.httpOutRequests = out;
				s.httpOutBytesIn = outBytes;
			}
		}
		report.peerSplit = true;
		return true;
	}

	static void WriteConnection(RakNet::BitStream& stream, const TrafficStats::Connection& c) {
		stream.Write(Clamp(c.packetsIn));
		stream.Write(Clamp(c.packetsOut));
		stream.Write(c.bytesIn);
		stream.Write(c.bytesOut);
		stream.Write(c.resends);
	}

	static bool ReadConnection(RakNet::BitStream& stream, TrafficStats::Connection& c) {
		uint32_t pin{}, pout{};
		if (!stream.Read(pin) || !stream.Read(pout) || !stream.Read(c.bytesIn) || !stream.Read(c.bytesOut) || !stream.Read(c.resends)) return false;
		c.packetsIn = pin;
		c.packetsOut = pout;
		return true;
	}

	void WriteConnections(RakNet::BitStream& stream) const {
		stream.Write(CONNECTIONS_MARKER);
		const auto count = std::min<size_t>(report.connections.size(), MAX_CONNECTIONS);
		stream.Write(static_cast<uint8_t>(count));
		for (size_t i = 0; i < count; i++) {
			const auto& c = report.connections[i];
			WriteText(stream, c.address);
			stream.Write(c.port);
			stream.Write(static_cast<uint8_t>(static_cast<uint8_t>(c.peer) | (c.http ? 0x80 : 0)));
			WriteConnection(stream, c);
			stream.Write(c.pingMs);
			stream.Write(c.accountId);
			stream.Write(c.characterId);
			WriteText(stream, c.account);
			WriteText(stream, c.character);
		}
		// The rest summed (those over MAX_CONNECTIONS too)
		auto others = report.otherConnections;
		uint32_t otherCount = report.otherConnectionCount;
		for (size_t i = count; i < report.connections.size(); i++, otherCount++) others.Merge(report.connections[i]);
		stream.Write(otherCount);
		WriteConnection(stream, others);
	}

	bool ReadConnections(RakNet::BitStream& stream) {
		uint8_t count{};
		if (!stream.Read(count) || count > MAX_CONNECTIONS) return false;
		report.connections.resize(count);
		for (auto& c : report.connections) {
			uint8_t flags{};
			if (!ReadText(stream, c.address) || !stream.Read(c.port) || !stream.Read(flags) || !ReadConnection(stream, c) || !stream.Read(c.pingMs) ||
				!stream.Read(c.accountId) || !stream.Read(c.characterId) || !ReadText(stream, c.account) || !ReadText(stream, c.character)) return false;
			c.peer = static_cast<TrafficStats::Peer>(std::min<uint8_t>(flags & 0x7F, TrafficStats::PEER_CLASSES - 1));
			c.http = (flags & 0x80) != 0;
		}
		if (!stream.Read(report.otherConnectionCount) || !ReadConnection(stream, report.otherConnections)) return false;
		report.hasConnections = true;
		return true;
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

		// Older servers stop here; newer ones add sections, each after its marker (a reader stops at one it doesn't know)
		report.peerSplit = false;
		report.hasConnections = false;
		frames = Profiler::Report{};
		uint8_t marker{};
		while (stream.GetNumberOfUnreadBits() >= 8 && stream.Read(marker)) {
			if (marker == PEER_SPLIT_MARKER && !report.peerSplit) {
				if (!ReadPeerSplit(stream)) return false;
			} else if (marker == CONNECTIONS_MARKER && !report.hasConnections) {
				if (!ReadConnections(stream)) return false;
			} else if (marker == FRAMES_MARKER && !frames.present) {
				if (!ReadFrames(stream, frames)) return false;
			} else {
				break;
			}
		}
		return true;
	}
};

#endif  //!__SERVERTRAFFIC__H__
