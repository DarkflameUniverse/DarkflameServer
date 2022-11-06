#include "BehaviorContext.h"
#include "Behavior.h"
#include "BehaviorBranchContext.h"
#include "EntityManager.h"
#include "SkillComponent.h"
#include "Game.h"
#include "dLogger.h"
#include "dServer.h"
#include "PacketUtils.h"

#include <sstream>


#include "DestroyableComponent.h"
#include "PhantomPhysicsComponent.h"
#include "RebuildComponent.h"

BehaviorSyncEntry::BehaviorSyncEntry() {
}

BehaviorTimerEntry::BehaviorTimerEntry() {
}

BehaviorEndEntry::BehaviorEndEntry() {
}

uint32_t BehaviorContext::GetUniqueSkillId() const {
	auto* entity = EntityManager::Instance()->GetEntity(this->originator);

	if (entity == nullptr) {
		Game::logger->Log("BehaviorContext", "Invalid entity for (%llu)!", this->originator);

		return 0;
	}

	auto* component = entity->GetComponent<SkillComponent>();

	if (component == nullptr) {
		Game::logger->Log("BehaviorContext", "No skill component attached to (%llu)!", this->originator);;

		return 0;
	}

	return component->GetUniqueSkillId();
}


void BehaviorContext::RegisterSyncBehavior(const uint32_t syncId, Behavior* behavior, const BehaviorBranchContext& branchContext) {
	auto entry = BehaviorSyncEntry();

	entry.handle = syncId;
	entry.behavior = behavior;
	entry.branchContext = branchContext;

	this->syncEntries.push_back(entry);
}

void BehaviorContext::RegisterTimerBehavior(Behavior* behavior, const BehaviorBranchContext& branchContext, const LWOOBJID second) {
	BehaviorTimerEntry entry;

	entry.time = branchContext.duration;
	entry.behavior = behavior;
	entry.branchContext = branchContext;
	entry.second = second;

	this->timerEntries.push_back(entry);
}

void BehaviorContext::RegisterEndBehavior(Behavior* behavior, const BehaviorBranchContext& branchContext, const LWOOBJID second) {
	BehaviorEndEntry entry;

	entry.behavior = behavior;
	entry.branchContext = branchContext;
	entry.second = second;
	entry.start = branchContext.start;

	this->endEntries.push_back(entry);
}

void BehaviorContext::ScheduleUpdate(const LWOOBJID id) {
	if (std::find(this->scheduledUpdates.begin(), this->scheduledUpdates.end(), id) != this->scheduledUpdates.end()) {
		return;
	}

	this->scheduledUpdates.push_back(id);
}

void BehaviorContext::ExecuteUpdates() {
	for (const auto& id : this->scheduledUpdates) {
		auto* entity = EntityManager::Instance()->GetEntity(id);

		if (entity == nullptr) continue;

		EntityManager::Instance()->SerializeEntity(entity);
	}

	this->scheduledUpdates.clear();
}

void BehaviorContext::SyncBehavior(const uint32_t syncId, RakNet::BitStream* bitStream) {
	BehaviorSyncEntry entry;
	auto found = false;

	/*
	 * There may be more than one of each handle
	 */
	for (auto i = 0u; i < this->syncEntries.size(); ++i) {
		const auto syncEntry = this->syncEntries.at(i);

		if (syncEntry.handle == syncId) {
			found = true;
			entry = syncEntry;

			this->syncEntries.erase(this->syncEntries.begin() + i);

			break;
		}
	}

	if (!found) {
		Game::logger->Log("BehaviorContext", "Failed to find behavior sync entry with sync id (%i)!", syncId);

		return;
	}

	auto* behavior = entry.behavior;
	const auto branch = entry.branchContext;

	if (behavior == nullptr) {
		Game::logger->Log("BehaviorContext", "Invalid behavior for sync id (%i)!", syncId);

		return;
	}

	behavior->Sync(this, bitStream, branch);
}


void BehaviorContext::Update(const float deltaTime) {
	for (auto i = 0u; i < this->timerEntries.size(); ++i) {
		auto entry = this->timerEntries.at(i);

		if (entry.time > 0) {
			entry.time -= deltaTime;

			this->timerEntries[i] = entry;
		}

		if (entry.time > 0) {
			continue;
		}

		entry.behavior->Timer(this, entry.branchContext, entry.second);
	}

	std::vector<BehaviorTimerEntry> valid;

	for (const auto& entry : this->timerEntries) {
		if (entry.time <= 0) {
			continue;
		}

		valid.push_back(entry);
	}

	this->timerEntries = valid;
}


