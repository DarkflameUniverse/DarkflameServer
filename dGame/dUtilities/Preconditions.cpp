#include "Preconditions.h"

#include "Game.h"
#include "dLogger.h"

#include <sstream>

#include "InventoryComponent.h"
#include "MissionComponent.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "LevelProgressionComponent.h"
#include "DestroyableComponent.h"
#include "GameMessages.h"


std::map<uint32_t, Precondition*> Preconditions::cache = {};

Precondition::Precondition(const uint32_t condition) {
	auto query = CDClientDatabase::CreatePreppedStmt(
		"SELECT type, targetLOT, targetCount FROM Preconditions WHERE id = ?;");
	query.bind(1, (int)condition);

	auto result = query.execQuery();

	if (result.eof()) {
		this->type = PreconditionType::ItemEquipped;
		this->count = 1;
		this->values = { 0 };

		Game::logger->Log("Precondition", "Failed to find precondition of id (%i)!", condition);

		return;
	}

	this->type = static_cast<PreconditionType>(result.fieldIsNull(0) ? 0 : result.getIntField(0));

	if (!result.fieldIsNull(1)) {
		std::istringstream stream(result.getStringField(1));
		std::string token;

		while (std::getline(stream, token, ',')) {
			uint32_t value;
			if (GeneralUtils::TryParse(token, value)) {
				this->values.push_back(value);
			}
		}
	}

	this->count = result.fieldIsNull(2) ? 1 : result.getIntField(2);

	result.finalize();
}


bool Precondition::Check(Entity* player, bool evaluateCosts) const {
	if (values.empty()) {
		return true; // There are very few of these
	}

	bool any;

	// Guesses
	switch (type) {
	case PreconditionType::HasItem:
	case PreconditionType::ItemEquipped:
	case PreconditionType::HasAchievement:
	case PreconditionType::MissionAvailable:
	case PreconditionType::OnMission:
	case PreconditionType::MissionComplete:
	case PreconditionType::PetDeployed:
	case PreconditionType::HasFlag:
	case PreconditionType::WithinShape:
	case PreconditionType::InBuild:
	case PreconditionType::TeamCheck:
	case PreconditionType::IsPetTaming:
	case PreconditionType::HasFaction:
	case PreconditionType::DoesNotHaveFaction:
	case PreconditionType::HasRacingLicence:
	case PreconditionType::DoesNotHaveRacingLicence:
	case PreconditionType::LegoClubMember:
	case PreconditionType::NoInteraction:
		any = true;
		break;
	case PreconditionType::DoesNotHaveItem:
	case PreconditionType::ItemNotEquipped:
	case PreconditionType::HasLevel:
		any = false;
		break;
	default:
		any = true;
		break;
	}

	auto passedAny = false;

	for (const auto value : values) {
		const auto passed = CheckValue(player, value, evaluateCosts);

		if (passed && any) {
			return true;
		}

		if (!passed && !any) {
			return false;
		}

		if (passed) {
			passedAny = true;
		}
	}

	return passedAny;
}


