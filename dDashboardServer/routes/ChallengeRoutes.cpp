#include "ChallengeRoutes.h"
#include "LiveEventRoutes.h"
#include "LiveWorld.h"
#include "DashboardRoutes.h"
#include "ClientAssets.h"
#include "GameLabels.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <set>

#include "RouteUtils.h"
#include "WSRoutes.h"
#include "Alerts.h"
#include "Database.h"
#include "CDClientDatabase.h"
#include "Game.h"
#include "dServer.h"
#include "Logger.h"
#include "dConfig.h"
#include "MailInfo.h"
#include "LiveOpsRules.h"
#include "StatisticID.h"
#include "IEconomyLedger.h"
#include "eHTTPMethod.h"
#include "magic_enum.hpp"

using namespace RouteUtils;

namespace {
	using Challenge = ILiveOps::Challenge;
	using eState = ILiveOps::eChallengeState;
	using eMetric = ILiveOps::eChallengeMetric;
	using LiveOpsRules::ePhase;
	constexpr auto CHECK_INTERVAL = std::chrono::seconds(10);
	constexpr size_t MAX_TITLE = 100;
	constexpr size_t MAX_DESCRIPTION = 400;
	constexpr size_t MAX_REWARD_ITEMS = 5;
	constexpr int64_t MAX_TARGET = 1'000'000'000'000;

	std::chrono::steady_clock::time_point g_NextCheck{};

	int64_t Now() { return static_cast<int64_t>(std::time(nullptr)); }

	std::string ZoneList(const std::vector<uint32_t>& zones) {
		if (zones.empty()) return "every zone";
		std::string text;
		const auto& names = ZoneNames();
		for (const auto zone : zones) {
			const auto key = std::to_string(zone);
			text += (text.empty() ? "" : ", ") + (names.contains(key) ? names[key].get<std::string>() : "zone " + key);
		}
		return text;
	}

	// "Enemy Kills", "Quick Builds Completed" (with the LOT's name when limited to one)
	std::string MetricName(const Challenge& challenge) {
		std::string name;
		if (challenge.metricKind == eMetric::STATISTIC) {
			const auto stat = magic_enum::enum_cast<StatisticID>(static_cast<int>(challenge.metric));
			name = stat ? GameLabels::Name(*stat) : "Statistic " + std::to_string(challenge.metric);
		} else {
			const auto kind = magic_enum::enum_cast<IEconomyLedger::eMapEvent>(static_cast<uint8_t>(challenge.metric));
			name = kind ? GameLabels::Name(*kind) : "Map event " + std::to_string(challenge.metric);
		}
		if (challenge.lot != 0) name += " (" + ClientAssets::ObjectName(challenge.lot).value_or("LOT " + std::to_string(challenge.lot)) + ")";
		return name;
	}

	const std::vector<uint8_t>& Milestones() {
		static std::string lastText;
		static std::vector<uint8_t> milestones = LiveOpsRules::DefaultMilestones();
		const auto text = Game::config ? Game::config->GetValue("challenge_milestones") : "";
		if (text != lastText) {
			lastText = text;
			milestones = LiveOpsRules::ParseMilestones(text);
		}
		return milestones;
	}

	ePhase PhaseOf(const Challenge& challenge, int64_t now) {
		return LiveOpsRules::Phase(static_cast<uint8_t>(challenge.state), challenge.startsAt, challenge.endsAt, now);
	}

	nlohmann::json RewardItems(const Challenge& challenge) {
		auto items = nlohmann::json::parse(challenge.rewardItems, nullptr, false);
		return items.is_array() ? items : nlohmann::json::array();
	}

	std::string RewardText(const Challenge& challenge) {
		std::string text;
		if (challenge.rewardCoins > 0) text = std::to_string(challenge.rewardCoins) + " coins";
		for (const auto& item : RewardItems(challenge)) {
			const auto lot = item.value("lot", 0);
			text += (text.empty() ? "" : ", ") + std::to_string(item.value("count", 1)) + "x " + ClientAssets::ItemName(lot);
		}
		return text;
	}

