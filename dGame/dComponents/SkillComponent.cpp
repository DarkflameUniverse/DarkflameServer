/*
 * Darkflame Universe
 * Copyright 2018
 */

#include "SkillComponent.h"

#include <sstream>
#include <vector>

#include "BehaviorContext.h"
#include "BehaviorBranchContext.h"
#include "Behavior.h"
#include "CDClientDatabase.h"
#include "dServer.h"
#include "EntityManager.h"
#include "Game.h"
#include "PacketUtils.h"
#include "BaseCombatAIComponent.h"
#include "ScriptComponent.h"
#include "BuffComponent.h"


ProjectileSyncEntry::ProjectileSyncEntry() {
}

bool SkillComponent::CastPlayerSkill(const uint32_t behaviorId, const uint32_t skillUid, RakNet::BitStream* bitStream, const LWOOBJID target, uint32_t skillID) {
	auto* context = new BehaviorContext(this->m_Parent->GetObjectID());

	context->caster = m_Parent->GetObjectID();

	context->skillID = skillID;

	this->m_managedBehaviors.insert_or_assign(skillUid, context);

	auto* behavior = Behavior::CreateBehavior(behaviorId);

	const auto branch = BehaviorBranchContext(target, 0);

	behavior->Handle(context, bitStream, branch);

	context->ExecuteUpdates();

	return !context->failed;
}

void SkillComponent::SyncPlayerSkill(const uint32_t skillUid, const uint32_t syncId, RakNet::BitStream* bitStream) {
	const auto index = this->m_managedBehaviors.find(skillUid);

	if (index == this->m_managedBehaviors.end()) {
		Game::logger->Log("SkillComponent", "Failed to find skill with uid (%i)!", skillUid, syncId);

		return;
	}

	auto* context = index->second;

	context->SyncBehavior(syncId, bitStream);
}


void SkillComponent::SyncPlayerProjectile(const LWOOBJID projectileId, RakNet::BitStream* bitStream, const LWOOBJID target) {
	auto index = -1;

	for (auto i = 0u; i < this->m_managedProjectiles.size(); ++i) {
		const auto& projectile = this->m_managedProjectiles.at(i);

		if (projectile.id == projectileId) {
			index = i;

			break;
		}
	}

	if (index == -1) {
		Game::logger->Log("SkillComponent", "Failed to find projectile id (%llu)!", projectileId);

		return;
	}

	const auto sync_entry = this->m_managedProjectiles.at(index);

	auto query = CDClientDatabase::CreatePreppedStmt(
		"SELECT behaviorID FROM SkillBehavior WHERE skillID = (SELECT skillID FROM ObjectSkills WHERE objectTemplate = ?);");
	query.bind(1, (int)sync_entry.lot);

	auto result = query.execQuery();

	if (result.eof()) {
		Game::logger->Log("SkillComponent", "Failed to find skill id for (%i)!", sync_entry.lot);

		return;
	}

	const auto behavior_id = static_cast<uint32_t>(result.getIntField(0));

	result.finalize();

	auto* behavior = Behavior::CreateBehavior(behavior_id);

	auto branch = sync_entry.branchContext;

	branch.isProjectile = true;

	if (target != LWOOBJID_EMPTY) {
		branch.target = target;
	}

	behavior->Handle(sync_entry.context, bitStream, branch);

	this->m_managedProjectiles.erase(this->m_managedProjectiles.begin() + index);
}

void SkillComponent::RegisterPlayerProjectile(const LWOOBJID projectileId, BehaviorContext* context, const BehaviorBranchContext& branch, const LOT lot) {
	ProjectileSyncEntry entry;

	entry.context = context;
	entry.branchContext = branch;
	entry.lot = lot;
	entry.id = projectileId;

	this->m_managedProjectiles.push_back(entry);
}

void SkillComponent::Update(const float deltaTime) {
	if (!m_Parent->HasComponent(COMPONENT_TYPE_BASE_COMBAT_AI) && m_Parent->GetLOT() != 1) {
		CalculateUpdate(deltaTime);
	}

	std::map<uint32_t, BehaviorContext*> keep{};

	for (const auto& pair : this->m_managedBehaviors) {
		auto* context = pair.second;

		if (context == nullptr) {
			continue;
		}

		if (context->clientInitalized) {
			context->CalculateUpdate(deltaTime);
		} else {
			context->Update(deltaTime);
		}

		// Cleanup old behaviors
		if (context->syncEntries.empty() && context->timerEntries.empty()) {
			auto any = false;

			for (const auto& projectile : this->m_managedProjectiles) {
				if (projectile.context == context) {
					any = true;

					break;
				}
			}

			if (!any) {
				context->Reset();

				delete context;

				context = nullptr;

				continue;
			}
		}

		keep.insert_or_assign(pair.first, context);
	}

	this->m_managedBehaviors = keep;
}

