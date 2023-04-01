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

#include "dMessageIdentifiers.h"
#include "DestroyableComponent.h"
#include "EchoSyncSkill.h"
#include "PhantomPhysicsComponent.h"
#include "RebuildComponent.h"
#include "eReplicaComponentType.h"
#include "TeamManager.h"

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


void BehaviorContext::RegisterSyncBehavior(const uint32_t syncId, Behavior* behavior, const BehaviorBranchContext& branchContext, bool ignoreInterrupts) {
	auto entry = BehaviorSyncEntry();

	entry.handle = syncId;
	entry.behavior = behavior;
	entry.branchContext = branchContext;
	entry.branchContext.isSync = true;
	entry.ignoreInterrupts = ignoreInterrupts;

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
		EchoSyncSkill echo;

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

void BehaviorContext::FilterTargets(std::vector<Entity*>& targets, std::forward_list<int32_t>& ignoreFactionList, std::forward_list<int32_t>& includeFactionList, bool targetSelf, bool targetEnemy, bool targetFriend, bool targetTeam) const {

	// if we aren't targeting anything, then clear the targets vector
	if (!targetSelf && !targetEnemy && !targetFriend && !targetTeam && ignoreFactionList.empty() && includeFactionList.empty()) {
		targets.clear();
	} else {
		auto* caster = EntityManager::Instance()->GetEntity(this->caster);

		// if the caster is not there, return empty targets list
		if (!caster) {
			Game::logger->LogDebug("BehaviorContext", "Invalid caster for (%llu)!", this->originator);
			targets.clear();
			return;
		}

		auto* casterDestroyableComponent = caster->GetComponent<DestroyableComponent>();
		if (!casterDestroyableComponent) return;

		auto index = targets.begin();
		while (index != targets.end()) {
			auto candidate = *index;
			if (!candidate)index = targets.erase(index);
			else if (candidate == caster){
				if (!targetSelf) index = targets.erase(index);
				else index++;
			} else {
				if (!CheckTargetingRequirements(candidate)) {
					index = targets.erase(index);
				} else {
					// get factions to check against
					// CheckTargetingRequirements checks for a destroyable component
					// but we check again because bounds check are great!
					auto candidateDestroyableComponent = candidate->GetComponent<DestroyableComponent>();
					if (!candidateDestroyableComponent) {
						index = targets.erase(index);
						continue;
					}
					auto candidateFactions = candidateDestroyableComponent->GetFactionIDs();

					if (candidateFactions.empty()){
						index = targets.erase(index);
					} else {
						if (candidateDestroyableComponent->GetIsDead()) {
							index = targets.erase(index);
							continue;
						}
						if (!CheckFactionList(includeFactionList, candidateFactions)){
							if (targetTeam){
								auto* team = TeamManager::Instance()->GetTeam(this->caster);
								if (team){
									if(std::find(team->members.begin(), team->members.end(), candidate->GetObjectID()) != team->members.end()){
										index++;
										continue;
									}
								}
							}
							auto isEnemy = casterDestroyableComponent->IsEnemy(candidate);
							if (!targetFriend && !isEnemy) index = targets.erase(index);
							else if(!targetEnemy && isEnemy) index = targets.erase(index);
							else if(CheckFactionList(ignoreFactionList, candidateFactions))	index = targets.erase(index);
							else index++;
							continue;
						}
						index++;
					}
				}
			}
			continue;
		}
	}
	return;
}

// some basic checks as well as the check that matters for this: if the quickbuild is complete
bool BehaviorContext::CheckTargetingRequirements(const Entity* target) const {
	// if the target is a nullptr, then it's not valid
	if (!target) return false;

	// ignore entities that don't have destroyable components
	auto* targetDestroyableComponent = target->GetComponent<DestroyableComponent>();
	if (!targetDestroyableComponent) return false;

	// ignore quickbuilds that aren't completed
	auto* targetQuickbuildComponent = target->GetComponent<RebuildComponent>();
	if (targetQuickbuildComponent && targetQuickbuildComponent->GetState() != REBUILD_COMPLETED) return false;

	return true;
}

// returns true if any of the object factions are in the faction list
bool BehaviorContext::CheckFactionList(std::forward_list<int32_t>& factionList, std::vector<int32_t>& objectsFactions) const {
	if (factionList.empty() || objectsFactions.empty()) return false;
	for (auto faction : factionList){
		if(std::find(objectsFactions.begin(), objectsFactions.end(), faction) != objectsFactions.end()) return true;
	}
	return false;
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
