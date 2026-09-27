#include "SQLiteDatabase.h"

void SQLiteDatabase::InsertTrafficMinutes(const std::vector<TrafficMinute>& minutes) {
	if (minutes.empty()) return;
	const auto prevCommit = GetAutoCommit();
	SetAutoCommit(false);
	for (const auto& m : minutes) {
		ExecuteInsert("INSERT OR REPLACE INTO server_traffic (time, server, packets_in, packets_out, bytes_in, bytes_out, resends, http_requests, http_4xx, http_5xx, http_bytes_out, "
			"latency_p50_us, latency_p95_us, latency_p99_us) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
			m.time, m.server, m.packetsIn, m.packetsOut, m.bytesIn, m.bytesOut, m.resends, m.httpRequests, m.http4xx, m.http5xx, m.httpBytesOut,
			m.latencyP50Us, m.latencyP95Us, m.latencyP99Us);
	}
	Commit();
	SetAutoCommit(prevCommit);
}

std::vector<IServerTraffic::TrafficMinute> SQLiteDatabase::GetTrafficMinutes(int64_t from, int64_t to, int64_t bucketSeconds) {
	std::vector<TrafficMinute> minutes;
	auto [_, result] = ExecuteSelect("SELECT (time / ?) * ? AS bucket, server, SUM(packets_in) AS packets_in, SUM(packets_out) AS packets_out, SUM(bytes_in) AS bytes_in, "
		"SUM(bytes_out) AS bytes_out, SUM(resends) AS resends, SUM(http_requests) AS http_requests, SUM(http_4xx) AS http_4xx, SUM(http_5xx) AS http_5xx, "
		"SUM(http_bytes_out) AS http_bytes_out, AVG(latency_p50_us) AS p50, MAX(latency_p95_us) AS p95, MAX(latency_p99_us) AS p99 "
		"FROM server_traffic WHERE time >= ? AND time <= ? GROUP BY bucket, server ORDER BY bucket, server;", bucketSeconds, bucketSeconds, from, to);
	for (; !result.eof(); result.nextRow()) {
		TrafficMinute m;
		m.time = result.getInt64Field("bucket");
		m.server = result.getStringField("server");
		m.packetsIn = static_cast<uint64_t>(result.getInt64Field("packets_in"));
		m.packetsOut = static_cast<uint64_t>(result.getInt64Field("packets_out"));
		m.bytesIn = static_cast<uint64_t>(result.getInt64Field("bytes_in"));
		m.bytesOut = static_cast<uint64_t>(result.getInt64Field("bytes_out"));
		m.resends = static_cast<uint64_t>(result.getInt64Field("resends"));
		m.httpRequests = static_cast<uint64_t>(result.getInt64Field("http_requests"));
		m.http4xx = static_cast<uint64_t>(result.getInt64Field("http_4xx"));
		m.http5xx = static_cast<uint64_t>(result.getInt64Field("http_5xx"));
		m.httpBytesOut = static_cast<uint64_t>(result.getInt64Field("http_bytes_out"));
		m.latencyP50Us = static_cast<uint32_t>(result.getFloatField("p50"));
		m.latencyP95Us = static_cast<uint32_t>(result.getInt64Field("p95"));
		m.latencyP99Us = static_cast<uint32_t>(result.getInt64Field("p99"));
		minutes.push_back(std::move(m));
	}
	return minutes;
}

uint32_t SQLiteDatabase::PruneTrafficMinutes(int64_t beforeTime) {
	return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM server_traffic WHERE time < ?;", beforeTime));
}