void SkillComponent::Reset() {
	for (const auto& behavior : this->m_managedBehaviors) {
		delete behavior.second;
	}

	this->m_managedProjectiles.clear();
	this->m_managedBehaviors.clear();
}

void SkillComponent::Interrupt() {
	if (m_Parent->IsPlayer()) return;

	auto* combat = m_Parent->GetComponent<BaseCombatAIComponent>();

	if (combat != nullptr && combat->GetStunImmune()) {
		return;
	}

	for (const auto& behavior : this->m_managedBehaviors) {
		behavior.second->Interrupt();
	}
}

void SkillComponent::RegisterCalculatedProjectile(const LWOOBJID projectileId, BehaviorContext* context, const BehaviorBranchContext& branch, const LOT lot, const float maxTime,
	const NiPoint3& startPosition, const NiPoint3& velocity, const bool trackTarget, const float trackRadius) {
	ProjectileSyncEntry entry;

	entry.context = context;
	entry.branchContext = branch;
	entry.lot = lot;
	entry.calculation = true;
	entry.time = 0;
	entry.maxTime = maxTime;
	entry.id = projectileId;
	entry.startPosition = startPosition;
	entry.lastPosition = startPosition;
	entry.velocity = velocity;
	entry.trackTarget = trackTarget;
	entry.trackRadius = trackRadius;

	this->m_managedProjectiles.push_back(entry);
}


SkillExecutionResult SkillComponent::CalculateBehavior(const uint32_t skillId, const uint32_t behaviorId, const LWOOBJID target, const bool ignoreTarget, const bool clientInitalized, const LWOOBJID originatorOverride) {
	auto* bitStream = new RakNet::BitStream();

	auto* behavior = Behavior::CreateBehavior(behaviorId);

	auto* context = new BehaviorContext(originatorOverride != LWOOBJID_EMPTY ? originatorOverride : this->m_Parent->GetObjectID(), true);

	context->caster = m_Parent->GetObjectID();

	context->clientInitalized = clientInitalized;

	context->foundTarget = target != LWOOBJID_EMPTY || ignoreTarget || clientInitalized;

	behavior->Calculate(context, bitStream, { target, 0 });

	for (auto* script : CppScripts::GetEntityScripts(m_Parent)) {
		script->OnSkillCast(m_Parent, skillId);
	}

	if (!context->foundTarget) {
		delete bitStream;
		delete context;

		// Invalid attack
		return { false, 0 };
	}

	this->m_managedBehaviors.insert_or_assign(context->skillUId, context);

	if (!clientInitalized) {
		// Echo start skill
		GameMessages::EchoStartSkill start;

		start.iCastType = 0;
		start.skillID = skillId;
		start.uiSkillHandle = context->skillUId;
		start.optionalOriginatorID = context->originator;

		auto* originator = EntityManager::Instance()->GetEntity(context->originator);

		if (originator != nullptr) {
			start.originatorRot = originator->GetRotation();
		}
		//start.optionalTargetID = target;

		start.sBitStream.assign((char*)bitStream->GetData(), bitStream->GetNumberOfBytesUsed());

		// Write message
		RakNet::BitStream message;

		PacketUtils::WriteHeader(message, CLIENT, MSG_CLIENT_GAME_MSG);
		message.Write(this->m_Parent->GetObjectID());
		start.Serialize(&message);

		Game::server->Send(&message, UNASSIGNED_SYSTEM_ADDRESS, true);
	}

	context->ExecuteUpdates();

	delete bitStream;

	// Valid attack
	return { true, context->skillTime };
}

