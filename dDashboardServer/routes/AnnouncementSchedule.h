#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Cron.h"

/**
 * When a scheduled announcement is shown: its schedule (cron or @every, as for scheduled tasks), limited to the time
 * between its start and end. Announcements missed while the dashboard was down are not sent late. Pure; unit tested.
 */
namespace AnnouncementSchedule {
	constexpr size_t MAX_SCHEDULE_LENGTH = 128;
	constexpr size_t MAX_ZONES = 200;
	// Anything more often than this would drown chat
	constexpr int64_t MIN_INTERVAL_SECONDS = 60;

	/**
	 * The first send strictly after `after` (unix seconds) that falls between startsAt and endsAt (0: no limit);
	 * nullopt when there are no more. An @every interval counts from the start when there is one.
	 */
	inline std::optional<int64_t> Next(const Cron::Schedule& schedule, int64_t startsAt, int64_t endsAt, int64_t after) {
		std::optional<int64_t> next;
		if (startsAt > 0 && after < startsAt) {
			// The first send is at the start for an interval, or the first cron time from the start on
			next = schedule.intervalSeconds > 0 ? std::optional<int64_t>(startsAt) : Cron::Next(schedule, startsAt - 1);
		} else if (schedule.intervalSeconds > 0 && startsAt > 0) {
			// Stay on the start's beat: startsAt + k * interval
			next = startsAt + ((after - startsAt) / schedule.intervalSeconds + 1) * schedule.intervalSeconds;
		} else {
			next = Cron::Next(schedule, after);
		}
		if (!next || (endsAt > 0 && *next > endsAt)) return std::nullopt;
		return next;
	}

	// The shortest time between two sends: the interval, or for cron the smallest gap over the next few sends
	inline int64_t ShortestGap(const Cron::Schedule& schedule, int64_t from) {
		if (schedule.intervalSeconds > 0) return schedule.intervalSeconds;
		int64_t shortest = INT64_MAX;
		auto at = Cron::Next(schedule, from);
		for (int i = 0; i < 60 && at; i++) {
			const auto next = Cron::Next(schedule, *at);
			if (!next) break;
			shortest = std::min(shortest, *next - *at);
			at = next;
		}
		return shortest;
	}

	// What is wrong with an announcement as entered, if anything
	inline std::optional<std::string> Validate(const std::string& title, const std::string& message, const std::string& schedule,
		int64_t startsAt, int64_t endsAt, const std::vector<uint32_t>& zones, int64_t now) {
		if (message.empty() || message.size() > 1000) return "Write a message of up to 1000 characters";
		if (title.size() > 100) return "The title can be at most 100 characters";
		if (schedule.size() > MAX_SCHEDULE_LENGTH) return "The schedule is too long";
		std::string error;
		const auto parsed = Cron::Parse(schedule, error);
		if (!parsed) return error;
		if (ShortestGap(*parsed, now) < MIN_INTERVAL_SECONDS) return "Announcements can repeat at most once a minute";
		if (startsAt < 0 || endsAt < 0) return "Pick valid dates";
		if (endsAt > 0 && startsAt > 0 && endsAt <= startsAt) return "The end has to be after the start";
		if (endsAt > 0 && endsAt <= now) return "The end is already in the past";
		if (zones.size() > MAX_ZONES) return "Pick at most 200 zones";
		if (!Next(*parsed, startsAt, endsAt, now)) return "The schedule never fires between the start and the end";
		return std::nullopt;
	}
}