	nlohmann::json ChallengeJson(const Challenge& challenge, const ILiveOps::ChallengeTotal& total, int64_t now) {
		return { {"id", challenge.id}, {"title", challenge.title}, {"description", challenge.description},
			{"metricKind", challenge.metricKind == eMetric::STATISTIC ? "stat" : "map"}, {"metric", challenge.metric}, {"lot", challenge.lot},
			{"metricName", MetricName(challenge)}, {"zones", challenge.zones}, {"zoneNames", ZoneList(challenge.zones)},
			{"target", challenge.target}, {"total", total.total}, {"contributors", total.contributors}, {"percent", LiveOpsRules::Percent(total.total, challenge.target)},
			{"startsAt", challenge.startsAt}, {"endsAt", challenge.endsAt}, {"phase", LiveOpsRules::PhaseName(PhaseOf(challenge, now))},
			{"includeStaff", challenge.includeStaff}, {"isPublic", challenge.isPublic}, {"rewardCoins", challenge.rewardCoins},
			{"rewardItems", RewardItems(challenge)}, {"rewardText", RewardText(challenge)}, {"rewardMin", challenge.rewardMin},
			{"milestone", challenge.milestone}, {"completedAt", challenge.completedAt}, {"rewardedAt", challenge.rewardedAt}, {"rewardedCount", challenge.rewardedCount},
			{"createdBy", challenge.createdBy}, {"createdAt", challenge.createdAt} };
	}

	std::string Describe(const Challenge& challenge) {
		return "#" + std::to_string(challenge.id) + " \"" + challenge.title + "\": " + std::to_string(challenge.target) + " " + MetricName(challenge) + " in " + ZoneList(challenge.zones);
	}

	void Save(Challenge& challenge, const std::string& by) {
		challenge.updatedAt = Now();
		challenge.updatedBy = by;
		Database::Get()->UpdateChallenge(challenge);
		BroadcastTableChanged("challenges", std::to_string(challenge.id));
	}

	std::string Clip(const std::string& text, size_t length) {
		return text.size() <= length ? text : text.substr(0, length - 3) + "...";
	}

	/**
	 * Hand out a completed challenge's rewards: a reward row per character that contributed at least reward_min, its
	 * items by mail (one mail per kind of item) and, for coins, a mail saying they wait in game. Characters already
	 * rewarded are skipped, so running this again after an interruption gives nobody anything twice.
	 */
	void GiveRewards(Challenge& challenge) {
		const auto now = Now();
		std::vector<std::pair<LWOOBJID, int64_t>> contributions;
		for (const auto& row : Database::Get()->GetChallengeContributions(challenge.id, 0)) contributions.emplace_back(row.characterId, row.amount);
		std::vector<LWOOBJID> rewarded;
		for (const auto& row : Database::Get()->GetChallengeRewards(challenge.id)) rewarded.push_back(row.characterId);
		const auto items = RewardItems(challenge);
		const bool anyReward = challenge.rewardCoins > 0 || !items.empty();

		uint32_t count = 0;
		for (const auto& recipient : LiveOpsRules::RewardRecipients(contributions, challenge.rewardMin, rewarded)) {
			const auto characterId = recipient.first;
			const auto amount = recipient.second;
			const auto info = Database::Get()->GetCharacterInfo(characterId);
			if (!info) continue; // deleted since
			Database::Get()->InsertChallengeReward({ challenge.id, characterId, amount, challenge.rewardCoins, now, challenge.rewardCoins > 0 ? 0 : now });
			count++;
			if (!anyReward) continue;
			const auto send = [&](LOT lot, int16_t itemCount, const std::string& body) {
				MailInfo mail;
				mail.senderId = LWOOBJID_EMPTY;
				mail.senderUsername = "Community Challenge";
				mail.receiverId = characterId;
				mail.recipient = info->name;
				mail.subject = Clip("Challenge complete: " + challenge.title, 50);
				mail.body = Clip(body, 400);
				mail.timeSent = static_cast<uint64_t>(now);
				mail.itemID = LWOOBJID_EMPTY;
				mail.itemSubkey = LWOOBJID_EMPTY;
				mail.itemLOT = lot;
				mail.itemCount = lot > 0 ? itemCount : 0;
				Database::Get()->InsertNewMail(mail);
			};
			const std::string thanks = "Together we reached " + std::to_string(challenge.target) + " " + MetricName(challenge) + ", and you added " + std::to_string(amount) + ". Thank you!";
			const std::string coins = challenge.rewardCoins > 0 ? " Your " + std::to_string(challenge.rewardCoins) + " coins are given to you in game (type /challenge if you don't see them)." : "";
			if (items.empty()) {
				send(0, 0, thanks + coins);
			} else {
				for (size_t i = 0; i < items.size(); i++) send(items[i].value("lot", 0), static_cast<int16_t>(items[i].value("count", 1)), thanks + (i == 0 ? coins : ""));
			}
		}
		challenge.rewardedAt = now;
		challenge.rewardedCount += count;
		Save(challenge, "[system]");
		Audit(SystemContext(), "challenge_rewards", Describe(challenge) + ": rewarded " + std::to_string(count) + " characters" +
			(anyReward ? " with " + RewardText(challenge) : "") + " (at least " + std::to_string(challenge.rewardMin) + " each)");
		if (count > 0) BroadcastTableChanged("mail");
	}

