#ifndef __ISERVERTRAFFIC__H__
#define __ISERVERTRAFFIC__H__

#include <cstdint>
#include <string>
#include <vector>

/**
 * Per-minute traffic of each server (packets, bytes, HTTP requests), written by the dashboard once a minute from the
 * servers' traffic reports and kept for traffic_days. The last hour at one second resolution is only in memory.
 */
class IServerTraffic {
public:
	struct TrafficMinute {
		int64_t time{};       // start of the minute (or bucket), Unix seconds
		std::string server;   // "master", "auth", "chat", "dashboard", "ugc", "world:<zone>:<instance>"
		uint64_t packetsIn{};
		uint64_t packetsOut{};
		uint64_t bytesIn{};
		uint64_t bytesOut{};
		uint64_t resends{};
		uint64_t httpRequests{};
		uint64_t http4xx{};
		uint64_t http5xx{};
		uint64_t httpBytesOut{};
		uint32_t latencyP50Us{}; // HTTP; in a bucket of several minutes: the average of the minutes'
		uint32_t latencyP95Us{}; // in a bucket: the worst minute's
		uint32_t latencyP99Us{}; // in a bucket: the worst minute's
	};

	// One batch a minute
	virtual void InsertTrafficMinutes(const std::vector<TrafficMinute>& minutes) = 0;

	// Rows between from and to (inclusive), summed per server into buckets of `bucketSeconds`, ordered by time
	virtual std::vector<TrafficMinute> GetTrafficMinutes(int64_t from, int64_t to, int64_t bucketSeconds) = 0;

	virtual uint32_t PruneTrafficMinutes(int64_t beforeTime) = 0;
};

#endif  //!__ISERVERTRAFFIC__H__
