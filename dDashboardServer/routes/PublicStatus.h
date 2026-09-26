#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "IServerHealth.h"

/**
 * Pure helpers for the public status page (PublicRoutes.cpp), unit tested without a database or web server.
 */
namespace PublicStatus {
	struct Uptime {
		int64_t upSince{};              // start of the bucket the server has been up since; 0 when it is down now
		double availability24h{ -1.0 }; // share of the last day the server was up (0-1); -1 with no history yet
		double availability7d{ -1.0 };
	};

	/**
	 * Uptime from the minute-by-minute health samples, grouped into `bucket`-second buckets (oldest first) the way
	 * IServerHealth::GetHealthSamples returns them. The server counts as up when auth was up for a whole bucket, since
	 * that is when players can log in. A bucket with no sample means the dashboard (started by master) wasn't running,
	 * so the server was down. The current bucket is still filling and isn't counted in the availability.
	 */
	inline Uptime Summarize(const std::vector<IServerHealth::HealthSample>& samples, int64_t now, int64_t bucket) {
		Uptime uptime;
		if (samples.empty() || bucket <= 0) return uptime;

		// Up now: the newest bucket is recent (this one or the one before) and was up. Then walk back over up buckets
		// with no gap between them.
		const auto& newest = samples.back();
		if (newest.authOnline && now - newest.time < 2 * bucket) {
			size_t first = samples.size() - 1;
			while (first > 0 && samples[first - 1].authOnline && samples[first].time - samples[first - 1].time <= bucket) first--;
			uptime.upSince = samples[first].time;
		}

		const auto currentBucket = now / bucket * bucket;
		const auto availability = [&](int64_t span) {
			// From the first sample in the span: a server that is newer than the span isn't counted as down before it existed
			int64_t start = -1;
			uint32_t up = 0;
			for (const auto& sample : samples) {
				if (sample.time < now - span || sample.time >= currentBucket) continue;
				if (start < 0) start = sample.time;
				if (sample.authOnline) up++;
			}
			if (start < 0) return -1.0;
			const auto expected = (currentBucket - start) / bucket;
			if (expected <= 0) return -1.0;
			return std::min(1.0, static_cast<double>(up) / static_cast<double>(expected));
		};
		uptime.availability24h = availability(24 * 60 * 60);
		uptime.availability7d = availability(7 * 24 * 60 * 60);
		return uptime;
	}

	// A length of time in its largest whole unit, e.g. "3 days", "1 hour", "12 minutes" (under a minute: "a moment")
	inline std::string DurationText(int64_t seconds) {
		const auto unit = [](int64_t count, const char* name) { return std::to_string(count) + " " + name + (count == 1 ? "" : "s"); };
		if (seconds >= 86400) return unit(seconds / 86400, "day");
		if (seconds >= 3600) return unit(seconds / 3600, "hour");
		if (seconds >= 60) return unit(seconds / 60, "minute");
		return "a moment";
	}

	struct World {
		uint32_t zoneId{};
		std::string zoneName;
		uint32_t players{};
	};

	struct ZoneCount {
		uint32_t zoneId{};
		std::string zoneName;
		uint32_t players{};
		uint32_t instances{};
	};

	/**
	 * Running world instances added up per zone, busiest first (then by name). Instance and clone IDs are left out:
	 * a property's clone would tell who owns the property someone is on.
	 */
	inline std::vector<ZoneCount> CountByZone(const std::vector<World>& worlds) {
		std::map<uint32_t, ZoneCount> byZone;
		for (const auto& world : worlds) {
			auto& zone = byZone[world.zoneId];
			zone.zoneId = world.zoneId;
			if (zone.zoneName.empty()) zone.zoneName = world.zoneName;
			zone.players += world.players;
			zone.instances++;
		}
		std::vector<ZoneCount> zones;
		for (auto& [id, zone] : byZone) zones.push_back(std::move(zone));
		std::sort(zones.begin(), zones.end(), [](const ZoneCount& a, const ZoneCount& b) {
			if (a.players != b.players) return a.players > b.players;
			return a.zoneName < b.zoneName;
		});
		return zones;
	}
}
