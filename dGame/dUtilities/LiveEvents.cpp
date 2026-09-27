#include "LiveEvents.h"

#include <chrono>
#include <cstdio>
#include <functional>
#include <cmath>
#include <ctime>
#include <map>
#include <set>

#include "json.hpp"

#include "Database.h"
#include "Game.h"
#include "Logger.h"
#include "dServer.h"
#include "dZoneManager.h"
#include "Zone.h"
#include "dpWorld.h"
#include "dNavMesh.h"
#include "Entity.h"
#include "EntityInfo.h"
#include "EntityManager.h"
#include "PlayerManager.h"
#include "Character.h"
#include "User.h"
#include "InventoryComponent.h"
#include "DestroyableComponent.h"
#include "GameMessages.h"
#include "EffectsMessages.h"
#include "ChatPackets.h"
#include "DashboardNotify.h"
#include "CDClientManager.h"
#include "CDObjectsTable.h"
#include "GeneralUtils.h"
#include "eGameMasterLevel.h"
#include "eLootSourceType.h"

using LiveOpsRules::eEventType;

namespace LiveEvents::Detail {
	LiveOpsRules::Multipliers g_Multipliers;
	bool g_Counting = false;
}

namespace {
	using Clock = std::chrono::steady_clock;
	constexpr auto TICK = std::chrono::milliseconds(250);
	constexpr auto FIRST_LOAD_DELAY = std::chrono::seconds(5); // let the zone's own objects load first
	constexpr auto FLUSH_INTERVAL = std::chrono::seconds(5);
	constexpr auto STATUS_INTERVAL = std::chrono::seconds(2);

	int64_t Now() { return static_cast<int64_t>(std::time(nullptr)); }

	struct Treasure {
		LWOOBJID id{};
		NiPoint3 position;
		bool gone{};
	};

	struct Running {
		ILiveOps::LiveEvent row;
		eEventType type{};
		nlohmann::json config;
		int64_t startedAt{};
		// Treasure hunt
		LOT lot{};
		std::vector<Treasure> treasures;
		uint32_t found{};
		float pickupRadius{ 4.0f };
		// Invasion
		NiPoint3 center;
		uint32_t wavesSent{};
		uint32_t spawned{};
		uint32_t kills{};
		std::set<LWOOBJID> alive;
		// Celebration
		int64_t nextBurst{};
		uint32_t bursts{};
		// Scores not written yet, and whether the instance status changed since it was written
		std::map<LWOOBJID, int64_t> scores;
		bool dirty{ true };
	};

	struct TrackedChallenge {
		uint64_t id{};
		ILiveOps::eChallengeMetric kind{};
		uint32_t metric{};
		LOT lot{};
		int64_t startsAt{};
		int64_t endsAt{};
		bool includeStaff{};
	};

	bool g_Disabled = false;
	bool g_Loaded = false;
	Clock::time_point g_FirstUpdate{};
	Clock::time_point g_NextTick{};
	Clock::time_point g_NextFlush{};
	Clock::time_point g_NextStatus{};
	std::map<uint64_t, Running> g_Running;
	std::vector<TrackedChallenge> g_Challenges;
	std::map<std::pair<uint64_t, LWOOBJID>, int64_t> g_Contributions;

	uint32_t ZoneId() { return Game::server ? Game::server->GetZoneID() : 0; }
	uint32_t InstanceId() { return Game::server ? static_cast<uint32_t>(Game::server->GetInstanceID()) : 0; }
	bool IsClone() { return Game::zoneManager && Game::zoneManager->GetZoneID().GetCloneID() != 0; }

	bool IsStaff(const Entity* player) {
		const auto* character = player ? player->GetCharacter() : nullptr;
		const auto* user = character ? character->GetParentUser() : nullptr;
		return user && user->GetMaxGMLevel() >= eGameMasterLevel::MODERATOR;
	}

	void TellAll(const std::string& text) {
		ChatPackets::SendSystemMessage(UNASSIGNED_SYSTEM_ADDRESS, GeneralUtils::UTF8ToUTF16(text), true);
	}

