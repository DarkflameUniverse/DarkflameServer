#include "TrafficStats.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <tuple>

#include "MessageIdentifiers.h"
#include "ServiceType.h"
#include "MessageType/Client.h"
#include "MessageType/World.h"

namespace TrafficStats {
	namespace {
		constexpr double FIRST_BOUND = 100.0; // microseconds

		const std::array<uint64_t, Histogram::BUCKETS>& Bounds() {
			static const auto bounds = [] {
				std::array<uint64_t, Histogram::BUCKETS> out{};
				for (size_t i = 0; i + 1 < Histogram::BUCKETS; i++) out[i] = static_cast<uint64_t>(std::llround(FIRST_BOUND * std::exp2(static_cast<double>(i) / 3.0)));
				out[Histogram::BUCKETS - 1] = UINT64_MAX;
				return out;
			}();
			return bounds;
		}

		template<size_t N>
		void AddArrays(std::array<uint64_t, N>& to, const std::array<uint64_t, N>& from) {
			for (size_t i = 0; i < N; i++) to[i] += from[i];
		}
	}

	uint64_t Histogram::UpperBound(size_t bucket) {
		return Bounds()[std::min(bucket, BUCKETS - 1)];
	}

	size_t Histogram::BucketFor(uint64_t microseconds) {
		const auto& bounds = Bounds();
		return static_cast<size_t>(std::lower_bound(bounds.begin(), bounds.end(), microseconds) - bounds.begin());
	}

	void Histogram::Add(uint64_t microseconds, uint32_t count) {
		m_Counts[BucketFor(microseconds)] += count;
		m_Count += count;
		m_Sum += microseconds * count;
	}

	void Histogram::AddBucket(size_t bucket, uint32_t count) {
		if (bucket >= BUCKETS) return;
		m_Counts[bucket] += count;
		m_Count += count;
	}

	void Histogram::Merge(const Histogram& other) {
		for (size_t i = 0; i < BUCKETS; i++) m_Counts[i] += other.m_Counts[i];
		m_Count += other.m_Count;
		m_Sum += other.m_Sum;
	}

	uint64_t Histogram::Percentile(double fraction) const {
		if (m_Count == 0) return 0;
		fraction = std::clamp(fraction, 0.0, 1.0);
		// The rank of the value wanted, 1-based: the smallest value is rank 1
		const double rank = std::max(1.0, std::ceil(fraction * static_cast<double>(m_Count)));
		uint64_t seen = 0;
		for (size_t i = 0; i < BUCKETS; i++) {
			if (m_Counts[i] == 0) continue;
			if (static_cast<double>(seen + m_Counts[i]) >= rank) {
				const double lower = i == 0 ? 0.0 : static_cast<double>(UpperBound(i - 1));
				// The overflow bucket has no upper bound: report its lower one
				if (i == BUCKETS - 1) return static_cast<uint64_t>(lower);
				const double upper = static_cast<double>(UpperBound(i));
				const double within = (rank - static_cast<double>(seen)) / static_cast<double>(m_Counts[i]);
				return static_cast<uint64_t>(std::llround(lower + (upper - lower) * within));
			}
			seen += m_Counts[i];
		}
		return 0;
	}

	std::vector<std::pair<uint8_t, uint32_t>> Histogram::Sparse() const {
		std::vector<std::pair<uint8_t, uint32_t>> out;
		for (size_t i = 0; i < BUCKETS; i++) {
			if (m_Counts[i]) out.emplace_back(static_cast<uint8_t>(i), m_Counts[i]);
		}
		return out;
	}

	Histogram Histogram::FromSparse(const std::vector<std::pair<uint8_t, uint32_t>>& sparse, uint64_t sum) {
		Histogram out;
		for (const auto& [bucket, count] : sparse) out.AddBucket(bucket, count);
		out.m_Sum = sum;
		return out;
	}

	size_t StatusClass(uint16_t status) {
		if (status >= 100 && status < 500) return status / 100 - 1;
		return 4;
	}

	uint64_t MessageKey::Packed() const {
		return (static_cast<uint64_t>(outbound) << 63) | (static_cast<uint64_t>(service & 0x7FFF) << 48) |
			(static_cast<uint64_t>(packet) << 16) | gameMessage;
	}

	MessageKey MessageKey::Unpack(uint64_t packed) {
		MessageKey key;
		key.outbound = (packed >> 63) != 0;
		key.service = static_cast<uint16_t>((packed >> 48) & 0x7FFF);
		if (key.service == (RAKNET & 0x7FFF)) key.service = RAKNET;
		key.packet = static_cast<uint32_t>(packed >> 16);
		key.gameMessage = static_cast<uint16_t>(packed);
		return key;
	}

