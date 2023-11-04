#include "Mission.h"

#include <ctime>

#include "CDClientManager.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "LevelProgressionComponent.h"
#include "DestroyableComponent.h"
#include "EntityManager.h"
#include "Game.h"
#include "GameMessages.h"
#include "Mail.h"
#include "MissionComponent.h"
#include "eRacingTaskParam.h"
#include "Logger.h"
#include "dServer.h"
#include "dZoneManager.h"
#include "InventoryComponent.h"
#include "User.h"
#include "Database.h"
#include "WorldConfig.h"
#include "eMissionState.h"
#include "eMissionTaskType.h"
#include "eMissionLockState.h"
#include "eReplicaComponentType.h"

#include "CDMissionEmailTable.h"

Mission::Mission(MissionComponent* missionComponent, const uint32_t missionId) {
	m_MissionComponent = missionComponent;

	m_Completions = 0;

	m_Timestamp = 0;

	m_UniqueMissionID = Game::zoneManager->GetUniqueMissionIdStartingValue();

	m_Reward = 0;

	m_State = eMissionState::UNKNOWN;

	auto* missionsTable = CDClientManager::Instance().GetTable<CDMissionsTable>();

	auto* mis = missionsTable->GetPtrByMissionID(missionId);
	info = *mis;

	if (mis == &CDMissionsTable::Default) {
		LOG("Failed to find mission (%i)!", missionId);

		return;
	}

	auto* tasksTable = CDClientManager::Instance().GetTable<CDMissionTasksTable>();

	auto tasks = tasksTable->GetByMissionID(missionId);

	for (auto i = 0U; i < tasks.size(); ++i) {
		auto* info = tasks[i];

		auto* task = new MissionTask(this, info, i);

		m_Tasks.push_back(task);
	}
}

void Mission::LoadFromXml(tinyxml2::XMLElement* element) {
	// Start custom XML
	if (element->Attribute("state") != nullptr) {
		m_State = static_cast<eMissionState>(std::stoul(element->Attribute("state")));
	}
	// End custom XML

	if (element->Attribute("cct") != nullptr) {
		m_Completions = std::stoul(element->Attribute("cct"));

		m_Timestamp = std::stoul(element->Attribute("cts"));

		if (IsComplete()) {
			return;
		}
	}

	auto* task = element->FirstChildElement();

	auto index = 0U;

	while (task != nullptr) {
		if (index >= m_Tasks.size()) {
			break;
		}

		const auto type = m_Tasks[index]->GetType();

		if (type == eMissionTaskType::COLLECTION ||
			type == eMissionTaskType::VISIT_PROPERTY) {
			std::vector<uint32_t> uniques;

			const auto value = std::stoul(task->Attribute("v"));

			m_Tasks[index]->SetProgress(value, false);

			task = task->NextSiblingElement();

			while (task != nullptr) {
				const auto unique = std::stoul(task->Attribute("v"));

				uniques.push_back(unique);

				if (m_MissionComponent != nullptr && type == eMissionTaskType::COLLECTION) {
					m_MissionComponent->AddCollectible(unique);
				}

				task = task->NextSiblingElement();
			}

			m_Tasks[index]->SetUnique(uniques);

			m_Tasks[index]->SetProgress(uniques.size(), false);

			break;
		} else {
			const auto value = std::stoul(task->Attribute("v"));

			m_Tasks[index]->SetProgress(value, false);

			task = task->NextSiblingElement();
		}

		index++;
	}
}