	void Tell(const SystemAddress& sysAddr, const std::string& text) {
		ChatPackets::SendSystemMessage(sysAddr, GeneralUtils::UTF8ToUTF16(text));
	}

	// 2 -> "2", 1.5 -> "1.5"
	std::string Times(float multiplier) {
		char text[16];
		std::snprintf(text, sizeof(text), "%g", std::round(multiplier * 100.0f) / 100.0f);
		return text;
	}

	std::string TimeLeft(int64_t seconds) {
		if (seconds >= 2 * 86400) return std::to_string(seconds / 86400) + " days";
		if (seconds >= 2 * 3600) return std::to_string(seconds / 3600) + " hours";
		if (seconds >= 120) return std::to_string(seconds / 60) + " minutes";
		return std::to_string(std::max<int64_t>(seconds, 0)) + " seconds";
	}

	// Whether an event is for this world: its zone (or every zone for bonuses), its instance, and not a property
	bool AppliesHere(const ILiveOps::LiveEvent& row, eEventType type, int64_t now) {
		if (!LiveOpsRules::InWindow(row.startsAt, row.endsAt, now)) return false;
		if (row.instanceId >= 0 && static_cast<uint32_t>(row.instanceId) != InstanceId()) return false;
		if (type != eEventType::BONUS && IsClone()) return false;
		if (row.zones.empty()) return type == eEventType::BONUS;
		return LiveOpsRules::CountsInZone(row.zones, ZoneId());
	}

	/**
	 * Places known to be on walkable ground: the waypoints of the zone's movement paths (where NPCs walk), where the
	 * zone's enemies spawn and the zone's spawn point. With a navmesh they are moved onto it.
	 */
	std::vector<NiPoint3> WalkablePoints() {
		std::vector<NiPoint3> points;
		if (!Game::zoneManager) return points;
		if (const auto* zone = Game::zoneManager->GetZone()) {
			for (const auto& path : zone->GetPaths()) {
				if (path.pathType != PathType::Movement) continue;
				for (const auto& waypoint : path.pathWaypoints) points.push_back(waypoint.position);
			}
			points.push_back(zone->GetSpawnPos());
		}
		auto* objects = CDClientManager::GetTable<CDObjectsTable>();
		for (const auto& [id, spawner] : Game::zoneManager->GetSpawners()) {
			if (!spawner || objects->GetByID(spawner->m_Info.templateID).type != "Enemies") continue;
			for (const auto* node : spawner->m_Info.nodes) if (node) points.push_back(node->position);
		}
		if (auto* navMesh = dpWorld::GetNavMesh(); navMesh && navMesh->IsNavmeshLoaded()) {
			for (auto& point : points) point = navMesh->NearestPoint(point, 3.0f);
		}
		return points;
	}

	Entity* SpawnAt(LOT lot, const NiPoint3& position) {
		auto* control = Game::entityManager->GetZoneControlEntity();
		EntityInfo info;
		info.lot = lot;
		info.pos = position;
		info.rot = QuatUtils::FromEuler(NiPoint3(0.0f, GeneralUtils::GenerateRandomNumber<float>(0.0f, 6.28f), 0.0f));
		info.spawnerID = control ? control->GetObjectID() : LWOOBJID_EMPTY;
		info.settings.Insert<bool>(u"SpawnedFromLiveEvent", true);
		auto* entity = Game::entityManager->CreateEntity(info);
		if (!entity) return nullptr;
		Game::entityManager->ConstructEntity(entity);
		return entity;
	}

	// Who smashed an entity that is dying: the player, or the player owning what did it
	Entity* KillerPlayer(Entity* victim) {
		const auto* destroyable = victim ? victim->GetComponent<DestroyableComponent>() : nullptr;
		auto* killer = destroyable ? Game::entityManager->GetEntity(destroyable->GetKillerID()) : nullptr;
		if (killer) killer = killer->GetOwner();
		return killer && killer->IsPlayer() ? killer : nullptr;
	}

	void AddScore(Running& event, const Entity* player, int64_t amount) {
		const auto* character = player ? player->GetCharacter() : nullptr;
		if (!character) return;
		event.scores[character->GetID()] += amount;
	}

