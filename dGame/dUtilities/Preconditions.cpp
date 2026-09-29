#include "Preconditions.h"

#include "Game.h"
#include "Logger.h"

#include <sstream>

#include "InventoryComponent.h"
#include "MissionComponent.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "LevelProgressionComponent.h"
#include "DestroyableComponent.h"
#include "GameMessages.h"
#include "ObjectMessages.h"
#include "eMissionState.h"
#include "PetComponent.h"

std::map<uint32_t, Precondition*> Preconditions::cache = {};

Precondition::Precondition(const uint32_t condition) {
	auto query = CDClientDatabase::CreatePreppedStmt(
		"SELECT type, targetLOT, targetCount FROM Preconditions WHERE id = ?;");
	query.bind(1, static_cast<int>(condition));

	auto result = query.execQuery();

	if (result.eof()) {
		this->type = PreconditionType::ItemEquipped;
		this->count = 1;
		this->values.clear();
		this->values.push_back(0);

		LOG("Failed to find precondition of id (%i)!", condition);

		return;
	}

	this->type = static_cast<PreconditionType>(result.fieldIsNull("type") ? 0 : result.getIntField("type"));

	if (!result.fieldIsNull("targetLOT")) {
		std::istringstream stream(result.getStringField("targetLOT"));
		std::string token;

		while (std::getline(stream, token, ',')) {
			const auto validToken = GeneralUtils::TryParse<uint32_t>(token);
			if (validToken) this->values.push_back(validToken.value());
		}
	}

	this->count = result.fieldIsNull("targetCount") ? 1 : result.getIntField("targetCount");

	result.finalize();
}