void Mission::UpdateXml(tinyxml2::XMLElement* element) {
	// Start custom XML
	element->SetAttribute("state", static_cast<unsigned int>(m_State));
	// End custom XML

	element->DeleteChildren();

	element->SetAttribute("id", static_cast<unsigned int>(info.id));

	if (m_Completions > 0) {
		element->SetAttribute("cct", static_cast<unsigned int>(m_Completions));

		element->SetAttribute("cts", static_cast<unsigned int>(m_Timestamp));

		if (IsComplete()) {
			return;
		}
	}

	for (auto* task : m_Tasks) {
		if (task->GetType() == eMissionTaskType::COLLECTION ||
			task->GetType() == eMissionTaskType::VISIT_PROPERTY) {

			auto* child = element->GetDocument()->NewElement("sv");

			child->SetAttribute("v", static_cast<unsigned int>(task->GetProgress()));

			element->LinkEndChild(child);

			for (auto unique : task->GetUnique()) {
				auto* uniqueElement = element->GetDocument()->NewElement("sv");

				uniqueElement->SetAttribute("v", static_cast<unsigned int>(unique));

				element->LinkEndChild(uniqueElement);
			}

			break;
		}
		auto* child = element->GetDocument()->NewElement("sv");

		child->SetAttribute("v", static_cast<unsigned int>(task->GetProgress()));

		element->LinkEndChild(child);
	}
}

bool Mission::IsValidMission(const uint32_t missionId) {
	auto* table = CDClientManager::Instance().GetTable<CDMissionsTable>();

	const auto missions = table->Query([=](const CDMissions& entry) {
		return entry.id == static_cast<int>(missionId);
		});

	return !missions.empty();
}

bool Mission::IsValidMission(const uint32_t missionId, CDMissions& info) {
	auto* table = CDClientManager::Instance().GetTable<CDMissionsTable>();

	const auto missions = table->Query([=](const CDMissions& entry) {
		return entry.id == static_cast<int>(missionId);
		});

	if (missions.empty()) {
		return false;
	}

	info = missions[0];

	return true;
}

Entity* Mission::GetAssociate() const {
	return m_MissionComponent->GetParent();
}

User* Mission::GetUser() const {
	return GetAssociate()->GetParentUser();
}

uint32_t Mission::GetMissionId() const {
	return info.id;
}

const CDMissions& Mission::GetClientInfo() const {
	return info;
}

uint32_t Mission::GetCompletions() const {
	return m_Completions;
}

uint32_t Mission::GetTimestamp() const {
	return m_Timestamp;
}

LOT Mission::GetReward() const {
	return m_Reward;
}

std::vector<MissionTask*> Mission::GetTasks() const {
	return m_Tasks;
}

eMissionState Mission::GetMissionState() const {
	return m_State;
}

bool Mission::IsAchievement() const {
	return !info.isMission;
}

bool Mission::IsMission() const {
	return info.isMission;
}

bool Mission::IsRepeatable() const {
	return info.repeatable;
}

bool Mission::IsComplete() const {
	return m_State == eMissionState::COMPLETE;
}

bool Mission::IsActive() const {
	return m_State == eMissionState::ACTIVE || m_State == eMissionState::COMPLETE_AVAILABLE;
}

void Mission::MakeActive() {
	SetMissionState(m_Completions == 0 ? eMissionState::ACTIVE : eMissionState::COMPLETE_ACTIVE);
}

bool Mission::IsReadyToComplete() const {
	return m_State == eMissionState::READY_TO_COMPLETE || m_State == eMissionState::COMPLETE_READY_TO_COMPLETE;
}

void Mission::MakeReadyToComplete() {
	SetMissionState(m_Completions == 0 ? eMissionState::READY_TO_COMPLETE : eMissionState::COMPLETE_READY_TO_COMPLETE);
}

bool Mission::IsAvalible() const {
	return m_State == eMissionState::AVAILABLE || m_State == eMissionState::COMPLETE_AVAILABLE;
}

bool Mission::IsFetchMission() const {
	return m_Tasks.size() == 1 && m_Tasks[0]->GetType() == eMissionTaskType::TALK_TO_NPC;
}

void Mission::MakeAvalible() {
	SetMissionState(m_Completions == 0 ? eMissionState::AVAILABLE : eMissionState::COMPLETE_AVAILABLE);
}