	std::string PlayerName(const Entity* player) {
		const auto* character = player ? player->GetCharacter() : nullptr;
		return character ? character->GetName() : "Someone";
	}

	// ---------- Treasure hunt ----------

	void TreasureFound(Running& event, Treasure& treasure, Entity* player, bool destroy) {
		if (treasure.gone) return;
		treasure.gone = true;
		event.dirty = true;
		if (destroy) Game::entityManager->DestroyEntity(treasure.id);
		if (!player) return; // smashed by something else: lost
		event.found++;
		AddScore(event, player, 1);
		auto* character = player->GetCharacter();
		const auto coins = std::clamp<int64_t>(event.config.value("coins", int64_t{ 0 }), 0, 100000);
		// Not ACTIVITY: the client holds ACTIVITY coins back for an activity's summary, so the HUD wouldn't show them until the next login
		if (character && coins > 0) character->SetCoins(character->GetCoins() + coins, eLootSourceType::CURRENCY);
		const LOT item = event.config.value("itemLot", 0);
		const auto itemCount = std::clamp<uint32_t>(event.config.value("itemCount", 1u), 1, 999);
		if (auto* inventory = player->GetComponent<InventoryComponent>(); inventory && item > 0) inventory->AddItem(item, itemCount, eLootSourceType::ACTIVITY);
		if (const auto effect = event.config.value("foundEffectId", 0); effect > 0) {
			GameMessages::PlayFXEffect(player->GetObjectID(), effect, GeneralUtils::UTF8ToUTF16(event.config.value("foundEffectType", std::string{})), "live_event_found").Send(UNASSIGNED_SYSTEM_ADDRESS);
		}
		const auto left = std::ranges::count_if(event.treasures, [](const Treasure& t) { return !t.gone; });
		TellAll(PlayerName(player) + " found a treasure! " + (left == 0 ? "That was the last one." : std::to_string(left) + " left to find."));
		if (left == 0) DashboardNotify::Announce(event.row.title.empty() ? "Treasure hunt" : event.row.title, "Every treasure here has been found. Well done!");
	}

	void StartTreasureHunt(Running& event) {
		event.lot = event.config.value("lot", 0);
		event.pickupRadius = std::clamp(event.config.value("radius", 4.0f), 2.0f, 15.0f);
		const auto count = std::clamp<uint32_t>(event.config.value("count", 10u), 1, LiveOpsRules::MAX_TREASURES);
		if (event.lot <= 0) return;
		LiveOpsRules::Placement placement{ .count = count, .minSpacing = 30.0f };
		for (const auto& position : LiveOpsRules::ChoosePositions(WalkablePoints(), placement, Game::randomEngine)) {
			auto* entity = SpawnAt(event.lot, position);
			if (!entity) continue;
			entity->SetIsGhostingCandidate(false); // seen from afar, so players can hunt for them
			event.treasures.push_back({ entity->GetObjectID(), position });
			// A smashable treasure counts for whoever smashes it
			const auto eventId = event.row.id;
			const auto objectId = entity->GetObjectID();
			entity->AddDieCallback([eventId, objectId, entity]() {
				auto it = g_Running.find(eventId);
				if (it == g_Running.end()) return;
				for (auto& treasure : it->second.treasures) {
					if (treasure.id == objectId) TreasureFound(it->second, treasure, KillerPlayer(entity), false);
				}
			});
		}
		LOG("Live event %llu: hid %zu treasures (LOT %i)", static_cast<unsigned long long>(event.row.id), event.treasures.size(), event.lot);
	}

	void TickTreasureHunt(Running& event) {
		const auto radiusSq = event.pickupRadius * event.pickupRadius;
		for (auto* player : PlayerManager::GetAllPlayers()) {
			if (!player || player->GetIsDead()) continue;
			const auto position = player->GetPosition();
			for (auto& treasure : event.treasures) {
				if (!treasure.gone && NiPoint3::DistanceSquared(position, treasure.position) <= radiusSq) TreasureFound(event, treasure, player, true);
			}
		}
	}

	// ---------- Invasion ----------

