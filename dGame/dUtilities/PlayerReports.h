#ifndef __PLAYERREPORTS__H__
#define __PLAYERREPORTS__H__

#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <string>

#include "dCommonVars.h"

class Entity;
namespace RakNet { class BitStream; }

/**
 * Reports players send from the game's Report Abuse window (the help menu, or "Report" on another player's name), kept
 * in player_reports for the dashboard. The client sends a report about another player as ReportBug naming the player
 * (nOtherPlayerID), and one about a model or a property as ReportOffensiveModel / ReportOffensiveProperty.
 */
namespace PlayerReports {
	/**
	 * How many reports one account may send from this world: a few a minute and a few dozen a day. Anything over is
	 * dropped (with a log line), so a player can't flood the table or the world's thread with 2000-character inserts.
	 * Times are unix seconds, passed in so it can be tested.
	 */
	class RateLimit {
	public:
		static constexpr size_t PER_MINUTE = 3;
		static constexpr size_t PER_DAY = 20;

		bool Allow(uint64_t reporter, int64_t now) {
			auto& times = m_Sent[reporter];
			while (!times.empty() && times.front() <= now - 24 * 60 * 60) times.pop_front();
			const auto lastMinute = static_cast<size_t>(std::count_if(times.begin(), times.end(), [now](int64_t t) { return t > now - 60; }));
			if (times.size() >= PER_DAY || lastMinute >= PER_MINUTE) return false;
			times.push_back(now);
			if (m_Sent.size() > 10000) std::erase_if(m_Sent, [now](const auto& entry) { return entry.second.empty() || entry.second.back() <= now - 24 * 60 * 60; });
			return true;
		}

	private:
		std::map<uint64_t, std::deque<int64_t>> m_Sent;
	};

	// ReportBug with nOtherPlayerID set; body is what the player wrote
	void ReportPlayer(Entity* reporter, LWOOBJID reportedId, const std::string& body);

	// ReportOffensiveModel: description, then the object the player picked
	void HandleReportOffensiveModel(RakNet::BitStream& inStream, Entity* reporter);

	// ReportOffensiveProperty: description, then the property's plaque
	void HandleReportOffensiveProperty(RakNet::BitStream& inStream, Entity* reporter);
}

#endif  //!__PLAYERREPORTS__H__