void Mission::Accept() {
	SetMissionTypeState(eMissionLockState::NEW, info.defined_type, info.defined_subtype);

	SetMissionState(m_Completions > 0 ? eMissionState::COMPLETE_ACTIVE : eMissionState::ACTIVE);

	Catchup();
}

void Mission::Complete(const bool yieldRewards) {
	if (m_State != eMissionState::ACTIVE && m_State != eMissionState::COMPLETE_ACTIVE) {
		// If we are accepting a mission here there is no point to giving it a unique ID since we just complete it immediately.
		Accept();
	}

	for (auto* task : m_Tasks) {
		task->Complete();
	}

	SetMissionState(eMissionState::REWARDING, true);

	if (yieldRewards) {
		YieldRewards();
	}

	SetMissionState(eMissionState::COMPLETE);

	m_Completions++;

	m_Timestamp = std::time(nullptr);

	auto* entity = GetAssociate();

	if (entity == nullptr) {
		return;
	}

	auto* characterComponent = entity->GetComponent<CharacterComponent>();
	if (characterComponent != nullptr) {
		characterComponent->TrackMissionCompletion(!info.isMission);
	}

	auto* missionComponent = entity->GetComponent<MissionComponent>();

	missionComponent->Progress(eMissionTaskType::META, info.id);

	missionComponent->Progress(eMissionTaskType::RACING, info.id, (LWOOBJID)eRacingTaskParam::COMPLETE_ANY_RACING_TASK);

	missionComponent->Progress(eMissionTaskType::RACING, info.id, (LWOOBJID)eRacingTaskParam::COMPLETE_TRACK_TASKS);

	auto* missionEmailTable = CDClientManager::Instance().GetTable<CDMissionEmailTable>();

	const auto missionId = GetMissionId();

	const auto missionEmails = missionEmailTable->Query([missionId](const CDMissionEmail& entry) {
		return entry.missionID == missionId;
		});

	for (const auto& email : missionEmails) {
		const auto missionEmailBase = "MissionEmail_" + std::to_string(email.ID) + "_";

		if (email.messageType == 1) {
			const auto subject = "%[" + missionEmailBase + "subjectText]";
			const auto body = "%[" + missionEmailBase + "bodyText]";
			const auto sender = "%[" + missionEmailBase + "senderName]";

			Mail::SendMail(LWOOBJID_EMPTY, sender, GetAssociate(), subject, body, email.attachmentLOT, 1);
		}
	}
}

void Mission::CheckCompletion() {
	for (auto* task : m_Tasks) {
		if (!task->IsComplete()) {
			return;
		}
	}

	if (IsAchievement()) {
		Complete();

		return;
	}

	MakeReadyToComplete();
}

void Mission::Catchup() {
	auto* entity = GetAssociate();

	auto* inventory = static_cast<InventoryComponent*>(entity->GetComponent(eReplicaComponentType::INVENTORY));

	for (auto* task : m_Tasks) {
		const auto type = task->GetType();

		if (type == eMissionTaskType::GATHER) {
			for (auto target : task->GetAllTargets()) {
				const auto count = inventory->GetLotCountNonTransfer(target);

				for (auto i = 0U; i < count; ++i) {
					task->Progress(target);
				}
			}
		}

		if (type == eMissionTaskType::PLAYER_FLAG) {
			for (int32_t target : task->GetAllTargets()) {
				const auto flag = GetUser()->GetLastUsedChar()->GetPlayerFlag(target);

				if (!flag) {
					continue;
				}

				task->Progress(target);

				if (task->IsComplete()) {
					break;
				}
			}
		}
	}
}