	void StartInvasion(Running& event) {
		const auto* zone = Game::zoneManager->GetZone();
		event.center = zone ? zone->GetSpawnPos() : NiPoint3Constant::ZERO;
		if (event.config.value("center", std::string{ "spawn" }) == "players") {
			const auto& players = PlayerManager::GetAllPlayers();
			if (!players.empty()) {
				const auto* player = players[GeneralUtils::GenerateRandomNumber<size_t>(0, players.size() - 1)];
				if (player) event.center = player->GetPosition();
			}
		}
	}

	std::vector<NiPoint3> InvasionPoints(const Running& event, float radius) {
		auto points = WalkablePoints();
		// With a navmesh, points on a ring around the center, moved onto it (dropped where there is none)
		if (auto* navMesh = dpWorld::GetNavMesh(); navMesh && navMesh->IsNavmeshLoaded()) {
			for (int i = 0; i < 24; i++) {
				const auto angle = static_cast<float>(i) * 0.2618f;
				const auto distance = radius * (0.4f + 0.6f * GeneralUtils::GenerateRandomNumber<float>(0.0f, 1.0f));
				const NiPoint3 ring{ event.center.x + std::cos(angle) * distance, event.center.y, event.center.z + std::sin(angle) * distance };
				const auto snapped = navMesh->NearestPoint(ring, 8.0f);
				if (snapped != ring) points.push_back(snapped);
			}
		}
		points.push_back(event.center);
		return points;
	}

	void SendWave(Running& event) {
		std::vector<LOT> lots;
		if (event.config.contains("lots") && event.config["lots"].is_array()) {
			for (const auto& lot : event.config["lots"]) if (lot.is_number_integer() && lot.get<int32_t>() > 0) lots.push_back(lot.get<int32_t>());
		}
		event.wavesSent++;
		event.dirty = true;
		if (lots.empty()) return;
		const auto perWave = std::clamp<uint32_t>(event.config.value("perWave", 5u), 1, LiveOpsRules::MAX_PER_WAVE);
		const auto room = LiveOpsRules::MAX_INVADERS_ALIVE > event.alive.size() ? LiveOpsRules::MAX_INVADERS_ALIVE - static_cast<uint32_t>(event.alive.size()) : 0;
		const auto count = std::min(perWave, room);
		const auto radius = std::clamp(event.config.value("radius", 25.0f), 5.0f, 80.0f);
		LiveOpsRules::Placement placement{ .count = count, .minSpacing = 3.0f, .center = event.center, .radius = radius, .reuse = true };
		auto positions = LiveOpsRules::ChoosePositions(InvasionPoints(event, radius), placement, Game::randomEngine);
		for (const auto& position : positions) {
			auto* entity = SpawnAt(lots[GeneralUtils::GenerateRandomNumber<size_t>(0, lots.size() - 1)], position);
			if (!entity) continue;
			event.alive.insert(entity->GetObjectID());
			event.spawned++;
			const auto eventId = event.row.id;
			const auto objectId = entity->GetObjectID();
			entity->AddDieCallback([eventId, objectId, entity]() {
				auto it = g_Running.find(eventId);
				if (it == g_Running.end() || !it->second.alive.erase(objectId)) return;
				auto& running = it->second;
				running.dirty = true;
				if (auto* player = KillerPlayer(entity)) {
					running.kills++;
					AddScore(running, player, 1);
				}
			});
		}
		const auto waves = std::clamp<uint32_t>(event.config.value("waves", 3u), 1, LiveOpsRules::MAX_WAVES);
		TellAll("Invasion wave " + std::to_string(event.wavesSent) + " of " + std::to_string(waves) + ": " + std::to_string(positions.size()) + " enemies are attacking!");
	}

	void TickInvasion(Running& event, int64_t now) {
		// Invaders removed some other way than being smashed
		const auto before = event.alive.size();
		std::erase_if(event.alive, [](LWOOBJID id) { return Game::entityManager->GetEntity(id) == nullptr; });
		if (event.alive.size() != before) event.dirty = true;

		const auto waves = std::clamp<uint32_t>(event.config.value("waves", 3u), 1, LiveOpsRules::MAX_WAVES);
		const auto interval = std::clamp<int64_t>(event.config.value("interval", int64_t{ 90 }), 15, 3600);
		const auto due = LiveOpsRules::WavesDue(event.startedAt, interval, waves, now);
		const bool wasOpen = event.wavesSent < waves || !event.alive.empty();
		while (event.wavesSent < due) SendWave(event);
		if (wasOpen && event.wavesSent >= waves && event.alive.empty() && event.spawned > 0) {
			DashboardNotify::Announce(event.row.title.empty() ? "Invasion" : event.row.title, "The invasion has been pushed back! " + std::to_string(event.kills) + " invaders smashed.");
		}
	}