	/**
	 * Tell players in game that a challenge is complete or over. It only goes out while the dashboard is connected to
	 * the worlds (not in the first moments after it starts); end_announced_at records that it did, and Update() tries
	 * again until it does (LiveOpsRules::EndAnnouncementDue), so nobody is told twice or never. Save afterwards.
	 */
	void AnnounceEnd(Challenge& challenge, int64_t total) {
		bool sent = false;
		if (challenge.state == eState::COMPLETED) {
			const auto reward = RewardText(challenge);
			sent = LiveWorld::Announce("Challenge complete!", challenge.title + ": " + std::to_string(total) + " " + MetricName(challenge) + "!" +
				(reward.empty() ? " Thank you, everyone!" : " Everyone who helped gets " + reward + "."));
		} else if (challenge.state == eState::EXPIRED) {
			sent = LiveWorld::Announce("Challenge over", challenge.title + " ended at " + std::to_string(LiveOpsRules::Percent(total, challenge.target)) + "% (" +
				std::to_string(total) + " of " + std::to_string(challenge.target) + "). Thanks for trying!");
		}
		if (sent) challenge.endAnnouncedAt = Now();
		else LOG("Challenge %llu ended while the worlds can't be reached; it is announced once they can", static_cast<unsigned long long>(challenge.id));
	}

	// When a challenge ended, for EndAnnouncementDue: an expired one is last saved when it expires
	int64_t EndedAt(const Challenge& challenge) {
		return challenge.state == eState::COMPLETED ? challenge.completedAt : challenge.updatedAt;
	}

	void Complete(Challenge& challenge, int64_t total) {
		challenge.state = eState::COMPLETED;
		challenge.completedAt = Now();
		challenge.milestone = 100;
		AnnounceEnd(challenge, total);
		Save(challenge, "[system]");
		Audit(SystemContext(), "complete_challenge", Describe(challenge) + " with " + std::to_string(total));
		Alerts::Emit("server", "Community challenge complete", challenge.title + ": " + std::to_string(total) + " of " + std::to_string(challenge.target), {}, "/challenges");
		GiveRewards(challenge);
	}

	void Check(Challenge& challenge, const ILiveOps::ChallengeTotal& total, int64_t now) {
		if (now < challenge.startsAt) return;
		if (total.total >= challenge.target) {
			Complete(challenge, total.total);
			LiveEventRoutes::ReloadWorlds(); // stop counting, and hand out the coins to whoever is online
			return;
		}
		if (now >= challenge.endsAt) {
			challenge.state = eState::EXPIRED;
			AnnounceEnd(challenge, total.total);
			Save(challenge, "[system]");
			Audit(SystemContext(), "expire_challenge", Describe(challenge) + " at " + std::to_string(total.total));
			LiveEventRoutes::ReloadWorlds();
			return;
		}
		if (const auto milestone = LiveOpsRules::NextMilestone(Milestones(), challenge.milestone, total.total, challenge.target); milestone && *milestone < 100) {
			// Recorded only once it went out, so a milestone passed while the worlds can't be reached is announced later
			if (LiveWorld::Announce("Community challenge", challenge.title + " is " + std::to_string(*milestone) + "% done: " + std::to_string(total.total) + " of " +
				std::to_string(challenge.target) + " " + MetricName(challenge) + ". Type /challenge to see your part!")) {
				challenge.milestone = *milestone;
				Save(challenge, "[system]");
			}
		}
	}