	MessageKey KeyOf(const uint8_t* data, size_t length, bool outbound) {
		MessageKey key;
		key.outbound = outbound;
		if (!data || length == 0) {
			key.service = MessageKey::RAKNET;
			return key;
		}
		// LU packets: ID_USER_PACKET_ENUM, uint16 service, uint32 packet ID, one padding byte
		if (data[0] != ID_USER_PACKET_ENUM || length < 8) {
			key.service = MessageKey::RAKNET;
			key.packet = data[0];
			return key;
		}
		key.service = static_cast<uint16_t>(data[1] | (data[2] << 8));
		key.packet = static_cast<uint32_t>(data[3]) | (static_cast<uint32_t>(data[4]) << 8) | (static_cast<uint32_t>(data[5]) << 16) | (static_cast<uint32_t>(data[6]) << 24);
		// Game messages: the header, the target object (8 bytes), then the uint16 game message ID
		const bool gameMessage = (key.service == static_cast<uint16_t>(ServiceType::WORLD) && key.packet == static_cast<uint32_t>(MessageType::World::GAME_MSG)) ||
			(key.service == static_cast<uint16_t>(ServiceType::CLIENT) && key.packet == static_cast<uint32_t>(MessageType::Client::GAME_MSG));
		if (gameMessage && length >= 18) key.gameMessage = static_cast<uint16_t>(data[16] | (data[17] << 8));
		return key;
	}

	void PeerCounts::Merge(const PeerCounts& other) {
		packetsIn += other.packetsIn;
		packetsOut += other.packetsOut;
		bytesIn += other.bytesIn;
		bytesOut += other.bytesOut;
	}

	void Second::Merge(const Second& other) {
		for (size_t i = 0; i < PEER_CLASSES; i++) peers[i].Merge(other.peers[i]);
		packetsIn += other.packetsIn;
		packetsOut += other.packetsOut;
		bytesIn += other.bytesIn;
		bytesOut += other.bytesOut;
		httpRequests += other.httpRequests;
		AddArrays(httpStatus, other.httpStatus);
		httpBytesOut += other.httpBytesOut;
		httpLatency.Merge(other.httpLatency);
		httpFromServers += other.httpFromServers;
		httpFromServersBytesOut += other.httpFromServersBytesOut;
		httpOutRequests += other.httpOutRequests;
		httpOutBytesIn += other.httpOutBytesIn;
	}

	void RouteStats::Merge(const RouteStats& other) {
		count += other.count;
		AddArrays(status, other.status);
		bytesOut += other.bytesOut;
		latency.Merge(other.latency);
	}

	void Recorder::Packet(int64_t now, const MessageKey& key, uint64_t bytes, uint32_t fanout, Peer peer) {
		if (fanout == 0) return;
		std::lock_guard lock(m_Mutex);
		auto& second = SecondAt(now);
		auto& side = second.peers[std::min<size_t>(static_cast<size_t>(peer), PEER_CLASSES - 1)];
		if (key.outbound) {
			second.packetsOut += fanout;
			second.bytesOut += bytes * fanout;
			side.packetsOut += fanout;
			side.bytesOut += bytes * fanout;
		} else {
			second.packetsIn += fanout;
			second.bytesIn += bytes * fanout;
			side.packetsIn += fanout;
			side.bytesIn += bytes * fanout;
		}
		auto& message = m_Messages[key.Packed()];
		message.key = key;
		message.count += fanout;
		message.bytes += bytes * fanout;
	}

	void Connection::Merge(const Connection& other) {
		packetsIn += other.packetsIn;
		packetsOut += other.packetsOut;
		bytesIn += other.bytesIn;
		bytesOut += other.bytesOut;
		resends += other.resends;
	}

	void TrimConnections(Report& report, size_t limit) {
		report.hasConnections = true;
		auto& list = report.connections;
		std::sort(list.begin(), list.end(), [](const Connection& a, const Connection& b) {
			return a.Bytes() != b.Bytes() ? a.Bytes() > b.Bytes() : std::tie(a.address, a.port) < std::tie(b.address, b.port);
		});
		for (size_t i = limit; i < list.size(); i++) {
			report.otherConnections.Merge(list[i]);
			report.otherConnectionCount++;
		}
		if (list.size() > limit) list.resize(limit);
	}

	void Recorder::HttpClient(const std::string& address, bool fromServer, uint64_t bytesIn, uint64_t bytesOut, uint32_t accountId, const std::string& user) {
		// One entry per address and signed-in account: people sharing an address (behind one NAT or proxy) stay apart
		const auto key = accountId ? address + '\n' + std::to_string(accountId) : address;
		std::lock_guard lock(m_Mutex);
		auto it = m_HttpClients.find(key);
		if (it == m_HttpClients.end()) {
			const bool full = m_HttpClients.size() >= MAX_HTTP_CLIENTS;
			it = m_HttpClients.try_emplace(full ? std::string() : key).first;
			it->second.address = full ? std::string() : address;
			it->second.http = true;
			if (!full) {
				it->second.accountId = accountId;
				it->second.account = user;
			}
		}
		auto& client = it->second;
		if (client.account.empty() && !user.empty() && client.accountId == accountId) client.account = user; // a WebSocket upgrade knows only the account
		if (fromServer) client.peer = Peer::SERVERS;
		client.packetsIn++;
		client.packetsOut++;
		client.bytesIn += bytesIn;
		client.bytesOut += bytesOut;
	}