	// ---------- Celebration ----------

	void TickCelebration(Running& event, int64_t now) {
		if (now < event.nextBurst) return;
		event.nextBurst = now + std::clamp<int64_t>(event.config.value("interval", int64_t{ 10 }), 3, 600);
		const auto effect = event.config.value("effectId", 0);
		const auto type = GeneralUtils::UTF8ToUTF16(event.config.value("effectType", std::string{}));
		if (effect <= 0) return;
		const auto name = "live_event_" + std::to_string(event.row.id);
		for (auto* player : PlayerManager::GetAllPlayers()) {
			if (player) GameMessages::PlayFXEffect(player->GetObjectID(), effect, type, name).Send(UNASSIGNED_SYSTEM_ADDRESS);
		}
		event.bursts++;
		event.dirty = true;
	}

	// ---------- Common ----------

	nlohmann::json StatusJson(const Running& event, bool closed) {
		nlohmann::json status{ {"players", PlayerManager::GetAllPlayers().size()}, {"closed", closed} };
		switch (event.type) {
		case eEventType::TREASURE_HUNT:
			status["treasures"] = event.treasures.size();
			status["found"] = event.found;
			status["left"] = std::ranges::count_if(event.treasures, [](const Treasure& t) { return !t.gone; });
			break;
		case eEventType::INVASION:
			status["wavesSent"] = event.wavesSent;
			status["spawned"] = event.spawned;
			status["alive"] = event.alive.size();
			status["kills"] = event.kills;
			break;
		case eEventType::CELEBRATION:
			status["bursts"] = event.bursts;
			break;
		case eEventType::BONUS:
			break;
		}
		return status;
	}

	void WriteStatus(Running& event, bool closed) {
		event.dirty = false;
		try {
			Database::Get()->SetLiveEventInstance({ event.row.id, ZoneId(), InstanceId(), StatusJson(event, closed).dump(), Now() });
			DashboardNotify::Changed("live_events", static_cast<LWOOBJID>(event.row.id));
		} catch (const std::exception& ex) {
			LOG("Could not write live event %llu's status: %s", static_cast<unsigned long long>(event.row.id), ex.what());
		}
	}

	void FlushScores(Running& event) {
		if (event.scores.empty()) return;
		std::vector<std::pair<LWOOBJID, int64_t>> scores(event.scores.begin(), event.scores.end());
		event.scores.clear();
		try {
			Database::Get()->AddLiveEventScores(event.row.id, scores, Now());
			DashboardNotify::Changed("live_events", static_cast<LWOOBJID>(event.row.id));
		} catch (const std::exception& ex) {
			LOG("Could not write live event %llu's scores: %s", static_cast<unsigned long long>(event.row.id), ex.what());
		}
	}

	void FlushContributions() {
		if (g_Contributions.empty()) return;
		std::vector<ILiveOps::Contribution> rows;
		for (const auto& [key, amount] : g_Contributions) rows.push_back({ key.first, key.second, amount });
		g_Contributions.clear();
		try {
			Database::Get()->AddChallengeContributions(rows, Now());
			DashboardNotify::Changed("challenges");
		} catch (const std::exception& ex) {
			LOG("Could not write challenge contributions: %s", ex.what());
		}
	}

	void RecomputeMultipliers(int64_t now) {
		std::vector<LiveOpsRules::BonusWindow> windows;
		for (const auto& [id, event] : g_Running) {
			if (event.type != eEventType::BONUS) continue;
			windows.push_back({ event.row.startsAt, event.row.endsAt,
				{ event.config.value("coins", 1.0f), event.config.value("uscore", 1.0f), event.config.value("lootChance", 1.0f) } });
		}
		LiveEvents::Detail::g_Multipliers = LiveOpsRules::ActiveMultipliers(windows, now);
	}

