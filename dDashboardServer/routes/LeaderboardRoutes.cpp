#include "LeaderboardRoutes.h"
#include "GameText.h"

#include <algorithm>
#include <set>

#include "RouteUtils.h"
#include "Strikes.h"
#include "WSRoutes.h"
#include "CDClientDatabase.h"
#include "Database.h"
#include "GameLabels.h"
#include "Game.h"
#include "dConfig.h"
#include "GeneralUtils.h"
#include "eHTTPMethod.h"
#include "eLeaderboardType.h"

using namespace RouteUtils;

namespace {
	constexpr size_t TOP_ENTRIES = 100;

	struct Board {
		uint32_t id{};
		eLeaderboardType type{};
		std::string name;
	};

	std::string ActivityName(uint32_t id) {
		std::string name = GameText::Phrase(GameText::Key("Activities", id, "ActivityName"));
		if (name.empty()) return "Activity " + std::to_string(id);
		// The client's locale has Windows-1252 dashes stored as U+0096 ("Foot Race \u0096 Nimbus Station"); make them real dashes
		for (size_t at = name.find("\xC2\x96"); at != std::string::npos; at = name.find("\xC2\x96", at)) name.replace(at, 2, "\xE2\x80\x93");
		return name;
	}

	std::vector<Board> Boards() {
		std::vector<Board> boards;
		std::set<uint32_t> seen;
		auto result = CDClientDatabase::ExecuteQuery("SELECT ActivityID, leaderboardType FROM Activities WHERE leaderboardType >= 0 ORDER BY ActivityID;");
		for (; !result.eof(); result.nextRow()) {
			const auto id = static_cast<uint32_t>(result.getIntField(0));
			if (seen.insert(id).second) boards.push_back({ id, static_cast<eLeaderboardType>(result.getIntField(1)), ActivityName(id) });
		}
		return boards;
	}

	std::optional<Board> FindBoard(uint32_t id) {
		for (auto& board : Boards()) if (board.id == id) return board;
		return std::nullopt;
	}

	struct Column {
		const char* key;    // field of EntryJson
		const char* phrase; // client locale phrase the game's header uses
		const char* format; // number, time, laptime (hundredths), percent
		const char* hint = "";
	};

	// The client's column header for a locale phrase ("RACE_BESTTIME" -> "Best Time"); the key as words when the locale lacks it
	std::string Label(const char* phrase) {
		return GameText::Text(phrase, GameLabels::Words(phrase));
	}

	/**
	 * The columns the game's leaderboard shows for each type, in its order and with its headers
	 * (LWOCharacterComponent::msgSendActivitySummaryLeaderboardData in the 1.10.64 client; values as the server sends
	 * them, see QueryToLdf in dGame/LeaderboardManager.cpp). The game adds Times Played and Last Played to all-time
	 * boards except donations; the page shows last played itself.
	 */
	nlohmann::json Columns(eLeaderboardType type) {
		using enum eLeaderboardType;
		std::vector<Column> columns;
		switch (type) {
		case ShootingGallery: columns = { {"primary", "SCORE", "number"}, {"secondary", "UI_SG_STREAK", "number"}, {"tertiary", "UI_SG_ACCURACY", "percent"} }; break;
		case Racing: columns = { {"primary", "RACE_BESTTIME", "laptime", "Less is better"}, {"secondary", "RACE_BESTLAP", "laptime"}, {"wins", "RACE_NUMWINS", "number"} }; break;
		case MonumentRace: columns = { {"primary", "TIME", "time", "Time taken: less is better"} }; break;
		// The foot race scripts save the time left on the race's countdown when the player finishes
		case FootRace: columns = { {"primary", "TIME", "time", "Time left on the clock at the finish: more is better"} }; break;
		case UnusedLeaderboard4: columns = { {"primary", "POINTS", "number"} }; break;
		case Survival: columns = { {"secondary", "TIME", "time"}, {"primary", "POINTS", "number"} }; break;
		case SurvivalNS: columns = { {"primary", "WAVE", "number"}, {"secondary", "TIME", "time", "Less is better for the same wave"} }; break;
		case Donations: columns = { {"primary", "DONATIONS", "number"} }; break;
		default: columns = { {"primary", "SCORE", "number"} }; break;
		}
		if (type != Donations) columns.push_back({ "played", "TIMES_PLAYED", "number" });
		// The score the order goes by first: survival ranks by time with classic_survival_scoring (as GetAgsLeaderboard does)
		const bool byTime = type == Survival && Game::config && Game::config->GetValue("classic_survival_scoring") == "1";
		const std::string ranksBy = byTime ? "secondary" : "primary";
		nlohmann::json out = nlohmann::json::array();
		for (const auto& column : columns) {
			out.push_back({ {"key", column.key}, {"label", Label(column.phrase)}, {"format", column.format}, {"hint", column.hint}, {"ranks", column.key == ranksBy} });
		}
		return out;
	}