bool Precondition::Check(Entity* player) const {
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
	case PreconditionType::NotFreeTrial:
	case PreconditionType::MissionActive:
	case PreconditionType::DoesNotHaveFlag:
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
		const auto passed = CheckValue(player, value);

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


bool Precondition::CheckValue(Entity* player, const uint32_t value) const {
	auto* character = player->GetCharacter();
	auto [missionComponent, inventoryComponent, destroyableComponent, levelComponent] =
		player->GetComponentsMut<const MissionComponent, /* not const */ InventoryComponent, const DestroyableComponent, const LevelProgressionComponent>();
	
	if (!missionComponent || !inventoryComponent || !destroyableComponent || !levelComponent || !character) {
		return false;
	}

	Mission* mission;

	switch (type) {
	case PreconditionType::ItemEquipped:
		return inventoryComponent->IsEquipped(value);
	case PreconditionType::ItemNotEquipped:
		return !inventoryComponent->IsEquipped(value);
	case PreconditionType::HasItem:
		return inventoryComponent->GetLotCount(value) >= count;
	case PreconditionType::DoesNotHaveItem:
		return inventoryComponent->IsEquipped(value) && count > 0;
	case PreconditionType::HasAchievement:
		if (missionComponent == nullptr) return false;
		return missionComponent->GetMissionState(value) >= eMissionState::COMPLETE;
	case PreconditionType::MissionAvailable:
		if (missionComponent == nullptr) return false;
		return missionComponent->GetMissionState(value) == eMissionState::AVAILABLE || missionComponent->GetMissionState(value) == eMissionState::COMPLETE_AVAILABLE;
	case PreconditionType::OnMission:
		if (missionComponent == nullptr) return false;
		return  missionComponent->GetMissionState(value) == eMissionState::ACTIVE || 
				missionComponent->GetMissionState(value) == eMissionState::COMPLETE_ACTIVE ||
				missionComponent->GetMissionState(value) == eMissionState::READY_TO_COMPLETE ||
				missionComponent->GetMissionState(value) == eMissionState::COMPLETE_READY_TO_COMPLETE;
	case PreconditionType::MissionComplete:
		if (missionComponent == nullptr) return false;
		return missionComponent->GetMissionState(value) >= eMissionState::COMPLETE;
	case PreconditionType::PetDeployed:
		return PetComponent::GetActivePet(player->GetObjectID()) != nullptr;
	case PreconditionType::HasFlag:
		return character->GetPlayerFlag(value);
	case PreconditionType::WithinShape:
		return true; // Client checks this one
	case PreconditionType::InBuild:
		return character->GetBuildMode();
	case PreconditionType::TeamCheck:
		return false; // TODO: requires knowing the player's minigame team assignment (red/blue etc.); DLU does not track this per-player
	case PreconditionType::IsPetTaming:
		return PetComponent::GetTamingPet(player->GetObjectID()) != nullptr;
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
		return false; // TODO: requires a racing licence level on the player; DLU does not track this
	case PreconditionType::DoesNotHaveRacingLicence:
		return false; // TODO: requires a racing licence level on the player; DLU does not track this
	case PreconditionType::LegoClubMember:
		return true; // Live LU opened LEGO CLUB to All players at some point, so always return true
	case PreconditionType::NoInteraction:
		return false; // TODO: requires tracking the player's currently active interaction object; DLU does not track this
	case PreconditionType::HasLevel:
		return levelComponent->GetLevel() >= value;
	case PreconditionType::NotFreeTrial:
		return true; // DLU does not support free trial accounts; all players pass this check
	case PreconditionType::MissionActive: {
		if (missionComponent == nullptr) return false;
		const auto state = missionComponent->GetMissionState(value);
		return state == eMissionState::ACTIVE || state == eMissionState::COMPLETE_ACTIVE;
	}
	case PreconditionType::DoesNotHaveFlag:
		return !character->GetPlayerFlag(value);
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
		case ':':
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


std::optional<ItemCost> Precondition::PickItemCost(const std::vector<uint32_t>& lots, const uint32_t count, const std::function<uint32_t(LOT)>& lotCount) {
	for (const auto lot : lots) {
		if (lotCount(static_cast<LOT>(lot)) >= count) return ItemCost{ static_cast<LOT>(lot), count };
	}
	return std::nullopt;
}

std::optional<ItemCost> Precondition::GetItemCost(Entity* player) const {
	if (type != PreconditionType::HasItem || count == 0) return std::nullopt;
	auto* const inventoryComponent = player->GetComponent<InventoryComponent>();
	if (!inventoryComponent) return std::nullopt;
	return PickItemCost(values, count, [inventoryComponent](const LOT lot) { return inventoryComponent->GetLotCount(lot); });
}

std::vector<ItemCost> PreconditionExpression::GetItemCosts(Entity* player) const {
	std::vector<ItemCost> costs;
	for (const auto* expression = this; expression && !expression->empty; expression = expression->next) {
		const auto cost = Preconditions::Get(expression->condition).GetItemCost(player);
		if (cost) costs.push_back(*cost);
	}
	return costs;
}

bool PreconditionExpression::Check(Entity* player) const {
	if (empty) {
		return true;
	}

	const auto a = Preconditions::Check(player, condition);

	if (!a) {
		GameMessages::NotifyClientFailedPrecondition failedPrecondition;
		failedPrecondition.target = player->GetObjectID();
		failedPrecondition.failedReason = u"";
		failedPrecondition.preconditionID = condition;
		failedPrecondition.Send(player->GetSystemAddress());
	}

	const auto b = next == nullptr ? true : next->Check(player);

	return m_or ? a || b : a && b;
}

PreconditionExpression::~PreconditionExpression() {
	delete next;
}


const Precondition& Preconditions::Get(const uint32_t condition) {
	const auto& index = cache.find(condition);
	if (index != cache.end()) return *index->second;

	auto* const precondition = new Precondition(condition);
	cache.insert_or_assign(condition, precondition);
	return *precondition;
}

bool Preconditions::Check(Entity* player, const uint32_t condition) {
	return Get(condition).Check(player);
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