void SkillComponent::CalculateUpdate(const float deltaTime) {
	if (this->m_managedBehaviors.empty())
		return;

	for (const auto& managedBehavior : this->m_managedBehaviors) {
		if (managedBehavior.second == nullptr) {
			continue;
		}

		managedBehavior.second->CalculateUpdate(deltaTime);
	}

	for (auto& managedProjectile : this->m_managedProjectiles) {
		auto entry = managedProjectile;

		if (!entry.calculation) continue;

		entry.time += deltaTime;

		auto* origin = EntityManager::Instance()->GetEntity(entry.context->originator);

		if (origin == nullptr) {
			continue;
		}

		const auto targets = origin->GetTargetsInPhantom();

		const auto position = entry.startPosition + (entry.velocity * entry.time);

		for (const auto& targetId : targets) {
			auto* target = EntityManager::Instance()->GetEntity(targetId);

			const auto targetPosition = target->GetPosition();

			const auto closestPoint = Vector3::ClosestPointOnLine(entry.lastPosition, position, targetPosition);

			const auto distance = Vector3::DistanceSquared(targetPosition, closestPoint);

			if (distance > 3 * 3) {
				/*
				if (entry.TrackTarget && distance <= entry.TrackRadius)
				{
					const auto rotation = NiQuaternion::LookAtUnlocked(position, targetPosition);

					const auto speed = entry.Velocity.Length();

					const auto homingTarget = rotation.GetForwardVector() * speed;

					Vector3 homing;

					// Move towards

					const auto difference = homingTarget - entry.Velocity;
					const auto mag = difference.Length();
					if (mag <= speed || mag == 0)
					{
						homing = homingTarget;
					}
					else
					{
						entry.Velocity + homingTarget / mag * speed;
					}

					entry.Velocity = homing;
				}
				*/

				continue;
			}

			entry.branchContext.target = targetId;

			SyncProjectileCalculation(entry);

			entry.time = entry.maxTime;

			break;
		}

		entry.lastPosition = position;

		managedProjectile = entry;
	}

	std::vector<ProjectileSyncEntry> valid;

	for (auto& entry : this->m_managedProjectiles) {
		if (entry.calculation) {
			if (entry.time >= entry.maxTime) {
				entry.branchContext.target = LWOOBJID_EMPTY;

				SyncProjectileCalculation(entry);

				continue;
			}
		}

		valid.push_back(entry);
	}

	this->m_managedProjectiles = valid;
}


void SkillComponent::SyncProjectileCalculation(const ProjectileSyncEntry& entry) const {
	auto* other = EntityManager::Instance()->GetEntity(entry.branchContext.target);

	if (other == nullptr) {
		if (entry.branchContext.target != LWOOBJID_EMPTY) {
			Game::logger->Log("SkillComponent", "Invalid projectile target (%llu)!", entry.branchContext.target);
		}

		return;
	}

	auto query = CDClientDatabase::CreatePreppedStmt(
		"SELECT behaviorID FROM SkillBehavior WHERE skillID = (SELECT skillID FROM ObjectSkills WHERE objectTemplate = ?);");
	query.bind(1, (int)entry.lot);
	auto result = query.execQuery();

	if (result.eof()) {
		Game::logger->Log("SkillComponent", "Failed to find skill id for (%i)!", entry.lot);

		return;
	}

	const auto behaviorId = static_cast<uint32_t>(result.getIntField(0));

	result.finalize();

	auto* behavior = Behavior::CreateBehavior(behaviorId);

	auto* bitStream = new RakNet::BitStream();

	behavior->Calculate(entry.context, bitStream, entry.branchContext);

	GameMessages::DoClientProjectileImpact projectileImpact;

	projectileImpact.sBitStream.assign((char*)bitStream->GetData(), bitStream->GetNumberOfBytesUsed());
	projectileImpact.i64OwnerID = this->m_Parent->GetObjectID();
	projectileImpact.i64OrgID = entry.id;
	projectileImpact.i64TargetID = entry.branchContext.target;

	RakNet::BitStream message;

	PacketUtils::WriteHeader(message, CLIENT, MSG_CLIENT_GAME_MSG);
	message.Write(this->m_Parent->GetObjectID());
	projectileImpact.Serialize(&message);

	Game::server->Send(&message, UNASSIGNED_SYSTEM_ADDRESS, true);

	entry.context->ExecuteUpdates();

	delete bitStream;
}

void SkillComponent::HandleUnmanaged(const uint32_t behaviorId, const LWOOBJID target, LWOOBJID source) {
	auto* context = new BehaviorContext(source);

	context->unmanaged = true;
	context->caster = target;

	auto* behavior = Behavior::CreateBehavior(behaviorId);

	auto* bitStream = new RakNet::BitStream();

	behavior->Handle(context, bitStream, { target });

	delete bitStream;

	delete context;
}

void SkillComponent::HandleUnCast(const uint32_t behaviorId, const LWOOBJID target) {
	auto* context = new BehaviorContext(target);

	context->caster = target;

	auto* behavior = Behavior::CreateBehavior(behaviorId);

	behavior->UnCast(context, { target });

	delete context;
}

SkillComponent::SkillComponent(Entity* parent) : Component(parent) {
	this->m_skillUid = 0;
}

SkillComponent::~SkillComponent() {
	Reset();
}

void SkillComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	if (bIsInitialUpdate) outBitStream->Write0();
}

/// <summary>
///		Get a unique skill ID for syncing behaviors to the client
/// </summary>
/// <returns>Unique skill ID</returns>
uint32_t SkillComponent::GetUniqueSkillId() {
	return ++this->m_skillUid;
}
