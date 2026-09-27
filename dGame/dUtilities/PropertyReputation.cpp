#include "PropertyReputation.h"

#include <chrono>
#include <ctime>
#include <map>
#include <set>

#include "Character.h"
#include "Database.h"
#include "DashboardNotify.h"
#include "dConfig.h"
#include "dZoneManager.h"
#include "eGameMasterLevel.h"
#include "EconomyLedger.h"
#include "Entity.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "PlayerManager.h"
#include "PropertyManagementComponent.h"
#include "PropertyRent.h"
#include "User.h"

namespace {
	using namespace PropertyReputationRules;
	constexpr auto INTERVAL = std::chrono::seconds(60);
	constexpr uint32_t REPEAT_DAYS_DEFAULT = 30;

	struct Tracked {
		LWOOBJID playerId{};
		Visit visit;
		Position last;
		bool eligible{};
		int64_t pendingSeconds{};
	};

	std::chrono::steady_clock::time_point g_LastTick{};
	LWOOBJID g_PropertyId{};
	uint32_t g_OwnerAccount{};
	std::set<uint32_t> g_Linked;
	uint32_t g_Day{};
	int64_t g_PropertyToday{};
	std::map<uint32_t, Tracked> g_Visitors; // by account

	template<typename T>
	T Setting(const std::string& key, T fallback) {
		return GeneralUtils::TryParse<T>(Game::config->GetValue(key)).value_or(fallback);
	}

	uint32_t RepeatDays() {
		return Setting<uint32_t>("property_reputation_repeat_days", REPEAT_DAYS_DEFAULT);
	}

	Position PositionOf(const Entity* entity) {
		const auto p = entity->GetPosition();
		return { p.x, p.y, p.z };
	}

	// Load who owns the property and which accounts are linked to the owner's
	void LoadOwner(LWOOBJID propertyId) {
		g_PropertyId = propertyId;
		g_OwnerAccount = 0;
		g_Linked.clear();
		g_Visitors.clear();
		g_Day = 0;
		const auto info = Database::Get()->GetPropertyInfo(propertyId);
		const auto owner = info ? Database::Get()->GetCharacterInfo(info->ownerId) : std::nullopt;
		if (!owner) return;
		g_OwnerAccount = owner->accountId;
		for (const auto& linked : Database::Get()->GetLinkedAccounts(g_OwnerAccount)) g_Linked.insert(linked.accountId);
	}

	void Flush(uint32_t accountId, Tracked& tracked, int64_t points) {
		if (!tracked.eligible || (points <= 0 && tracked.pendingSeconds <= 0)) return;
		Database::Get()->AddPropertyReputation(g_PropertyId, accountId, g_Day, points, tracked.pendingSeconds);
		tracked.pendingSeconds = 0;
	}
}

namespace PropertyReputation {
	PropertyReputationRules::Params LoadParams() {
		Params params;
		params.minVisitSeconds = Setting<int64_t>("property_reputation_min_visit", params.minVisitSeconds);
		params.multiplier = Setting<double>("property_reputation_multiplier", params.multiplier);
		params.maxMinutesPerVisit = Setting<int64_t>("property_reputation_max_minutes", params.maxMinutesPerVisit);
		params.visitorDailyCap = Setting<int64_t>("property_reputation_visitor_daily_cap", params.visitorDailyCap);
		params.propertyDailyCap = Setting<int64_t>("property_reputation_daily_cap", params.propertyDailyCap);
		params.repeatFalloff = Setting<double>("property_reputation_repeat_falloff", params.repeatFalloff);
		params.requireActivity = Game::config->GetValue("property_reputation_require_activity") != "0";
		params.ignoreStaff = Game::config->GetValue("property_reputation_ignore_staff") != "0";
		params.ignoreLinked = Game::config->GetValue("property_reputation_ignore_linked") != "0";
		return params;
	}

	void Tick() {
		const auto nowSteady = std::chrono::steady_clock::now();
		if (nowSteady - g_LastTick < INTERVAL) return;
		g_LastTick = nowSteady;

		auto* property = PropertyManagementComponent::Instance();
		if (!property || property->GetId() == LWOOBJID_EMPTY || property->GetOwnerId() == LWOOBJID_EMPTY) return;
		if (Game::config->GetValue("property_reputation_enabled") == "0") return;
		if (property->GetId() != g_PropertyId) LoadOwner(property->GetId());
		if (g_OwnerAccount == 0) return;

		const auto params = LoadParams();
		const auto* row = PropertyRent::WorldTemplate(Game::zoneManager->GetZoneID().GetMapID());
		const int32_t perMinute = row ? row->reputationPerMinute : 1;
		const auto now = static_cast<int64_t>(std::time(nullptr));
		const auto day = EconomyLedger::Today();
		if (day != g_Day) {
			g_Day = day;
			g_PropertyToday = Database::Get()->GetPropertyReputationOnDay(g_PropertyId, day);
		}

		std::set<uint32_t> present;
		int64_t gained = 0;
		for (auto* player : PlayerManager::GetAllPlayers()) {
			auto* character = player ? player->GetCharacter() : nullptr;
			auto* user = character ? character->GetParentUser() : nullptr;
			if (!user) continue;
			const auto accountId = user->GetAccountID();
			// One visitor per account, however many of its characters are here
			if (!present.insert(accountId).second) continue;

			auto it = g_Visitors.find(accountId);
			if (it == g_Visitors.end() || it->second.playerId != player->GetObjectID()) {
				Tracked tracked;
				tracked.playerId = player->GetObjectID();
				tracked.last = PositionOf(player);
				tracked.visit.enteredAt = now;
				tracked.visit.day = day;
				tracked.eligible = Eligible(accountId, g_OwnerAccount, g_Linked, user->GetMaxGMLevel() > eGameMasterLevel::CIVILIAN, params);
				if (tracked.eligible) {
					const auto history = Database::Get()->GetPropertyVisitorHistory(g_PropertyId, accountId, day, RepeatDays());
					tracked.visit.visitorToday = history.today;
					tracked.visit.repeatFactor = RepeatFactor(history.previousDays, params.repeatFalloff);
				}
				if (it != g_Visitors.end()) Flush(accountId, it->second, 0);
				g_Visitors[accountId] = tracked;
				continue;
			}

			auto& tracked = it->second;
			if (!tracked.eligible) continue;
			const auto position = PositionOf(player);
			const bool active = Moved(tracked.last, position);
			tracked.last = position;
			tracked.pendingSeconds += std::chrono::duration_cast<std::chrono::seconds>(INTERVAL).count();
			const auto points = OnMinute(tracked.visit, params, perMinute, now, day, active, g_PropertyToday);
			if (points > 0) {
				Flush(accountId, tracked, points);
				gained += points;
			}
		}

		// Visitors who left: keep the time they spent
		for (auto it = g_Visitors.begin(); it != g_Visitors.end();) {
			if (present.contains(it->first)) { ++it; continue; }
			Flush(it->first, it->second, 0);
			it = g_Visitors.erase(it);
		}

		if (gained > 0) {
			property->AddReputation(static_cast<uint32_t>(gained));
			DashboardNotify::Changed("properties", g_PropertyId);
		}
	}
}
