#include "LeaderboardManager.h"

#include <ranges>
#include <sstream>
#include <utility>

#include "Database.h"
#include "EntityManager.h"
#include "Character.h"
#include "Game.h"
#include "GameMessages.h"
#include "Logger.h"
#include "dConfig.h"
#include "CDClientManager.h"
#include "GeneralUtils.h"
#include "Entity.h"
#include "LDFFormat.h"
#include "DluAssert.h"

#include "CDActivitiesTable.h"
#include "Metrics.hpp"

namespace LeaderboardManager {
	std::map<GameID, Leaderboard::Type> leaderboardCache;
}

Leaderboard::Leaderboard(const GameID gameID, const Leaderboard::InfoType infoType, const bool weekly, LWOOBJID relatedPlayer, const uint32_t numResults, const Leaderboard::Type leaderboardType) {
	this->gameID = gameID;
	this->weekly = weekly;
	this->infoType = infoType;
	this->leaderboardType = leaderboardType;
	this->relatedPlayer = relatedPlayer;
	this->numResults = numResults;
}

Leaderboard::~Leaderboard() {
	Clear();
}

void Leaderboard::Clear() {
	for (auto& entry : entries) for (auto ldfData : entry) delete ldfData;
}

inline void WriteLeaderboardRow(std::ostringstream& leaderboard, const uint32_t& index, LDFBaseData* data) {
	leaderboard << "\nResult[0].Row[" << index << "]." << data->GetString();
}

void Leaderboard::Serialize(RakNet::BitStream& bitStream) const {
	bitStream.Write(gameID);
	bitStream.Write(infoType);

	std::ostringstream leaderboard;

	leaderboard << "ADO.Result=7:1"; // Unused in 1.10.64, but is in captures
	leaderboard << "\nResult.Count=1:1"; // number of results, always 1
	if (!this->entries.empty()) leaderboard << "\nResult[0].Index=0:RowNumber"; // "Primary key".  Live doesn't include this if there are no entries.
	leaderboard << "\nResult[0].RowCount=1:" << entries.size();

	int32_t rowNumber = 0;
	for (auto& entry : entries) {
		for (auto* data : entry) {
			WriteLeaderboardRow(leaderboard, rowNumber, data);
		}
		rowNumber++;
	}

	// Serialize the thing to a BitStream
	uint32_t leaderboardSize = leaderboard.tellp();
	bitStream.Write<uint32_t>(leaderboardSize);
	// Doing this all in 1 call so there is no possbility of a dangling pointer.
	bitStream.WriteAlignedBytes(reinterpret_cast<const unsigned char*>(GeneralUtils::ASCIIToUTF16(leaderboard.str()).c_str()), leaderboardSize * sizeof(char16_t));
	if (leaderboardSize > 0) bitStream.Write<uint16_t>(0);
	bitStream.Write0();
	bitStream.Write0();
}

// Takes the resulting query from a leaderboard lookup and converts it to the LDF we need
// to send it to a client.
void QueryToLdf(Leaderboard& leaderboard, const std::vector<ILeaderboard::Entry>& leaderboardEntries) {
	using enum Leaderboard::Type;
	leaderboard.Clear();
	if (leaderboardEntries.empty()) return;

	for (const auto& leaderboardEntry : leaderboardEntries) {
		constexpr int32_t MAX_NUM_DATA_PER_ROW = 9;
		auto& entry = leaderboard.PushBackEntry();
		entry.reserve(MAX_NUM_DATA_PER_ROW);
		entry.push_back(new LDFData<uint64_t>(u"CharacterID", leaderboardEntry.charId));
		entry.push_back(new LDFData<uint64_t>(u"LastPlayed", leaderboardEntry.lastPlayedTimestamp));
		entry.push_back(new LDFData<int32_t>(u"NumPlayed", leaderboardEntry.numTimesPlayed));
		entry.push_back(new LDFData<std::u16string>(u"name", GeneralUtils::ASCIIToUTF16(leaderboardEntry.name)));
		entry.push_back(new LDFData<uint64_t>(u"RowNumber", leaderboardEntry.ranking));
		switch (leaderboard.GetLeaderboardType()) {
		case ShootingGallery:
			entry.push_back(new LDFData<int32_t>(u"Score", leaderboardEntry.primaryScore));
			// Score:1
			entry.push_back(new LDFData<int32_t>(u"Streak", leaderboardEntry.secondaryScore));
			// Streak:1
			entry.push_back(new LDFData<float>(u"HitPercentage", leaderboardEntry.tertiaryScore));
			// HitPercentage:3 between 0 and 1
			break;
		case Racing:
			entry.push_back(new LDFData<float>(u"BestTime", leaderboardEntry.primaryScore));
			// BestLapTime:3
			entry.push_back(new LDFData<float>(u"BestLapTime", leaderboardEntry.secondaryScore));
			// BestTime:3
			entry.push_back(new LDFData<int32_t>(u"License", 1));
			// License:1 - 1 if player has completed mission 637 and 0 otherwise
			entry.push_back(new LDFData<int32_t>(u"NumWins", leaderboardEntry.numWins));
			// NumWins:1
			break;
		case UnusedLeaderboard4:
			entry.push_back(new LDFData<int32_t>(u"Points", leaderboardEntry.primaryScore));
			// Points:1
			break;
		case MonumentRace:
			entry.push_back(new LDFData<int32_t>(u"Time", leaderboardEntry.primaryScore));
			// Time:1(?)
			break;
		case FootRace:
			entry.push_back(new LDFData<int32_t>(u"Time", leaderboardEntry.primaryScore));
			// Time:1
			break;
		case Survival:
			entry.push_back(new LDFData<int32_t>(u"Points", leaderboardEntry.primaryScore));
			// Points:1
			entry.push_back(new LDFData<int32_t>(u"Time", leaderboardEntry.secondaryScore));
			// Time:1
			break;
		case SurvivalNS:
			entry.push_back(new LDFData<int32_t>(u"Wave", leaderboardEntry.primaryScore));
			// Wave:1
			entry.push_back(new LDFData<int32_t>(u"Time", leaderboardEntry.secondaryScore));
			// Time:1
			break;
		case Donations:
			entry.push_back(new LDFData<int32_t>(u"Score", leaderboardEntry.primaryScore));
			// Score:1
			break;
		case None:
			[[fallthrough]];
		default:
			break;
		}
	}
}