bool Precondition::CheckValue(Entity* player, const uint32_t value, bool evaluateCosts) const {
	auto* missionComponent = player->GetComponent<MissionComponent>();
	auto* inventoryComponent = player->GetComponent<InventoryComponent>();
	auto* destroyableComponent = player->GetComponent<DestroyableComponent>();
	auto* levelComponent = player->GetComponent<LevelProgressionComponent>();
	auto* character = player->GetCharacter();

	Mission* mission;

	switch (type) {
	case PreconditionType::ItemEquipped:
		return inventoryComponent->IsEquipped(value);
	case PreconditionType::ItemNotEquipped:
		return !inventoryComponent->IsEquipped(value);
	case PreconditionType::HasItem:
		if (evaluateCosts) // As far as I know this is only used for quickbuilds, and removal shouldn't actually be handled here.
		{
			inventoryComponent->RemoveItem(value, count);

			return true;
		}

		return inventoryComponent->GetLotCount(value) >= count;
	case PreconditionType::DoesNotHaveItem:
		return inventoryComponent->IsEquipped(value) < count;
	case PreconditionType::HasAchievement:
		mission = missionComponent->GetMission(value);

		return mission == nullptr || mission->GetMissionState() >= MissionState::MISSION_STATE_COMPLETE;
	case PreconditionType::MissionAvailable:
		mission = missionComponent->GetMission(value);

		return mission == nullptr || mission->GetMissionState() >= MissionState::MISSION_STATE_AVAILABLE;
	case PreconditionType::OnMission:
		mission = missionComponent->GetMission(value);

		return mission == nullptr || mission->GetMissionState() >= MissionState::MISSION_STATE_ACTIVE;
	case PreconditionType::MissionComplete:
		mission = missionComponent->GetMission(value);

		return mission == nullptr || mission->GetMissionState() >= MissionState::MISSION_STATE_COMPLETE;
	case PreconditionType::PetDeployed:
		return false; // TODO
	case PreconditionType::HasFlag:
		return character->GetPlayerFlag(value);
	case PreconditionType::WithinShape:
		return true; // Client checks this one
	case PreconditionType::InBuild:
		return character->GetBuildMode();
	case PreconditionType::TeamCheck:
		return false; // TODO
	case PreconditionType::IsPetTaming:
		return false; // TODO
	case PreconditionType::HasFaction:
		for (const auto faction : destroyableComponent->GetFactionIDs()) {
			if (faction == static_cast<int>(value)) {
				return true;
			}
		}

		return false;
	case PreconditionType::DoesNotHaveFaction:
		for (const auto faction : destroyableComponent->GetFactionIDs()) {
			if (faction == static_cast<int>(value)) {
				return false;
			}
		}

		return true;
	case PreconditionType::HasRacingLicence:
		return false; // TODO
	case PreconditionType::DoesNotHaveRacingLicence:
		return false; // TODO
	case PreconditionType::LegoClubMember:
		return false; // TODO
	case PreconditionType::NoInteraction:
		return false; // TODO
	case PreconditionType::HasLevel:
		return levelComponent->GetLevel() >= value;
	default:
		return true; // There are a couple more unknown preconditions. Always return true in this case.
	}
}

PreconditionExpression::PreconditionExpression(const std::string& conditions) {
	if (conditions.empty()) {
		empty = true;

		return;
	}

	std::stringstream a;
	std::stringstream b;

	auto bor = false;

	auto done = false;

	for (auto i = 0u; i < conditions.size(); ++i) {
		if (done) {
			break;
		}

		const auto character = conditions[i];

		switch (character) {
		case '|':
			bor = true;
			b << conditions.substr(i + 1);
			done = true;
			break;
		case ' ':
		case ')':
			break;
		case ',':
		case '&':
		case ';':
		case '(':
			b << conditions.substr(i + 1);
			done = true;
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			a << character;
			break;
		default:
			break;
		}
	}

	this->m_or = bor;

	const auto aString = a.str();

	if (!aString.empty()) {
		this->condition = std::stoul(a.str());
	} else {
		this->condition = 0;
	}

	const auto bString = b.str();

	if (!bString.empty()) {
		this->next = new PreconditionExpression(bString);
	} else {
		this->next = nullptr;
	}
}


bool PreconditionExpression::Check(Entity* player, bool evaluateCosts) const {
	if (empty) {
		return true;
	}

	if (player->GetGMLevel() >= 9) // Developers can skip this for testing
	{
		return true;
	}

	const auto a = Preconditions::Check(player, condition, evaluateCosts);

	if (!a) {
		GameMessages::SendNotifyClientFailedPrecondition(player->GetObjectID(), player->GetSystemAddress(), u"", condition);
	}

	const auto b = next == nullptr ? true : next->Check(player, evaluateCosts);

	return m_or ? a || b : a && b;
}

PreconditionExpression::~PreconditionExpression() {
	delete next;
}


bool Preconditions::Check(Entity* player, const uint32_t condition, bool evaluateCosts) {
	Precondition* precondition;

	const auto& index = cache.find(condition);

	if (index != cache.end()) {
		precondition = index->second;
	} else {
		precondition = new Precondition(condition);

		cache.insert_or_assign(condition, precondition);
	}

	return precondition->Check(player, evaluateCosts);
}


PreconditionExpression Preconditions::CreateExpression(const std::string& conditions) {
	return PreconditionExpression(conditions);
}


Preconditions::~Preconditions() {
	for (const auto& condition : cache) {
		delete condition.second;
	}

	cache.clear();
}