void Mission::YieldRewards() {
	auto* entity = GetAssociate();

	if (entity == nullptr) {
		return;
	}

	auto* character = GetUser()->GetLastUsedChar();

	auto* inventoryComponent = entity->GetComponent<InventoryComponent>();
	auto* levelComponent = entity->GetComponent<LevelProgressionComponent>();
	auto* characterComponent = entity->GetComponent<CharacterComponent>();
	auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();
	auto* missionComponent = entity->GetComponent<MissionComponent>();

	// Remove mission items
	for (auto* task : m_Tasks) {
		if (task->GetType() != eMissionTaskType::GATHER) {
			continue;
		}

		const auto& param = task->GetParameters();

		if (param.empty() || (param[0] & 1) == 0) // Should items be removed?
		{
			for (const auto target : task->GetAllTargets()) {
				// This is how live did it.  ONLY remove item collection items from the items and hidden inventories and none of the others.
				inventoryComponent->RemoveItem(target, task->GetClientInfo().targetValue, eInventoryType::ITEMS);
				inventoryComponent->RemoveItem(target, task->GetClientInfo().targetValue, eInventoryType::QUEST);

				missionComponent->Progress(eMissionTaskType::GATHER, target, LWOOBJID_EMPTY, "", -task->GetClientInfo().targetValue);
			}
		}
	}

	int32_t coinsToSend = 0;
	if (info.LegoScore > 0) {
		eLootSourceType lootSource = info.isMission ? eLootSourceType::MISSION : eLootSourceType::ACHIEVEMENT;
		if (levelComponent->GetLevel() >= Game::zoneManager->GetWorldConfig()->levelCap) {
			// Since the character is at the level cap we reward them with coins instead of UScore.
			coinsToSend += info.LegoScore * Game::zoneManager->GetWorldConfig()->levelCapCurrencyConversion;
		} else {
			characterComponent->SetUScore(characterComponent->GetUScore() + info.LegoScore);
			GameMessages::SendModifyLEGOScore(entity, entity->GetSystemAddress(), info.LegoScore, lootSource);
		}
	}

	if (m_Completions > 0) {
		std::vector<std::pair<LOT, uint32_t>> items;

		items.emplace_back(info.reward_item1_repeatable, info.reward_item1_repeat_count);
		items.emplace_back(info.reward_item2_repeatable, info.reward_item2_repeat_count);
		items.emplace_back(info.reward_item3_repeatable, info.reward_item3_repeat_count);
		items.emplace_back(info.reward_item4_repeatable, info.reward_item4_repeat_count);

		for (const auto& pair : items) {
			// Some missions reward zero of an item and so they must be allowed through this clause,
			// hence pair.second < 0 instead of pair.second <= 0.
			if (pair.second < 0 || (m_Reward > 0 && pair.first != m_Reward)) {
				continue;
			}

			// If a mission rewards zero of an item, make it reward 1.
			auto count = pair.second > 0 ? pair.second : 1;

			// Sanity check, 6 is the max any mission yields
			if (count > 6) {
				count = 0;
			}

			inventoryComponent->AddItem(pair.first, count, IsMission() ? eLootSourceType::MISSION : eLootSourceType::ACHIEVEMENT);
		}

		if (info.reward_currency_repeatable > 0 || coinsToSend > 0) {
			eLootSourceType lootSource = info.isMission ? eLootSourceType::MISSION : eLootSourceType::ACHIEVEMENT;
			character->SetCoins(character->GetCoins() + info.reward_currency_repeatable + coinsToSend, lootSource);
		}

		return;
	}

	std::vector<std::pair<LOT, int32_t>> items;

	items.emplace_back(info.reward_item1, info.reward_item1_count);
	items.emplace_back(info.reward_item2, info.reward_item2_count);
	items.emplace_back(info.reward_item3, info.reward_item3_count);
	items.emplace_back(info.reward_item4, info.reward_item4_count);

	for (const auto& pair : items) {
		// Some missions reward zero of an item and so they must be allowed through this clause,
		// hence pair.second < 0 instead of pair.second <= 0.
		if (pair.second < 0 || (m_Reward > 0 && pair.first != m_Reward)) {
			continue;
		}

		// If a mission rewards zero of an item, make it reward 1.
		auto count = pair.second > 0 ? pair.second : 1;

		// Sanity check, 6 is the max any mission yields
		if (count > 6) {
			count = 0;
		}

		inventoryComponent->AddItem(pair.first, count, IsMission() ? eLootSourceType::MISSION : eLootSourceType::ACHIEVEMENT);
	}

	if (info.reward_currency > 0 || coinsToSend > 0) {
		eLootSourceType lootSource = info.isMission ? eLootSourceType::MISSION : eLootSourceType::ACHIEVEMENT;
		character->SetCoins(character->GetCoins() + info.reward_currency + coinsToSend, lootSource);
	}

	if (info.reward_maxinventory > 0) {
		auto* inventory = inventoryComponent->GetInventory(ITEMS);

		inventory->SetSize(inventory->GetSize() + info.reward_maxinventory);
	}

	if (info.reward_bankinventory > 0) {
		auto* inventory = inventoryComponent->GetInventory(eInventoryType::VAULT_ITEMS);
		auto modelInventory = inventoryComponent->GetInventory(eInventoryType::VAULT_MODELS);

		inventory->SetSize(inventory->GetSize() + info.reward_bankinventory);
		modelInventory->SetSize(modelInventory->GetSize() + info.reward_bankinventory);
	}

	if (info.reward_reputation > 0) {
		missionComponent->Progress(eMissionTaskType::EARN_REPUTATION, 0, 0L, "", info.reward_reputation);
		auto character = entity->GetComponent<CharacterComponent>();
		if (character) {
			character->SetReputation(character->GetReputation() + info.reward_reputation);
			GameMessages::SendUpdateReputation(entity->GetObjectID(), character->GetReputation(), entity->GetSystemAddress());
		}
	}

	if (info.reward_maxhealth > 0) {
		destroyableComponent->SetMaxHealth(destroyableComponent->GetMaxHealth() + static_cast<float>(info.reward_maxhealth), true);
	}

	if (info.reward_maximagination > 0) {
		destroyableComponent->SetMaxImagination(destroyableComponent->GetMaxImagination() + static_cast<float>(info.reward_maximagination), true);
	}

	Game::entityManager->SerializeEntity(entity);

	if (info.reward_emote > 0) {
		character->UnlockEmote(info.reward_emote);
	}

	if (info.reward_emote2 > 0) {
		character->UnlockEmote(info.reward_emote2);
	}

	if (info.reward_emote3 > 0) {
		character->UnlockEmote(info.reward_emote3);
	}

	if (info.reward_emote4 > 0) {
		character->UnlockEmote(info.reward_emote4);
	}
}