	// Ranked the same way the game ranks it (ILeaderboard::GetRankedLeaderboard, shared with LeaderboardManager)
	std::vector<ILeaderboard::Entry> Ranked(const Board& board) {
		auto entries = Database::Get()->GetRankedLeaderboard(board.type, board.id);
		uint32_t rank = 0;
		for (auto& entry : entries) entry.ranking = ++rank; // the game numbers rows the same way
		return entries;
	}

	nlohmann::json EntryJson(const ILeaderboard::Entry& e, bool mine) {
		return { {"rank", e.ranking}, {"character_id", std::to_string(e.charId)}, {"name", e.name}, {"primary", e.primaryScore}, {"secondary", e.secondaryScore},
			{"tertiary", e.tertiaryScore}, {"wins", e.numWins}, {"played", e.numTimesPlayed}, {"last_played", e.lastPlayedTimestamp}, {"mine", mine} };
	}
}

nlohmann::json LeaderboardTops(size_t perBoard) {
	nlohmann::json boards = nlohmann::json::array();
	if (perBoard == 0) return boards;
	const auto sizes = Database::Get()->GetLeaderboardSizes();
	for (const auto& board : Boards()) {
		const auto size = sizes.find(board.id);
		if (size == sizes.end() || size->second == 0) continue;
		nlohmann::json top = nlohmann::json::array();
		for (const auto& e : Ranked(board)) {
			if (top.size() >= perBoard) break;
			top.push_back({ {"rank", e.ranking}, {"name", e.name}, {"primary", e.primaryScore}, {"secondary", e.secondaryScore},
				{"tertiary", e.tertiaryScore}, {"wins", e.numWins}, {"played", e.numTimesPlayed} });
		}
		boards.push_back({ {"id", board.id}, {"name", board.name}, {"columns", Columns(board.type)}, {"top", top} });
	}
	return boards;
}