std::vector<ILeaderboard::Entry> FilterToNumResults(const std::vector<ILeaderboard::Entry>& leaderboard, const LWOOBJID relatedPlayer, const Leaderboard::InfoType infoType, const uint32_t numResults) {
	std::vector<ILeaderboard::Entry> toReturn;

	int32_t index = 0;
	// for friends and top, we dont need to find this players index.
	if (infoType == Leaderboard::InfoType::MyStanding || infoType == Leaderboard::InfoType::Friends) {
		for (; index < leaderboard.size(); index++) {
			if (leaderboard[index].charId == relatedPlayer) break;
		}
	}

	if (leaderboard.size() < numResults) {
		toReturn.assign(leaderboard.begin(), leaderboard.end());
		index = 0;
	} else if (index < numResults) {
		toReturn.assign(leaderboard.begin(), leaderboard.begin() + numResults); // get the top 10 since we are in the top 10
		index = 0;
	} else if (index > leaderboard.size() - numResults) {
		toReturn.assign(leaderboard.end() - numResults, leaderboard.end()); // get the bottom 10 since we are in the bottom 10
		index = leaderboard.size() - numResults;
	} else {
		auto half = numResults / 2;
		toReturn.assign(leaderboard.begin() + index - half, leaderboard.begin() + index + half); // get the 5 above and below
		index -= half;
	}

	int32_t i = index;
	for (auto& entry : toReturn) {
		entry.ranking = ++i;
	}

	return toReturn;
}

std::vector<ILeaderboard::Entry> FilterWeeklies(const std::vector<ILeaderboard::Entry>& leaderboard) {
	using namespace std::chrono;
	// Filter the leaderboard to only include entries from the last week
	const auto epochTime = system_clock::now();
	constexpr auto oneWeek = weeks(1);

	std::vector<ILeaderboard::Entry> weeklyLeaderboard;
	for (const auto& entry : leaderboard) {
		const sys_time<seconds> asSysTime(seconds(entry.lastPlayedTimestamp));
		const auto timeDiff = epochTime - asSysTime;
		if (timeDiff < oneWeek) {
			weeklyLeaderboard.push_back(entry);
		}
	}

	return weeklyLeaderboard;
}

std::vector<ILeaderboard::Entry> FilterFriends(const std::vector<ILeaderboard::Entry>& leaderboard, const LWOOBJID relatedPlayer) {
	// Filter the leaderboard to only include friends of the player
	auto friendOfPlayer = Database::Get()->GetFriendsList(relatedPlayer);
	std::vector<ILeaderboard::Entry> friendsLeaderboard;
	for (const auto& entry : leaderboard) {
		const auto res = std::ranges::find_if(friendOfPlayer, [&entry, relatedPlayer](const FriendData& data) {
			return entry.charId == data.friendID;
			});
		if (res != friendOfPlayer.cend() || entry.charId == relatedPlayer) {
			friendsLeaderboard.push_back(entry);
		}
	}

	return friendsLeaderboard;
}

std::vector<ILeaderboard::Entry> ProcessLeaderboard(
	const std::vector<ILeaderboard::Entry>& leaderboard,
	const bool weekly,
	const Leaderboard::InfoType infoType,
	const LWOOBJID relatedPlayer,
	const uint32_t numResults) {
	std::vector<ILeaderboard::Entry> toReturn;

	if (infoType == Leaderboard::InfoType::Friends) {
		const auto friendsLeaderboard = FilterFriends(leaderboard, relatedPlayer);
		toReturn = FilterToNumResults(weekly ? FilterWeeklies(friendsLeaderboard) : friendsLeaderboard, relatedPlayer, infoType, numResults);
	} else {
		toReturn = FilterToNumResults(weekly ? FilterWeeklies(leaderboard) : leaderboard, relatedPlayer, infoType, numResults);
	}

	return toReturn;
}