void Mission::Progress(eMissionTaskType type, int32_t value, LWOOBJID associate, const std::string& targets, int32_t count) {
	const auto isRemoval = count < 0;

	if (isRemoval && (IsComplete() || IsAchievement())) {
		return;
	}

	for (auto* task : m_Tasks) {
		if (task->IsComplete() && !isRemoval) {
			continue;
		}

		if (task->GetType() != type) {
			continue;
		}

		if (isRemoval && !task->InAllTargets(value)) {
			continue;
		}

		task->Progress(value, associate, targets, count);
	}
}

void Mission::SetMissionState(const eMissionState state, const bool sendingRewards) {
	this->m_State = state;

	auto* entity = GetAssociate();

	if (entity == nullptr) {
		return;
	}

	GameMessages::SendNotifyMission(entity, entity->GetParentUser()->GetSystemAddress(), info.id, static_cast<int>(state), sendingRewards);
}

void Mission::SetMissionTypeState(eMissionLockState state, const std::string& type, const std::string& subType) {
	// TODO
}

void Mission::SetCompletions(const uint32_t value) {
	m_Completions = value;
}

void Mission::SetReward(const LOT lot) {
	m_Reward = lot;
}

Mission::~Mission() {
	for (auto* task : m_Tasks) {
		delete task;
	}

	m_Tasks.clear();
}
