#ifndef __ILEADERBOARD__H__
#define __ILEADERBOARD__H__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class ILeaderboard {
public:

	struct Entry {
		uint32_t charId{};
		uint32_t lastPlayedTimestamp{};
		float primaryScore{};
		float secondaryScore{};
		uint32_t tertiaryScore{};
		uint32_t numWins{};
		uint32_t numTimesPlayed{};
		uint32_t ranking{};
		std::string name{};
	};

	struct Score {
		auto operator<=>(const Score& rhs) const = default;

		float primaryScore{ 0.0f };
		float secondaryScore{ 0.0f };
		float tertiaryScore{ 0.0f };
	};

	// Get the donation total for the given activity id.
	virtual std::optional<uint32_t> GetDonationTotal(const uint32_t activityId) = 0;

	virtual std::vector<ILeaderboard::Entry> GetDescendingLeaderboard(const uint32_t activityId) = 0;
	virtual std::vector<ILeaderboard::Entry> GetAscendingLeaderboard(const uint32_t activityId) = 0;
	virtual std::vector<ILeaderboard::Entry> GetNsLeaderboard(const uint32_t activityId) = 0;
	virtual std::vector<ILeaderboard::Entry> GetAgsLeaderboard(const uint32_t activityId) = 0;
	virtual std::optional<Score> GetPlayerScore(const uint32_t playerId, const uint32_t gameId) = 0;

	virtual void SaveScore(const uint32_t playerId, const uint32_t gameId, const Score& score) = 0;
	virtual void UpdateScore(const uint32_t playerId, const uint32_t gameId, const Score& score) = 0;
	virtual void IncrementNumWins(const uint32_t playerId, const uint32_t gameId) = 0;
	virtual void IncrementTimesPlayed(const uint32_t playerId, const uint32_t gameId) = 0;
};

#endif  //!__ILEADERBOARD__H__
