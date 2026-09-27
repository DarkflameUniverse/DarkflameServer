#include "PlayerReports.h"

#include <ctime>

#include "Database.h"
#include "DashboardNotify.h"
#include "Entity.h"
#include "EntityManager.h"
#include "Character.h"
#include "User.h"
#include "Game.h"
#include "dServer.h"
#include "dZoneManager.h"
#include "Logger.h"
#include "GeneralUtils.h"
#include "PetComponent.h"
#include "PropertyManagementComponent.h"
#include "ePlayerReportKind.h"
#include "magic_enum.hpp"

namespace {
	constexpr uint32_t MAX_BODY = 2000;

	// Who sent it and where they are
	IModeration::PlayerReport NewReport(Entity* reporter, ePlayerReportKind kind, const std::string& body) {
		IModeration::PlayerReport report;
		report.createdAt = static_cast<int64_t>(std::time(nullptr));
		report.kind = std::string(magic_enum::enum_name(kind));
		report.body = body.substr(0, MAX_BODY);
		if (const auto* character = reporter ? reporter->GetCharacter() : nullptr) {
			report.reporterId = character->GetID();
			if (const auto* user = character->GetParentUser()) report.reporterAccountId = user->GetAccountID();
		}
		report.zoneId = Game::server->GetZoneID();
		report.instanceId = Game::server->GetInstanceID();
		report.cloneId = Game::zoneManager->GetZoneID().GetCloneID();
		return report;
	}

	// The player a report is about, when the world knows
	void SetTarget(IModeration::PlayerReport& report, LWOOBJID characterId) {
		const auto info = characterId ? Database::Get()->GetCharacterInfo(characterId) : std::nullopt;
		if (!info) return;
		report.targetCharacterId = info->id;
		report.targetAccountId = info->accountId;
	}

	PlayerReports::RateLimit g_RateLimit;

	void Save(const IModeration::PlayerReport& report) {
		// By account (a player can't get more by switching characters), or by character when the account is unknown
		const uint64_t reporter = report.reporterAccountId != 0 ? report.reporterAccountId : static_cast<uint64_t>(report.reporterId) | (1ULL << 63);
		if (!g_RateLimit.Allow(reporter, report.createdAt)) {
			LOG("Dropped a player report (%s) from account %u / character %llu: over the report limit", report.kind.c_str(), report.reporterAccountId, report.reporterId);
			return;
		}
		const auto id = Database::Get()->InsertPlayerReport(report);
		LOG("Player report %llu (%s) from %llu about %llu", id, report.kind.c_str(), report.reporterId, report.targetCharacterId);
		DashboardNotify::Changed("player_reports");
	}
}

namespace PlayerReports {
	void ReportPlayer(Entity* reporter, LWOOBJID reportedId, const std::string& body) {
		auto report = NewReport(reporter, ePlayerReportKind::PLAYER, body);
		report.objectId = reportedId;
		// The client sends the player's object ID; when they're in this world, their character says which one it is
		const auto* reported = Game::entityManager->GetEntity(reportedId);
		const auto* character = reported ? reported->GetCharacter() : nullptr;
		SetTarget(report, character ? character->GetID() : reportedId);
		Save(report);
	}

	void ReportOffensiveModel(Entity* reporter, const std::u16string& text, LWOOBJID objectId) {
		if (text.size() > MAX_BODY) return;
		const auto description = GeneralUtils::UTF16ToWTF8(text);
		auto report = NewReport(reporter, ePlayerReportKind::MODEL, description);
		report.objectId = objectId;
		auto* object = Game::entityManager->GetEntity(objectId);
		if (object) report.objectLot = object->GetLOT();
		// A model placed on this property belongs to the property's owner; a pet to its owner
		auto* property = PropertyManagementComponent::Instance();
		if (property && property->GetModels().contains(objectId)) {
			report.propertyId = property->GetId();
			SetTarget(report, property->GetOwnerId());
		} else if (auto* pet = object ? object->GetComponent<PetComponent>() : nullptr) {
			SetTarget(report, pet->GetOwnerId());
		}
		Save(report);
	}

	void ReportOffensiveProperty(Entity* reporter, const std::u16string& text, LWOOBJID plaqueId) {
		if (text.size() > MAX_BODY) return;
		const auto description = GeneralUtils::UTF16ToWTF8(text);
		auto report = NewReport(reporter, ePlayerReportKind::PROPERTY, description);
		report.objectId = plaqueId;
		if (auto* plaque = Game::entityManager->GetEntity(plaqueId)) report.objectLot = plaque->GetLOT();
		// The plaque stands on the property this world holds
		if (auto* property = PropertyManagementComponent::Instance()) {
			report.propertyId = property->GetId();
			SetTarget(report, property->GetOwnerId());
		}
		Save(report);
	}
}