void BehaviorContext::SyncCalculation(const uint32_t syncId, const float time, Behavior* behavior, const BehaviorBranchContext& branch, const bool ignoreInterrupts) {
	BehaviorSyncEntry entry;

	entry.behavior = behavior;
	entry.time = time;
	entry.branchContext = branch;
	entry.handle = syncId;
	entry.ignoreInterrupts = ignoreInterrupts;

	this->syncEntries.push_back(entry);
}

void BehaviorContext::InvokeEnd(const uint32_t id) {
	std::vector<BehaviorEndEntry> entries;

	for (const auto& entry : this->endEntries) {
		if (entry.start == id) {
			entry.behavior->End(this, entry.branchContext, entry.second);

			continue;
		}

		entries.push_back(entry);
	}

	this->endEntries = entries;
}

bool BehaviorContext::CalculateUpdate(const float deltaTime) {
	auto any = false;

	for (auto i = 0u; i < this->syncEntries.size(); ++i) {
		auto entry = this->syncEntries.at(i);

		if (entry.time > 0) {
			entry.time -= deltaTime;

			this->syncEntries[i] = entry;
		}

		if (entry.time > 0) {
			any = true;

			continue;
		}

		// Echo sync
		GameMessages::EchoSyncSkill echo;

		echo.bDone = true;
		echo.uiBehaviorHandle = entry.handle;
		echo.uiSkillHandle = this->skillUId;

		auto* bitStream = new RakNet::BitStream();

		// Calculate sync
		entry.behavior->SyncCalculation(this, bitStream, entry.branchContext);

		if (!clientInitalized) {
			echo.sBitStream.assign((char*)bitStream->GetData(), bitStream->GetNumberOfBytesUsed());

			// Write message
			RakNet::BitStream message;

			PacketUtils::WriteHeader(message, CLIENT, MSG_CLIENT_GAME_MSG);
			message.Write(this->originator);
			echo.Serialize(&message);

			Game::server->Send(&message, UNASSIGNED_SYSTEM_ADDRESS, true);
		}

		ExecuteUpdates();

		delete bitStream;
	}

	std::vector<BehaviorSyncEntry> valid;

	for (const auto& entry : this->syncEntries) {
		if (entry.time <= 0) {
			continue;
		}

		valid.push_back(entry);
	}

	this->syncEntries = valid;

	return any;
}

void BehaviorContext::Interrupt() {
	std::vector<BehaviorSyncEntry> keptSync{};

	for (const auto& entry : this->syncEntries) {
		if (!entry.ignoreInterrupts) continue;

		keptSync.push_back(entry);
	}

	this->syncEntries = keptSync;
}

void BehaviorContext::Reset() {
	for (const auto& entry : this->timerEntries) {
		entry.behavior->Timer(this, entry.branchContext, entry.second);
	}

	for (const auto& entry : this->endEntries) {
		entry.behavior->End(this, entry.branchContext, entry.second);
	}

	this->endEntries.clear();
	this->timerEntries.clear();
	this->syncEntries.clear();
	this->scheduledUpdates.clear();
}

std::vector<LWOOBJID> BehaviorContext::GetValidTargets(int32_t ignoreFaction, int32_t includeFaction, bool targetSelf, bool targetEnemy, bool targetFriend) const {
	auto* entity = EntityManager::Instance()->GetEntity(this->caster);

	std::vector<LWOOBJID> targets;

	if (entity == nullptr) {
		Game::logger->Log("BehaviorContext", "Invalid entity for (%llu)!", this->originator);

		return targets;
	}

	if (!ignoreFaction && !includeFaction) {
		for (auto entry : entity->GetTargetsInPhantom()) {
			auto* instance = EntityManager::Instance()->GetEntity(entry);

			if (instance == nullptr) {
				continue;
			}

			targets.push_back(entry);
		}
	}

	if (ignoreFaction || includeFaction || (!entity->HasComponent(COMPONENT_TYPE_PHANTOM_PHYSICS) && targets.empty())) {
		DestroyableComponent* destroyableComponent;
		if (!entity->TryGetComponent(COMPONENT_TYPE_DESTROYABLE, destroyableComponent)) {
			return targets;
		}

		auto entities = EntityManager::Instance()->GetEntitiesByComponent(COMPONENT_TYPE_CONTROLLABLE_PHYSICS);
		for (auto* candidate : entities) {
			const auto id = candidate->GetObjectID();

			if ((id != entity->GetObjectID() || targetSelf) && destroyableComponent->CheckValidity(id, ignoreFaction || includeFaction, targetEnemy, targetFriend)) {
				targets.push_back(id);
			}
		}
	}

	return targets;
}


BehaviorContext::BehaviorContext(const LWOOBJID originator, const bool calculation) {
	this->originator = originator;
	this->syncEntries = {};
	this->timerEntries = {};

	if (calculation) {
		this->skillUId = GetUniqueSkillId();
	} else {
		this->skillUId = 0;
	}
}

BehaviorContext::~BehaviorContext() {
	Reset();
}