	void Recorder::HttpOut(int64_t now, uint64_t bytesIn) {
		std::lock_guard lock(m_Mutex);
		auto& second = SecondAt(now);
		second.httpOutRequests++;
		second.httpOutBytesIn += bytesIn;
	}

	void Recorder::Http(int64_t now, const std::string& route, uint16_t status, uint64_t microseconds, uint64_t bytesOut, bool fromServer) {
		std::lock_guard lock(m_Mutex);
		auto& second = SecondAt(now);
		if (fromServer) {
			second.httpFromServers++;
			second.httpFromServersBytesOut += bytesOut;
		}
		second.httpRequests++;
		second.httpStatus[StatusClass(status)]++;
		second.httpBytesOut += bytesOut;
		second.httpLatency.Add(microseconds);

		auto it = m_Routes.find(route);
		if (it == m_Routes.end()) {
			const bool full = m_Routes.size() >= MAX_ROUTES;
			it = m_Routes.try_emplace(full ? std::string("other") : route).first;
			it->second.route = it->first;
		}
		it->second.count++;
		it->second.status[StatusClass(status)]++;
		it->second.bytesOut += bytesOut;
		it->second.latency.Add(microseconds);
	}

	Second& Recorder::SecondAt(int64_t now) {
		// Nearly every packet falls in the same second as the one before
		if (m_Current && m_CurrentTime == now) return *m_Current;
		auto& second = m_Seconds[now];
		second.time = now;
		m_Current = &second;
		m_CurrentTime = now;
		return second;
	}

	void Recorder::SetGauge(const std::string& name, std::function<double()> source) {
		std::lock_guard lock(m_Mutex);
		for (auto& gauge : m_Gauges) {
			if (gauge.first == name) {
				gauge.second = std::move(source);
				return;
			}
		}
		m_Gauges.emplace_back(name, std::move(source));
	}

	bool Recorder::Due(int64_t now, int64_t interval) {
		std::lock_guard lock(m_Mutex);
		if (m_LastTake == 0) {
			m_LastTake = now;
			return false;
		}
		return now - m_LastTake >= interval;
	}

	Report Recorder::Take(int64_t now) {
		Report report;
		report.peerSplit = true;
		std::vector<std::pair<std::string, std::function<double()>>> gauges;
		{
			std::lock_guard lock(m_Mutex);
			m_LastTake = now;
			// Fill every second from the last report to the one before now; after a long silence only the last MAX_GAP
			int64_t from = m_LastReported ? m_LastReported + 1 : (m_Seconds.empty() ? now : std::min(m_Seconds.begin()->first, now - 1));
			from = std::max(from, now - MAX_GAP);
			for (int64_t t = from; t < now; t++) {
				const auto it = m_Seconds.find(t);
				if (it != m_Seconds.end()) report.seconds.push_back(std::move(it->second));
				else report.seconds.push_back(Second{ .time = t });
			}
			m_Seconds.erase(m_Seconds.begin(), m_Seconds.lower_bound(now));
			m_Current = nullptr;
			if (now - 1 > m_LastReported) m_LastReported = now - 1;

			std::vector<MessageCount> messages;
			messages.reserve(m_Messages.size());
			for (auto& [_, count] : m_Messages) messages.push_back(count);
			m_Messages.clear();
			report.messages = Top(messages, TOP_MESSAGES);

			for (auto& [_, route] : m_Routes) report.routes.push_back(std::move(route));
			m_Routes.clear();
			for (auto& [address, client] : m_HttpClients) {
				if (address.empty()) { // the ones over MAX_HTTP_CLIENTS
					report.otherConnections.Merge(client);
					report.otherConnectionCount++;
				} else {
					report.connections.push_back(std::move(client));
				}
			}
			m_HttpClients.clear();
			gauges = m_Gauges;
		}
		for (const auto& [name, source] : gauges) report.gauges.emplace_back(name, source ? source() : 0.0);
		return report;
	}

	std::vector<MessageCount> Top(const std::vector<MessageCount>& counts, size_t limit) {
		std::vector<MessageCount> out;
		for (const bool outbound : { false, true }) {
			std::vector<MessageCount> direction;
			for (const auto& count : counts) if (count.key.outbound == outbound) direction.push_back(count);
			std::sort(direction.begin(), direction.end(), [](const MessageCount& a, const MessageCount& b) {
				return a.count != b.count ? a.count > b.count : a.key.Packed() < b.key.Packed();
			});
			if (direction.size() > limit) direction.resize(limit);
			out.insert(out.end(), direction.begin(), direction.end());
		}
		return out;
	}

	Recorder& Local() {
		static Recorder recorder;
		return recorder;
	}

	int64_t Now() {
		return static_cast<int64_t>(std::time(nullptr));
	}
}