	// Metrics to pick from, straight from the server's enums
	nlohmann::json MetricOptions() {
		std::set<int> recorded;
		try {
			const auto today = static_cast<uint32_t>(Now() / 86400);
			for (const auto& row : Database::Get()->GetPlayerStatsPerDay(today - 90, today, false, {})) recorded.insert(row.value("stat", 0));
		} catch (const std::exception&) {}
		nlohmann::json stats = nlohmann::json::array();
		for (const auto stat : magic_enum::enum_values<StatisticID>()) {
			stats.push_back({ {"value", static_cast<int>(stat)}, {"name", GameLabels::Name(stat)}, {"recorded", recorded.contains(static_cast<int>(stat))} });
		}
		nlohmann::json events = nlohmann::json::array();
		for (const auto kind : magic_enum::enum_values<IEconomyLedger::eMapEvent>()) events.push_back({ {"value", static_cast<int>(kind)}, {"name", GameLabels::Name(kind)} });
		return { {"stats", stats}, {"mapEvents", events} };
	}

	nlohmann::json ZoneOptions() {
		std::vector<std::pair<uint32_t, std::string>> zones;
		for (const auto& [id, name] : ZoneNames().items()) {
			if (const auto zone = GeneralUtils::TryParse<uint32_t>(id); zone && *zone > 0) zones.emplace_back(*zone, name.get<std::string>());
		}
		std::sort(zones.begin(), zones.end());
		nlohmann::json options = nlohmann::json::array();
		for (const auto& [id, name] : zones) options.push_back({ {"id", id}, {"name", name} });
		return options;
	}

	// A challenge from a request body into challenge (metric only when `metric`); error says what is wrong
	bool ReadBody(const nlohmann::json& body, Challenge& challenge, bool metric, std::string& error) {
		const auto fail = [&error](const std::string& message) { error = message; return false; };
		challenge.title = body.value("title", std::string{});
		challenge.description = body.value("description", std::string{});
		if (challenge.title.empty() || challenge.title.size() > MAX_TITLE) return fail("The title is needed (up to 100 characters)");
		if (challenge.description.size() > MAX_DESCRIPTION) return fail("The description is up to 400 characters");
		if (metric) {
			const auto kind = body.value("metricKind", std::string{ "stat" });
			const auto value = body.value("metric", -1);
			if (kind == "stat" && magic_enum::enum_cast<StatisticID>(value)) challenge.metricKind = eMetric::STATISTIC;
			else if (kind == "map" && value >= 0 && value <= 255 && magic_enum::enum_cast<IEconomyLedger::eMapEvent>(static_cast<uint8_t>(value))) challenge.metricKind = eMetric::MAP_EVENT;
			else return fail("Pick what to count from the list");
			challenge.metric = static_cast<uint32_t>(value);
			challenge.lot = challenge.metricKind == eMetric::MAP_EVENT ? body.value("lot", 0) : 0;
			if (challenge.lot < 0 || (challenge.lot > 0 && !ClientAssets::ObjectName(challenge.lot))) return fail("Unknown LOT");
			challenge.zones.clear();
			if (body.contains("zones") && body["zones"].is_array()) {
				const auto& known = ZoneNames();
				for (const auto& zone : body["zones"]) {
					if (!zone.is_number_unsigned() || !known.contains(std::to_string(zone.get<uint32_t>()))) return fail("Pick zones from the list");
					if (std::find(challenge.zones.begin(), challenge.zones.end(), zone.get<uint32_t>()) == challenge.zones.end()) challenge.zones.push_back(zone.get<uint32_t>());
				}
			}
			challenge.includeStaff = body.value("includeStaff", false);
		}
		challenge.target = body.value("target", int64_t{ 0 });
		if (challenge.target < 1 || challenge.target > MAX_TARGET) return fail("The target is a number from 1 up");
		challenge.startsAt = body.value("startsAt", int64_t{ 0 });
		challenge.endsAt = body.value("endsAt", int64_t{ 0 });
		if (challenge.startsAt <= 0) challenge.startsAt = Now();
		if (challenge.endsAt <= challenge.startsAt + 60 || challenge.endsAt - challenge.startsAt > 366 * 86400) return fail("It ends between a minute and a year after it starts");
		if (challenge.endsAt <= Now()) return fail("The end is in the past");
		challenge.isPublic = body.value("isPublic", true);
		challenge.rewardCoins = body.value("rewardCoins", int64_t{ 0 });
		if (challenge.rewardCoins < 0 || challenge.rewardCoins > 1'000'000) return fail("Coins: 0 to 1000000");
		challenge.rewardMin = body.value("rewardMin", int64_t{ 1 });
		if (challenge.rewardMin < 1) return fail("A reward needs at least 1 contributed");
		nlohmann::json items = nlohmann::json::array();
		if (body.contains("rewardItems") && body["rewardItems"].is_array()) {
			for (const auto& item : body["rewardItems"]) {
				const auto lot = item.is_object() ? item.value("lot", 0) : 0;
				const auto count = item.is_object() ? item.value("count", 1) : 0;
				if (lot <= 0 || !ClientAssets::ObjectName(lot)) return fail("Pick reward items from the list");
				if (count < 1 || count > 999) return fail("Item counts go from 1 to 999");
				items.push_back({ {"lot", lot}, {"count", count} });
			}
		}
		if (items.size() > MAX_REWARD_ITEMS) return fail("Up to 5 kinds of reward item");
		challenge.rewardItems = items.dump();
		return true;
	}