void Leaderboard::SetupLeaderboard(bool weekly) {
	const auto leaderboardType = LeaderboardManager::GetLeaderboardType(gameID);
	std::vector<ILeaderboard::Entry> leaderboardRes;

	switch (leaderboardType) {
	case Type::SurvivalNS:
		leaderboardRes = Database::Get()->GetNsLeaderboard(gameID);
		break;
	case Type::Survival:
		leaderboardRes = Database::Get()->GetAgsLeaderboard(gameID);
		break;
	case Type::Racing:
		[[fallthrough]];
	case Type::MonumentRace:
		leaderboardRes = Database::Get()->GetAscendingLeaderboard(gameID);
		break;
	case Type::ShootingGallery:
		[[fallthrough]];
	case Type::FootRace:
		[[fallthrough]];
	case Type::Donations:
		[[fallthrough]];
	case Type::None:
		[[fallthrough]];
	default:
		leaderboardRes = Database::Get()->GetDescendingLeaderboard(gameID);
		break;
	}

	const auto processedLeaderboard = ProcessLeaderboard(leaderboardRes, weekly, infoType, relatedPlayer, numResults);

	QueryToLdf(*this, processedLeaderboard);
}

void Leaderboard::Send(const LWOOBJID targetID) const {
	auto* player = Game::entityManager->GetEntity(relatedPlayer);
	if (player != nullptr) {
		GameMessages::SendActivitySummaryLeaderboardData(targetID, this, player->GetSystemAddress());
	}
}

void LeaderboardManager::SaveScore(const LWOOBJID& playerID, const GameID activityId, const float primaryScore, const float secondaryScore, const float tertiaryScore) {
	const Leaderboard::Type leaderboardType = GetLeaderboardType(activityId);

	const auto oldScore = Database::Get()->GetPlayerScore(playerID, activityId);

	ILeaderboard::Score newScore{ .primaryScore = primaryScore, .secondaryScore = secondaryScore, .tertiaryScore = tertiaryScore };
	if (oldScore.has_value()) {
		bool lowerScoreBetter = leaderboardType == Leaderboard::Type::Racing || leaderboardType == Leaderboard::Type::MonumentRace;
		bool newHighScore = lowerScoreBetter ? newScore < oldScore : newScore > oldScore;
		// Nimbus station has a weird leaderboard where we need a custom scoring system
		if (leaderboardType == Leaderboard::Type::SurvivalNS) {
			newHighScore = newScore.primaryScore > oldScore->primaryScore ||
				(newScore.primaryScore == oldScore->primaryScore && newScore.secondaryScore < oldScore->secondaryScore);
		} else if (leaderboardType == Leaderboard::Type::Survival && Game::config->GetValue("classic_survival_scoring") == "1") {
			ILeaderboard::Score oldScoreFlipped{oldScore->secondaryScore, oldScore->primaryScore, oldScore->tertiaryScore};
			ILeaderboard::Score newScoreFlipped{newScore.secondaryScore, newScore.primaryScore, newScore.tertiaryScore};
			newHighScore = newScoreFlipped > oldScoreFlipped;
		} else if (leaderboardType == Leaderboard::Type::Donations) {
			// Donations just need to go up if updated
			newHighScore = true;
			newScore.primaryScore += oldScore->primaryScore;
		}

		if (newHighScore) {
			Database::Get()->UpdateScore(playerID, activityId, newScore);
		} else {
			Database::Get()->IncrementTimesPlayed(playerID, activityId);
		}
	} else {
		Database::Get()->SaveScore(playerID, activityId, newScore);
	}

	// track wins separately
	if (leaderboardType == Leaderboard::Type::Racing && tertiaryScore != 0.0f) {
		Database::Get()->IncrementNumWins(playerID, activityId);
	}
}

void LeaderboardManager::SendLeaderboard(const GameID gameID, const Leaderboard::InfoType infoType, const bool weekly, const LWOOBJID playerID, const LWOOBJID targetID, const uint32_t numResults) {
	Leaderboard leaderboard(gameID, infoType, weekly, playerID, numResults, GetLeaderboardType(gameID));
	leaderboard.SetupLeaderboard(weekly);
	leaderboard.Send(targetID);
}

Leaderboard::Type LeaderboardManager::GetLeaderboardType(const GameID gameID) {
	auto lookup = leaderboardCache.find(gameID);
	if (lookup != leaderboardCache.end()) return lookup->second;

	auto* activitiesTable = CDClientManager::GetTable<CDActivitiesTable>();
	std::vector<CDActivities> activities = activitiesTable->Query([gameID](const CDActivities& entry) {
		return entry.ActivityID == gameID;
		});
	auto type = !activities.empty() ? static_cast<Leaderboard::Type>(activities.at(0).leaderboardType) : Leaderboard::Type::None;
	leaderboardCache.insert_or_assign(gameID, type);
	return type;
}