	void RecomputeCounting(int64_t now) {
		std::erase_if(g_Challenges, [now](const TrackedChallenge& challenge) { return now >= challenge.endsAt; });
		LiveEvents::Detail::g_Counting = std::ranges::any_of(g_Challenges, [now](const TrackedChallenge& c) { return LiveOpsRules::InWindow(c.startsAt, c.endsAt, now); });
	}

	void Start(Running& event) {
		try {
			switch (event.type) {
			case eEventType::TREASURE_HUNT: StartTreasureHunt(event); break;
			case eEventType::INVASION: StartInvasion(event); break;
			case eEventType::CELEBRATION: event.nextBurst = event.startedAt; break;
			case eEventType::BONUS: break;
			}
		} catch (const std::exception& ex) {
			LOG("Live event %llu could not start: %s", static_cast<unsigned long long>(event.row.id), ex.what());
		}
		LOG("Live event %llu (%s) running in zone %u instance %u until %lld", static_cast<unsigned long long>(event.row.id), event.row.type.c_str(),
			ZoneId(), InstanceId(), static_cast<long long>(event.row.endsAt));
	}

	// Remove everything the event put in this world
	void Stop(Running& event, bool timeUp) {
		for (const auto& treasure : event.treasures) {
			if (!treasure.gone) Game::entityManager->DestroyEntity(treasure.id);
		}
		for (const auto id : event.alive) Game::entityManager->DestroyEntity(id);
		event.alive.clear();
		if (event.type == eEventType::CELEBRATION) {
			const auto name = "live_event_" + std::to_string(event.row.id);
			for (auto* player : PlayerManager::GetAllPlayers()) if (player) GameMessages::StopFXEffect(player->GetObjectID(), true, name).Send(UNASSIGNED_SYSTEM_ADDRESS);
		}
		FlushScores(event);
		WriteStatus(event, true);
		switch (event.type) {
		case eEventType::TREASURE_HUNT:
			TellAll("The treasure hunt is over: " + std::to_string(event.found) + " of " + std::to_string(event.treasures.size()) + " treasures were found here.");
			break;
		case eEventType::INVASION:
			TellAll("The invasion is over: " + std::to_string(event.kills) + " invaders smashed here." + (timeUp ? "" : " The rest retreated."));
			break;
		default:
			break;
		}
		LOG("Live event %llu stopped in zone %u instance %u", static_cast<unsigned long long>(event.row.id), ZoneId(), InstanceId());
	}

	void StopAll(const std::function<bool(const Running&)>& shouldStop, bool timeUp) {
		for (auto it = g_Running.begin(); it != g_Running.end();) {
			if (!shouldStop(it->second)) { ++it; continue; }
			Stop(it->second, timeUp);
			it = g_Running.erase(it);
		}
	}

	// Give online players the challenge coins waiting for them
	uint32_t GiveWaitingCoins(Entity* player) {
		auto* character = player ? player->GetCharacter() : nullptr;
		if (!character) return 0;
		int64_t given = 0;
		for (const auto& reward : Database::Get()->GetUnclaimedChallengeCoins(character->GetID())) {
			if (!Database::Get()->ClaimChallengeCoins(reward.challengeId, reward.characterId, Now())) continue;
			given += reward.coins;
		}
		if (given <= 0) return 0;
		character->SetCoins(character->GetCoins() + given, eLootSourceType::CURRENCY); // not ACTIVITY, see TreasureFound
		Tell(player->GetSystemAddress(), "You received " + std::to_string(given) + " coins for your part in a community challenge. Thank you!");
		return 1;
	}
}

namespace LiveEvents::Detail {
	void CountStat(const Entity* player, uint32_t stat, int64_t amount) {
		const auto* character = player ? player->GetCharacter() : nullptr;
		if (!character || amount <= 0) return;
		const auto now = Now();
		const bool staff = IsStaff(player);
		for (const auto& challenge : g_Challenges) {
			if (challenge.kind != ILiveOps::eChallengeMetric::STATISTIC || challenge.metric != stat) continue;
			if ((staff && !challenge.includeStaff) || !LiveOpsRules::InWindow(challenge.startsAt, challenge.endsAt, now)) continue;
			g_Contributions[{ challenge.id, character->GetID() }] += amount;
		}
	}