	std::optional<Challenge> FromPath(const HTTPContext& context, HTTPReply& reply) {
		const auto id = PathId<uint64_t>(context.path, 2);
		auto challenge = id ? Database::Get()->GetChallenge(*id) : std::nullopt;
		if (!challenge) JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Challenge not found");
		return challenge;
	}
}

namespace ChallengeRoutes {
	void Update() {
		const auto steady = std::chrono::steady_clock::now();
		if (steady < g_NextCheck) return;
		g_NextCheck = steady + CHECK_INTERVAL;
		try {
			const auto now = Now();
			auto open = Database::Get()->GetChallenges(true);
			std::vector<uint64_t> ids;
			for (const auto& challenge : open) ids.push_back(challenge.id);
			const auto totals = Database::Get()->GetChallengeTotals(ids);
			for (auto& challenge : open) {
				const auto it = totals.find(challenge.id);
				Check(challenge, it == totals.end() ? ILiveOps::ChallengeTotal{} : it->second, now);
			}
			std::vector<Challenge> unannounced;
			for (auto& challenge : Database::Get()->GetChallenges(false)) {
				// A completed challenge whose rewards were interrupted
				if (challenge.state == eState::COMPLETED && challenge.rewardedAt == 0) GiveRewards(challenge);
				// One that ended while the worlds couldn't be told (e.g. the dashboard was restarting)
				if (LiveOpsRules::EndAnnouncementDue(static_cast<uint8_t>(challenge.state), challenge.endAnnouncedAt, EndedAt(challenge), now)) unannounced.push_back(challenge);
			}
			if (!unannounced.empty()) {
				std::vector<uint64_t> unannouncedIds;
				for (const auto& challenge : unannounced) unannouncedIds.push_back(challenge.id);
				const auto endTotals = Database::Get()->GetChallengeTotals(unannouncedIds);
				for (auto& challenge : unannounced) {
					if (!Game::server || !Game::server->GetIsConnectedToMaster()) break;
					const auto it = endTotals.find(challenge.id);
					// Fresh from the database: GiveRewards above may have saved it since the list was read
					if (auto current = Database::Get()->GetChallenge(challenge.id); current && current->endAnnouncedAt == 0) {
						AnnounceEnd(*current, it == endTotals.end() ? 0 : it->second.total);
						if (current->endAnnouncedAt != 0) Save(*current, "[system]");
					}
				}
			}
		} catch (const std::exception& ex) {
			LOG("Challenges check failed: %s", ex.what());
		}
	}

	nlohmann::json PublicJson() {
		nlohmann::json rows = nlohmann::json::array();
		const auto now = Now();
		const auto challenges = Database::Get()->GetChallenges(false);
		std::vector<uint64_t> ids;
		for (const auto& challenge : challenges) ids.push_back(challenge.id);
		const auto totals = Database::Get()->GetChallengeTotals(ids);
		for (const auto& challenge : challenges) {
			const auto phase = PhaseOf(challenge, now);
			if (!challenge.isPublic) continue;
			if (phase != ePhase::ACTIVE && !(phase == ePhase::COMPLETED && now - challenge.completedAt < 7 * 86400)) continue;
			const auto it = totals.find(challenge.id);
			const auto total = it == totals.end() ? 0 : it->second.total;
			rows.push_back({ {"title", challenge.title}, {"description", challenge.description}, {"counting", MetricName(challenge)}, {"total", total},
				{"target", challenge.target}, {"percent", LiveOpsRules::Percent(total, challenge.target)}, {"ends_at", challenge.endsAt},
				{"completed", phase == ePhase::COMPLETED} });
		}
		return rows;
	}