void RegisterLeaderboardRoutes() {
	Route(eHTTPMethod::GET, "/api/leaderboards", Perm("leaderboards_view"), "Activities with a leaderboard, and how many scores each has",
		[](HTTPReply& reply, const HTTPContext&) {
			const auto sizes = Database::Get()->GetLeaderboardSizes();
			nlohmann::json boards = nlohmann::json::array();
			for (const auto& board : Boards()) {
				const auto size = sizes.find(board.id);
				boards.push_back({ {"id", board.id}, {"name", board.name}, {"type", static_cast<uint32_t>(board.type)}, {"scores", size == sizes.end() ? 0 : size->second} });
			}
			JsonSuccess(reply, { {"boards", boards} });
		});

	Route(eHTTPMethod::GET, "/api/leaderboards/:id", Perm("leaderboards_view"),
		"One leaderboard: the top 100 in the game's order, and where your own characters are. Query: ?search= (a character name) to find anyone's place",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto id = PathId<uint32_t>(context.path, 2);
			const auto board = id ? FindBoard(*id) : std::nullopt;
			if (!board) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No leaderboard for that activity");
			auto search = QueryValue(context.queryString, "search");
			std::transform(search.begin(), search.end(), search.begin(), ::tolower);

			std::set<LWOOBJID> own;
			for (const auto charId : Database::Get()->GetAccountCharacterIds(context.accountId)) own.insert(charId);
			nlohmann::json top = nlohmann::json::array(), mine = nlohmann::json::array(), found = nlohmann::json::array();
			const auto entries = Ranked(*board);
			for (const auto& entry : entries) {
				const bool isMine = own.contains(entry.charId);
				if (top.size() < TOP_ENTRIES) top.push_back(EntryJson(entry, isMine));
				if (isMine) mine.push_back(EntryJson(entry, true));
				if (!search.empty() && found.size() < 50) {
					auto name = entry.name;
					std::transform(name.begin(), name.end(), name.begin(), ::tolower);
					if (name.find(search) != std::string::npos || std::to_string(entry.charId) == search) found.push_back(EntryJson(entry, isMine));
				}
			}
			JsonSuccess(reply, { {"id", board->id}, {"name", board->name}, {"type", static_cast<uint32_t>(board->type)}, {"columns", Columns(board->type)},
				{"total", entries.size()}, {"top", top}, {"mine", mine}, {"found", found} });
		});

	Route(eHTTPMethod::POST, "/api/leaderboards/:id/remove", Perm("leaderboards_manage"), "Remove one character's score from a leaderboard. Body: {character_id, reason, strike (true: also a strike on their account)}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto id = PathId<uint32_t>(context.path, 2);
			const auto body = ParseBody(context);
			const auto board = id ? FindBoard(*id) : std::nullopt;
			if (!board) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No leaderboard for that activity");
			if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			const auto charId = GeneralUtils::TryParse<LWOOBJID>(body->value("character_id", ""));
			const auto info = charId ? Database::Get()->GetCharacterInfo(*charId) : std::nullopt;
			if (!info) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Character not found");
			const auto strike = Strikes::Requested(context, *body, reply);
			if (!strike) return;
			if (*strike && !AuthorizeAccountAction(context, info->accountId, reply, eAccountAction::MODERATION)) return;
			Database::Get()->DeleteLeaderboardScore(*charId, board->id);
			const std::string reason = body->value("reason", "");
			Audit(context, "remove_leaderboard_score", info->name + " from " + board->name + " (activity " + std::to_string(board->id) + ")" + (reason.empty() ? "" : ": " + reason.substr(0, 200)), AuditTarget::Character(*charId));
			BroadcastTableChanged("leaderboard", std::to_string(board->id));
			nlohmann::json result{ {"message", info->name + "'s score was removed"} };
			if (*strike) {
				const auto given = Strikes::Give(context, info->accountId, *charId, eStrikeSource::LEADERBOARD, board->name, reason);
				given.Into(result);
				result["message"] = info->name + "'s score was removed and their account has a strike (" + std::to_string(given.active) + " active)";
			}
			JsonSuccess(reply, result);
		});

	Route(eHTTPMethod::POST, "/api/leaderboards/:id/reset", Perm("leaderboards_manage"), "Clear a whole leaderboard. Body: {confirm: the activity ID}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto id = PathId<uint32_t>(context.path, 2);
			const auto body = ParseBody(context);
			const auto board = id ? FindBoard(*id) : std::nullopt;
			if (!board) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No leaderboard for that activity");
			if (!body || body->value("confirm", "") != std::to_string(board->id)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Confirm with the activity ID");
			const auto before = Database::Get()->GetLeaderboardSizes()[board->id];
			Database::Get()->ResetLeaderboard(board->id);
			Audit(context, "reset_leaderboard", board->name + " (activity " + std::to_string(board->id) + "), " + std::to_string(before) + " scores");
			BroadcastTableChanged("leaderboard", std::to_string(board->id));
			JsonSuccess(reply, { {"message", "Cleared " + std::to_string(before) + " scores"} });
		});
}