	void CountMapEvent(const Entity* player, uint8_t kind, LOT lot, int64_t quantity) {
		const auto* character = player->GetCharacter();
		if (!character || quantity <= 0) return;
		const auto now = Now();
		const bool staff = IsStaff(player);
		for (const auto& challenge : g_Challenges) {
			if (challenge.kind != ILiveOps::eChallengeMetric::MAP_EVENT || challenge.metric != kind || (challenge.lot != 0 && challenge.lot != lot)) continue;
			if ((staff && !challenge.includeStaff) || !LiveOpsRules::InWindow(challenge.startsAt, challenge.endsAt, now)) continue;
			g_Contributions[{ challenge.id, character->GetID() }] += quantity;
		}
	}
}

namespace LiveEvents {
	uint32_t Reload() {
		if (!Game::server || ZoneId() == 0 || !Game::zoneManager || !Game::entityManager) return 0;
		g_Loaded = true;
		const auto now = Now();
		std::vector<ILiveOps::LiveEvent> events;
		std::vector<ILiveOps::Challenge> challenges;
		try {
			events = Database::Get()->GetLiveEvents(true, 200);
			challenges = Database::Get()->GetChallenges(true);
		} catch (const std::exception& ex) {
			LOG("Could not load live events: %s", ex.what());
			return 0;
		}

		std::map<uint64_t, std::pair<ILiveOps::LiveEvent, eEventType>> wanted;
		for (auto& row : events) {
			const auto type = LiveOpsRules::ParseType(row.type);
			if (type && AppliesHere(row, *type, now)) wanted[row.id] = { std::move(row), *type };
		}
		StopAll([&wanted](const Running& event) { return !wanted.contains(event.row.id); }, false);
		for (auto& [id, entry] : wanted) {
			if (g_Running.contains(id)) {
				g_Running[id].row.endsAt = entry.first.endsAt; // may have been ended early by changing the end
				continue;
			}
			auto& event = g_Running[id];
			event.row = std::move(entry.first);
			event.type = entry.second;
			event.config = nlohmann::json::parse(event.row.config, nullptr, false);
			if (!event.config.is_object()) event.config = nlohmann::json::object();
			event.startedAt = now;
			Start(event);
		}
		RecomputeMultipliers(now);

		g_Challenges.clear();
		for (const auto& challenge : challenges) {
			if (now >= challenge.endsAt || !LiveOpsRules::CountsInZone(challenge.zones, ZoneId())) continue;
			g_Challenges.push_back({ challenge.id, challenge.metricKind, challenge.metric, challenge.lot, challenge.startsAt, challenge.endsAt, challenge.includeStaff });
		}
		RecomputeCounting(now);

		try {
			for (auto* player : PlayerManager::GetAllPlayers()) GiveWaitingCoins(player);
		} catch (const std::exception& ex) {
			LOG("Could not give challenge coins: %s", ex.what());
		}
		return 1;
	}

	void Update() {
		if (g_Disabled) return;
		const auto steady = Clock::now();
		if (steady < g_NextTick) return;
		g_NextTick = steady + TICK;
		if (!g_Loaded) {
			if (!Game::server || ZoneId() == 0) { g_Disabled = true; return; }
			if (g_FirstUpdate == Clock::time_point{}) g_FirstUpdate = steady;
			if (steady - g_FirstUpdate < FIRST_LOAD_DELAY) return;
			Reload();
			if (!g_Loaded) { g_Disabled = true; return; }
		}
		if (g_Running.empty() && g_Challenges.empty() && g_Contributions.empty()) return;

		const auto now = Now();
		StopAll([now](const Running& event) { return now >= event.row.endsAt; }, true);
		for (auto& [id, event] : g_Running) {
			switch (event.type) {
			case eEventType::TREASURE_HUNT: TickTreasureHunt(event); break;
			case eEventType::INVASION: TickInvasion(event, now); break;
			case eEventType::CELEBRATION: TickCelebration(event, now); break;
			case eEventType::BONUS: break;
			}
		}
		RecomputeMultipliers(now);
		RecomputeCounting(now);

		if (steady >= g_NextStatus) {
			g_NextStatus = steady + STATUS_INTERVAL;
			for (auto& [id, event] : g_Running) if (event.dirty) WriteStatus(event, false);
		}
		if (steady >= g_NextFlush) {
			g_NextFlush = steady + FLUSH_INTERVAL;
			for (auto& [id, event] : g_Running) FlushScores(event);
			FlushContributions();
		}
	}