	void RegisterRoutes() {
		Route(eHTTPMethod::GET, "/challenges", Perm("challenges_view"), "Community challenges page",
			[](HTTPReply& reply, const HTTPContext& context) { RenderPage(reply, context, "challenges.jinja2", "challenges"); });

		Route(eHTTPMethod::GET, "/api/challenges", Perm("challenges_view"),
			"Community challenges with their progress and what your own characters added; for staff who manage them, the metrics and zones to pick",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto now = Now();
				auto challenges = Database::Get()->GetChallenges(false);
				if (challenges.size() > 60) challenges.resize(60);
				std::vector<uint64_t> ids;
				for (const auto& challenge : challenges) ids.push_back(challenge.id);
				const auto totals = Database::Get()->GetChallengeTotals(ids);

				// What the viewer's own characters added
				std::map<uint64_t, nlohmann::json> mine;
				for (const auto& character : Database::Get()->GetAccountCharacters(context.accountId)) {
					const auto id = GeneralUtils::TryParse<LWOOBJID>(character.value("id", std::string{}));
					if (!id) continue;
					for (const auto& [challengeId, amount] : Database::Get()->GetCharacterContributions(*id)) {
						if (amount > 0) mine[challengeId].push_back({ {"name", character.value("name", std::string{})}, {"amount", amount} });
					}
				}

				const bool manage = Can(context, "challenges_manage");
				nlohmann::json rows = nlohmann::json::array();
				for (const auto& challenge : challenges) {
					if (!manage && PhaseOf(challenge, now) == ePhase::CANCELLED) continue;
					const auto it = totals.find(challenge.id);
					auto row = ChallengeJson(challenge, it == totals.end() ? ILiveOps::ChallengeTotal{} : it->second, now);
					row["mine"] = mine.contains(challenge.id) ? mine[challenge.id] : nlohmann::json::array();
					rows.push_back(std::move(row));
				}
				nlohmann::json result{ {"challenges", rows}, {"now", now}, {"canManage", manage} };
				if (manage) {
					result["metrics"] = MetricOptions();
					result["zones"] = ZoneOptions();
					result["milestones"] = Milestones();
				}
				JsonSuccess(reply, result);
			});

