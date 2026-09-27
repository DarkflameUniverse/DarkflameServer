#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * Traffic diagnostics every server keeps about itself: packets and bytes in and out per second, the packet and game
 * message types they were, and (servers with a web server) HTTP requests with their latency. Counting is a few
 * additions under an uncontended lock; every few seconds dServer takes a Report and ships it to the dashboard through
 * master (see docs/Dashboard.md, "Traffic diagnostics").
 *
 * Seconds are Unix seconds, so reports from different servers line up.
 */
namespace TrafficStats {
	/**
	 * Latency histogram with fixed, geometric buckets (three per doubling, from 100 microseconds), so histograms from
	 * different seconds and servers add up and percentiles of a minute or an hour come out right.
	 */
	class Histogram {
	public:
		static constexpr size_t BUCKETS = 58; // the last one holds everything over ~41 s

		// Largest value (microseconds) bucket i holds; the last bucket has no limit (UINT64_MAX)
		static uint64_t UpperBound(size_t bucket);
		static size_t BucketFor(uint64_t microseconds);

		void Add(uint64_t microseconds, uint32_t count = 1);
		void Merge(const Histogram& other);
		void AddBucket(size_t bucket, uint32_t count);

		uint64_t Count() const { return m_Count; }
		uint64_t Sum() const { return m_Sum; } // microseconds, all values together
		void SetSum(uint64_t sum) { m_Sum = sum; }
		bool Empty() const { return m_Count == 0; }
		uint32_t At(size_t bucket) const { return m_Counts[bucket]; }

		// The value below which `fraction` (0..1) of the values are, interpolated within its bucket; 0 when empty
		uint64_t Percentile(double fraction) const;

		// Non-empty buckets as (bucket, count), for the wire and for keeping many histograms small
		std::vector<std::pair<uint8_t, uint32_t>> Sparse() const;
		static Histogram FromSparse(const std::vector<std::pair<uint8_t, uint32_t>>& sparse, uint64_t sum);

	private:
		std::array<uint32_t, BUCKETS> m_Counts{};
		uint64_t m_Count{};
		uint64_t m_Sum{};
	};

	// HTTP status classes 1xx..5xx as index 0..4 (anything else counts as 5xx)
	size_t StatusClass(uint16_t status);

	// What a packet was: its LU service and packet ID, and for game messages the game message ID
	struct MessageKey {
		bool outbound{};
		uint16_t service{};  // ServiceType; RAKNET for RakNet's own messages (no LU header)
		uint32_t packet{};   // LU packet ID, or the RakNet message ID
		uint16_t gameMessage{}; // game messages only

		static constexpr uint16_t RAKNET = 0xFFFF;

		uint64_t Packed() const;
		static MessageKey Unpack(uint64_t packed);
		bool operator==(const MessageKey&) const = default;
	};

	// Reads the key from a packet's first bytes (never reads past `length`)
	MessageKey KeyOf(const uint8_t* data, size_t length, bool outbound);

	struct MessageCount {
		MessageKey key;
		uint64_t count{};
		uint64_t bytes{};
	};

	// One second of traffic
	struct Second {
		int64_t time{};
		uint64_t packetsIn{};
		uint64_t packetsOut{};
		uint64_t bytesIn{};
		uint64_t bytesOut{};
		uint64_t httpRequests{};
		std::array<uint64_t, 5> httpStatus{}; // by StatusClass
		uint64_t httpBytesOut{};
		Histogram httpLatency;

		void Merge(const Second& other); // adds the counts (keeps this one's time)
		bool Idle() const { return packetsIn == 0 && packetsOut == 0 && httpRequests == 0; }
	};

	struct RouteStats {
		std::string route; // "GET /api/players/:id"
		uint64_t count{};
		std::array<uint64_t, 5> status{};
		uint64_t bytesOut{};
		Histogram latency;

		void Merge(const RouteStats& other);
	};

	// RakNet's view of the connections (all datagrams, acknowledgements and resends included), over the report
	struct Link {
		uint32_t connections{};
		uint64_t datagramsSent{};
		uint64_t datagramsReceived{};
		uint64_t bytesSent{};
		uint64_t bytesReceived{};
		uint64_t resends{};
		uint32_t resendQueue{};   // messages waiting to be resent now
		uint32_t averagePingMs{}; // over the connections
	};

	struct Report {
		std::vector<Second> seconds;         // oldest first, one per second without gaps
		std::vector<MessageCount> messages;  // the busiest types per direction
		std::vector<RouteStats> routes;
		Link link;
		std::vector<std::pair<std::string, double>> gauges; // e.g. workers_busy
	};

	class Recorder {
	public:
		static constexpr size_t TOP_MESSAGES = 24; // per direction, per report
		static constexpr size_t MAX_ROUTES = 64;   // distinct routes per report; more count as "other"
		static constexpr int64_t MAX_GAP = 120;    // seconds of silence a report fills in at most

		// `fanout`: how many connections a broadcast went to
		void Packet(int64_t now, const MessageKey& key, uint64_t bytes, uint32_t fanout = 1);
		void Http(int64_t now, const std::string& route, uint16_t status, uint64_t microseconds, uint64_t bytesOut);

		// Evaluated when a report is taken (on the thread that takes it)
		void SetGauge(const std::string& name, std::function<double()> source);

		/**
		 * The seconds before `now` not reported yet (silent ones as zeros, at most MAX_GAP of them), the busiest
		 * message types and the routes since the last report. The current second stays for the next one.
		 */
		Report Take(int64_t now);

		// Whether `interval` seconds passed since the last Take (the first call only starts the clock)
		bool Due(int64_t now, int64_t interval);

	private:
		Second& SecondAt(int64_t now); // under m_Mutex

		std::mutex m_Mutex;
		std::map<int64_t, Second> m_Seconds;
		Second* m_Current{};       // m_Seconds[m_CurrentTime] (map entries stay put)
		int64_t m_CurrentTime{};
		int64_t m_LastReported{}; // last second a report covered
		std::unordered_map<uint64_t, MessageCount> m_Messages;
		std::map<std::string, RouteStats> m_Routes;
		std::vector<std::pair<std::string, std::function<double()>>> m_Gauges;
		int64_t m_LastTake{};
	};

	// The busiest `limit` message types of each direction, busiest first
	std::vector<MessageCount> Top(const std::vector<MessageCount>& counts, size_t limit);

	// This process's recorder (dServer counts packets into it, the web server requests)
	Recorder& Local();

	int64_t Now(); // Unix seconds
}