	void Shutdown() {
		for (auto& [id, event] : g_Running) {
			FlushScores(event);
			WriteStatus(event, true);
		}
		g_Running.clear();
		FlushContributions();
		Detail::g_Multipliers = {};
		Detail::g_Counting = false;
	}

	void ChallengeCommand(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
		auto* character = entity ? entity->GetCharacter() : nullptr;
		if (!character) return;
		try {
			if (GiveWaitingCoins(entity) == 0 && args == "claim") Tell(sysAddr, "You have no challenge rewards waiting.");

			const auto now = Now();
			auto challenges = Database::Get()->GetChallenges(true);
			std::erase_if(challenges, [now](const ILiveOps::Challenge& c) { return LiveOpsRules::Phase(0, c.startsAt, c.endsAt, now) != LiveOpsRules::ePhase::ACTIVE; });
			std::vector<uint64_t> ids;
			for (const auto& challenge : challenges) ids.push_back(challenge.id);
			const auto totals = Database::Get()->GetChallengeTotals(ids);
			auto mine = Database::Get()->GetCharacterContributions(character->GetID());
			for (const auto& [key, amount] : g_Contributions) if (key.second == character->GetID()) mine[key.first] += amount;

			if (challenges.empty()) Tell(sysAddr, "There are no community challenges running right now.");
			for (const auto& challenge : challenges) {
				const auto it = totals.find(challenge.id);
				const auto total = it == totals.end() ? 0 : it->second.total;
				const auto contributed = mine.contains(challenge.id) ? mine[challenge.id] : 0;
				Tell(sysAddr, "Challenge: " + challenge.title + " - " + std::to_string(total) + " of " + std::to_string(challenge.target) + " (" +
					std::to_string(LiveOpsRules::Percent(total, challenge.target)) + "%), " + TimeLeft(challenge.endsAt - now) + " left. You: " + std::to_string(contributed) +
					(contributed > 0 && !LiveOpsRules::Eligible(contributed, challenge.rewardMin) ? " (" + std::to_string(challenge.rewardMin) + " needed for the reward)" : ""));
			}

			const auto position = entity->GetPosition();
			for (const auto& [id, event] : g_Running) {
				std::string text = "Live event here: " + (event.row.title.empty() ? std::string(LiveOpsRules::TypeName(event.type)) : event.row.title) + ", " + TimeLeft(event.row.endsAt - now) + " left.";
				if (event.type == eEventType::TREASURE_HUNT) {
					float nearest = -1.0f;
					size_t left = 0;
					for (const auto& treasure : event.treasures) {
						if (treasure.gone) continue;
						left++;
						const auto distance = NiPoint3::Distance(position, treasure.position);
						if (nearest < 0 || distance < nearest) nearest = distance;
					}
					text += " " + std::to_string(left) + " treasures left" + (nearest >= 0 ? ", the nearest about " + std::to_string(static_cast<int32_t>(nearest)) + " away." : ".");
				} else if (event.type == eEventType::INVASION) {
					text += " " + std::to_string(event.kills) + " invaders smashed, " + std::to_string(event.alive.size()) + " still here.";
				} else if (event.type == eEventType::BONUS) {
					const auto& bonus = Detail::g_Multipliers;
					text += " Coins x" + Times(bonus.coins) + ", U-score x" + Times(bonus.uscore) + ", loot chance x" + Times(bonus.lootChance) + ".";
				}
				Tell(sysAddr, text);
			}
		} catch (const std::exception& ex) {
			LOG("/challenge failed: %s", ex.what());
			Tell(sysAddr, "Challenges can't be shown right now.");
		}
	}
}