		Route(eHTTPMethod::GET, "/api/challenges/objects", Perm("challenges_manage"),
			"CDClient objects to pick for a challenge: reward items (kind=item) or the object a map event counts (kind=any). Query: kind, q (name or LOT)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const bool items = QueryValue(context.queryString, "kind") == "item";
				const auto query = QueryValue(context.queryString, "q");
				auto stmt = CDClientDatabase::CreatePreppedStmt(std::string("SELECT id, name, displayName, type FROM Objects WHERE ") + (items ? "type = 'Loot' AND " : "") +
					"(? = '' OR name LIKE '%' || ? || '%' OR displayName LIKE '%' || ? || '%' OR id = ?) ORDER BY name LIMIT 60;");
				stmt.bind(1, query.c_str());
				stmt.bind(2, query.c_str());
				stmt.bind(3, query.c_str());
				stmt.bind(4, GeneralUtils::TryParse<int>(query).value_or(-1));
				auto result = stmt.execQuery();
				nlohmann::json objects = nlohmann::json::array();
				for (; !result.eof(); result.nextRow()) {
					const std::string displayName = result.getStringField("displayName", "");
					objects.push_back({ {"lot", result.getIntField("id")}, {"name", displayName.empty() ? result.getStringField("name", "") : displayName},
						{"type", result.getStringField("type", "")} });
				}
				JsonSuccess(reply, { {"objects", objects} });
			});

		Route(eHTTPMethod::GET, "/api/challenges/:id", Perm("challenges_view"), "One challenge's top contributors (and for staff, who was rewarded)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto challenge = FromPath(context, reply);
				if (!challenge) return;
				nlohmann::json top = nlohmann::json::array();
				for (const auto& row : Database::Get()->GetChallengeContributions(challenge->id, 20)) {
					top.push_back({ {"characterId", std::to_string(row.characterId)}, {"name", row.name}, {"amount", row.amount},
						{"eligible", LiveOpsRules::Eligible(row.amount, challenge->rewardMin)} });
				}
				nlohmann::json result{ {"top", top} };
				if (Can(context, "challenges_manage")) {
					uint32_t waiting = 0;
					const auto rewards = Database::Get()->GetChallengeRewards(challenge->id);
					for (const auto& reward : rewards) if (reward.claimedAt == 0 && reward.coins > 0) waiting++;
					result["rewards"] = { {"count", rewards.size()}, {"coinsWaiting", waiting} };
				}
				JsonSuccess(reply, result);
			});

		Route(eHTTPMethod::POST, "/api/challenges", Perm("challenges_manage"),
			"Create a challenge. Body: {title, description, metricKind: stat|map, metric (StatisticID or eMapEvent value), lot (map events, 0: any), "
			"zones: [zone IDs] (empty: all), target, startsAt, endsAt (unix), includeStaff, isPublic, rewardCoins, rewardItems: [{lot, count}], rewardMin}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				Challenge challenge;
				std::string error;
				if (!ReadBody(*body, challenge, true, error)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, error);
				challenge.createdAt = challenge.updatedAt = Now();
				challenge.createdBy = challenge.updatedBy = context.authenticatedUser;
				challenge.id = Database::Get()->InsertChallenge(challenge);
				Audit(context, "create_challenge", Describe(challenge) + ", reward: " + (RewardText(challenge).empty() ? "none" : RewardText(challenge)));
				LiveEventRoutes::ReloadWorlds();
				if (challenge.startsAt <= Now()) {
					LiveWorld::Announce("New community challenge", challenge.title + ": " + std::to_string(challenge.target) + " " + MetricName(challenge) +
						" together! " + (challenge.description.empty() ? "" : challenge.description + " ") + "Type /challenge to follow it.");
				}
				BroadcastTableChanged("challenges", std::to_string(challenge.id));
				JsonSuccess(reply, { {"message", "Challenge created"}, {"id", challenge.id} });
			});

		Route(eHTTPMethod::POST, "/api/challenges/:id", Perm("challenges_manage"),
			"Change an open challenge's text, target, times, visibility and rewards (what it counts stays). Body as when creating one",
			[](HTTPReply& reply, const HTTPContext& context) {
				auto challenge = FromPath(context, reply);
				if (!challenge) return;
				if (challenge->state != eState::OPEN) return JsonError(reply, eHTTPStatusCode::CONFLICT, "Only open challenges can be changed");
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				std::string error;
				if (!ReadBody(*body, *challenge, false, error)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, error);
				Save(*challenge, context.authenticatedUser);
				Audit(context, "update_challenge", Describe(*challenge));
				LiveEventRoutes::ReloadWorlds();
				JsonSuccess(reply, { {"message", "Saved"} });
			});

		Route(eHTTPMethod::POST, "/api/challenges/:id/cancel", Perm("challenges_manage"), "Stop an open challenge without rewards",
			[](HTTPReply& reply, const HTTPContext& context) {
				auto challenge = FromPath(context, reply);
				if (!challenge) return;
				if (challenge->state != eState::OPEN) return JsonError(reply, eHTTPStatusCode::CONFLICT, "It isn't open");
				challenge->state = eState::CANCELLED;
				Save(*challenge, context.authenticatedUser);
				Audit(context, "cancel_challenge", Describe(*challenge));
				LiveEventRoutes::ReloadWorlds();
				JsonSuccess(reply, { {"message", "Cancelled"} });
			});

		Route(eHTTPMethod::POST, "/api/challenges/:id/delete", Perm("challenges_manage"),
			"Delete a challenge that is no longer open, with its contributions and reward records (mail already sent stays)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto challenge = FromPath(context, reply);
				if (!challenge) return;
				if (challenge->state == eState::OPEN) return JsonError(reply, eHTTPStatusCode::CONFLICT, "Cancel it first");
				if (challenge->state == eState::COMPLETED && challenge->rewardedAt == 0) return JsonError(reply, eHTTPStatusCode::CONFLICT, "Its rewards are still being handed out");
				Database::Get()->DeleteChallenge(challenge->id);
				Audit(context, "delete_challenge", Describe(*challenge));
				BroadcastTableChanged("challenges", std::to_string(challenge->id));
				JsonSuccess(reply, { {"message", "Deleted"} });
			});
	}
}
