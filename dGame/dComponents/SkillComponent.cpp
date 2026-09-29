/*
 * Darkflame Universe
 * Copyright 2018
 */

#include "SkillComponent.h"
#include "GeneralUtils.h"
#include "tinyxml2.h"

#include <sstream>
#include <vector>

#include "BehaviorContext.h"
#include "BehaviorBranchContext.h"
#include "Behavior.h"
#include "CDClientDatabase.h"
#include "dServer.h"
#include "EntityManager.h"
#include "Game.h"
#include "BitStreamUtils.h"
#include "BaseCombatAIComponent.h"
#include "ScriptComponent.h"
#include "BuffComponent.h"
#include "SkillMessages.h"
#include "CDClientManager.h"
#include "CDSkillBehaviorTable.h"
#include "ServiceType.h"
#include "MessageType/Client.h"

ProjectileSyncEntry::ProjectileSyncEntry() {
}

std::unordered_map<uint32_t, uint32_t> SkillComponent::m_skillBehaviorCache = {};

bool SkillComponent::CastPlayerSkill(const uint32_t behaviorId, const uint32_t skillUid, RakNet::BitStream& bitStream, const LWOOBJID target, uint32_t skillID) {
	auto* context = new BehaviorContext(this->m_Parent->GetObjectID());

	context->caster = m_Parent->GetObjectID();

	context->skillID = skillID;

	this->m_managedBehaviors.insert({ skillUid, context });

	auto* behavior = Behavior::CreateBehavior(behaviorId);

	const auto branch = BehaviorBranchContext(target, 0);

	behavior->Handle(context, bitStream, branch);

	context->ExecuteUpdates();

	if (!context->failed && m_Parent->IsPlayer()) StartCooldown(skillID);

	return !context->failed;
}

void SkillComponent::StartCooldown(const uint32_t skillID) {
	const auto& skill = CDClientManager::GetTable<CDSkillBehaviorTable>()->GetSkillByID(skillID);
	// The client keys cooldown groups 0 and up (a null group is -1); skills without a cooldown have none running
	const auto group = static_cast<int32_t>(skill.cooldowngroup);
	if (group < 0 || skill.cooldown <= 0.0f) return;
	auto& left = m_CooldownGroups[group];
	left = std::max(left, skill.cooldown);
}

void SkillComponent::LoadFromXml(const tinyxml2::XMLDocument& doc) {
	m_CooldownGroups.clear();
	const auto* const obj = doc.FirstChildElement("obj");
	const auto* const skil = obj ? obj->FirstChildElement("skil") : nullptr;
	const char* const cooldowns = skil ? skil->Attribute("sc") : nullptr;
	if (!cooldowns) return;

	// Alternating group and seconds, split on ';' and ':' as the client does
	const auto tokens = GeneralUtils::SplitString(std::string(cooldowns), ';');
	for (const auto& entry : tokens) {
		const auto pair = GeneralUtils::SplitString(entry, ':');
		if (pair.size() != 2) continue;
		const auto group = GeneralUtils::TryParse<int32_t>(pair[0]);
		const auto left = GeneralUtils::TryParse<float>(pair[1]);
		if (group && left && *left > 0.0f) m_CooldownGroups[*group] = *left;
	}
}

void SkillComponent::UpdateXml(tinyxml2::XMLDocument& doc) {
	auto* const obj = doc.FirstChildElement("obj");
	if (!obj) return;
	auto* skil = obj->FirstChildElement("skil");
	if (!skil) skil = obj->InsertNewChildElement("skil");

	// Live: <skil/> with nothing running, else e.g. <skil sc="17:15.6958;"/>
	std::ostringstream cooldowns;
	for (const auto& [group, left] : m_CooldownGroups) cooldowns << group << ':' << left << ';';
	if (cooldowns.view().empty()) skil->DeleteAttribute("sc");
	else skil->SetAttribute("sc", cooldowns.str().c_str());
}

void SkillComponent::SyncPlayerSkill(const uint32_t skillUid, const uint32_t syncId, RakNet::BitStream& bitStream) {
	const auto index = this->m_managedBehaviors.equal_range(skillUid);

	if (index.first == this->m_managedBehaviors.end()) {
		LOG("Failed to find skill with uid (%i)!", skillUid, syncId);

		return;
	}

	bool foundSyncId = false;
	for (auto it = index.first; it != index.second && !foundSyncId; ++it) {
		const auto& context = it->second;

		foundSyncId = context->SyncBehavior(syncId, bitStream);
	}

	if (!foundSyncId) {
		LOG("Failed to find sync id (%i) for skill with uid (%i)!", syncId, skillUid);
	}
}


void SkillComponent::SyncPlayerProjectile(const LWOOBJID projectileId, RakNet::BitStream& bitStream, const LWOOBJID target) {
	auto index = -1;

	for (auto i = 0u; i < this->m_managedProjectiles.size(); ++i) {
		const auto& projectile = this->m_managedProjectiles.at(i);

		if (projectile.id == projectileId) {
			index = i;

			break;
		}
	}

	if (index == -1) {
		LOG("Failed to find projectile id (%llu)!", projectileId);

		return;
	}

	const auto sync_entry = this->m_managedProjectiles.at(index);

	auto query = CDClientDatabase::CreatePreppedStmt(
		"SELECT behaviorID FROM SkillBehavior WHERE skillID = (SELECT skillID FROM ObjectSkills WHERE objectTemplate = ?);");
	query.bind(1, static_cast<int>(sync_entry.lot));

	auto result = query.execQuery();

	if (result.eof()) {
		LOG("Failed to find skill id for (%i)!", sync_entry.lot);

		return;
	}

	const auto behavior_id = static_cast<uint32_t>(result.getIntField("behaviorID"));

	result.finalize();

	auto* behavior = Behavior::CreateBehavior(behavior_id);

	auto branch = sync_entry.branchContext;

	branch.isProjectile = true;

	if (target != LWOOBJID_EMPTY) {
		branch.target = target;
	}

	behavior->Handle(sync_entry.context, bitStream, branch);

	this->m_managedProjectiles.erase(this->m_managedProjectiles.begin() + index);

	GameMessages::ActivityNotify notify;
	notify.notification.Insert<int32_t>(u"shot_done", sync_entry.skillId);
	m_Parent->OnActivityNotify(notify);
}

void SkillComponent::RegisterPlayerProjectile(const LWOOBJID projectileId, BehaviorContext* context, const BehaviorBranchContext& branch, const LOT lot) {
	ProjectileSyncEntry entry;

	entry.context = context;
	entry.branchContext = branch;
	entry.lot = lot;
	entry.id = projectileId;
	entry.skillId = context->skillID;

	this->m_managedProjectiles.push_back(entry);
}

void SkillComponent::Update(const float deltaTime) {
	for (auto it = m_CooldownGroups.begin(); it != m_CooldownGroups.end();) {
		it->second -= deltaTime;
		it = it->second <= 0.0f ? m_CooldownGroups.erase(it) : std::next(it);
	}

	if (!m_Parent->HasComponent(eReplicaComponentType::BASE_COMBAT_AI) && m_Parent->GetLOT() != 1) {
		CalculateUpdate(deltaTime);
	}

	if (m_Parent->IsPlayer()) {
		for (const auto& pair : this->m_managedBehaviors) pair.second->UpdatePlayerSyncs(deltaTime);
	}

	std::multimap<uint32_t, BehaviorContext*> keep{};

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

		keep.insert({ pair.first, context });
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

void SkillComponent::EndSkill(const uint32_t skillUid) {
	const auto [begin, end] = m_managedBehaviors.equal_range(skillUid);
	for (auto it = begin; it != end; ++it) {
		auto* const context = it->second;
		if (!context) continue;
		std::erase_if(m_managedProjectiles, [context](const ProjectileSyncEntry& projectile) { return projectile.context == context; });
		// End what the skill started, but drop its pending timers (Reset would run them now: a late hit)
		for (const auto& entry : context->endEntries) entry.behavior->End(context, entry.branchContext, entry.second);
		context->endEntries.clear();
		context->timerEntries.clear();
		context->syncEntries.clear();
		context->scheduledUpdates.clear();
		delete context;
	}
	m_managedBehaviors.erase(begin, end);
}

void SkillComponent::Interrupt() {
	// TODO: need to check immunities on the destroyable component, but they aren't implemented
	auto* combat = m_Parent->GetComponent<BaseCombatAIComponent>();
	if (combat != nullptr && combat->GetStunImmune()) return;

	for (const auto& behavior : this->m_managedBehaviors) {
		for (const auto& behaviorEndEntry : behavior.second->endEntries) {
			behaviorEndEntry.behavior->End(behavior.second, behaviorEndEntry.branchContext, behaviorEndEntry.second);
		}
		behavior.second->endEntries.clear();
		if (m_Parent->IsPlayer()) continue;
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

bool SkillComponent::CastSkill(const uint32_t skillId, LWOOBJID target, const LWOOBJID optionalOriginatorID, const int32_t castType, const NiQuaternion rotationOverride) {
	uint32_t behaviorId = -1;
	// try to find it via the cache
	const auto& pair = m_skillBehaviorCache.find(skillId);

	// if it's not in the cache look it up and cache it
	if (pair == m_skillBehaviorCache.end()) {
		auto skillTable = CDClientManager::GetTable<CDSkillBehaviorTable>();
		behaviorId = skillTable->GetSkillByID(skillId).behaviorID;
		m_skillBehaviorCache.insert_or_assign(skillId, behaviorId);
	} else {
		behaviorId = pair->second;
	}

	// check to see if we got back a valid behavior
	if (behaviorId == -1) {
		LOG_DEBUG("Tried to cast skill %i but found no behavior", skillId);
		return false;
	}

	return CalculateBehavior(skillId, behaviorId, target, false, false, optionalOriginatorID, castType, rotationOverride).success;
}


SkillExecutionResult SkillComponent::CalculateBehavior(
	const uint32_t skillId,
	const uint32_t behaviorId,
	const LWOOBJID target,
	const bool ignoreTarget,
	const bool clientInitalized,
	const LWOOBJID originatorOverride,
	const int32_t castType,
	const NiQuaternion rotationOverride) {
	RakNet::BitStream bitStream{};

	auto* behavior = Behavior::CreateBehavior(behaviorId);

	auto* context = new BehaviorContext(originatorOverride != LWOOBJID_EMPTY ? originatorOverride : this->m_Parent->GetObjectID(), true);

	context->caster = m_Parent->GetObjectID();

	context->skillID = skillId;

	context->clientInitalized = clientInitalized;

	context->foundTarget = target != LWOOBJID_EMPTY || ignoreTarget || clientInitalized;

	behavior->Calculate(context, bitStream, { target, 0 });

	m_Parent->GetScript()->OnSkillCast(m_Parent, skillId);

	if (!context->foundTarget) {
		delete context;

		// Invalid attack
		return { false, 0 };
	}

	this->m_managedBehaviors.insert({ context->skillUId, context });

	if (!clientInitalized) {
		// Echo start skill
		GameMessages::EchoStartSkill start;
		start.target = this->m_Parent->GetObjectID();

		start.iCastType = castType;
		start.skillID = skillId;
		start.uiSkillHandle = context->skillUId;
		start.optionalOriginatorID = context->originator;
		start.optionalTargetID = target;

		auto* originator = Game::entityManager->GetEntity(context->originator);

		if (originator != nullptr) {
			start.originatorRot = originator->GetRotation();
		}

		if (rotationOverride != QuatUtils::IDENTITY) {
			start.originatorRot = rotationOverride;
		}
		//start.optionalTargetID = target;

		start.sBitStream.assign(reinterpret_cast<char*>(bitStream.GetData()), bitStream.GetNumberOfBytesUsed());

		start.Send(UNASSIGNED_SYSTEM_ADDRESS);
	}

	context->ExecuteUpdates();

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

		auto* origin = Game::entityManager->GetEntity(entry.context->originator);

		if (origin == nullptr) {
			continue;
		}

		const auto targets = origin->GetTargetsInPhantom();

		const auto position = entry.startPosition + (entry.velocity * entry.time);

		for (const auto& targetId : targets) {
			auto* target = Game::entityManager->GetEntity(targetId);

			const auto targetPosition = target->GetPosition();

			const auto closestPoint = Vector3::ClosestPointOnLine(entry.lastPosition, position, targetPosition);

			const auto distance = Vector3::DistanceSquared(targetPosition, closestPoint);

			if (distance > 3 * 3) {
				// TODO There is supposed to be an implementation for homing projectiles here
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
	auto* other = Game::entityManager->GetEntity(entry.branchContext.target);

	if (other == nullptr) {
		if (entry.branchContext.target != LWOOBJID_EMPTY) {
			LOG("Invalid projectile target (%llu)!", entry.branchContext.target);
		}

		return;
	}

	auto query = CDClientDatabase::CreatePreppedStmt(
		"SELECT behaviorID FROM SkillBehavior WHERE skillID = (SELECT skillID FROM ObjectSkills WHERE objectTemplate = ?);");
	query.bind(1, static_cast<int>(entry.lot));
	auto result = query.execQuery();

	if (result.eof()) {
		LOG("Failed to find skill id for (%i)!", entry.lot);

		return;
	}

	const auto behaviorId = static_cast<uint32_t>(result.getIntField("behaviorID"));

	result.finalize();

	auto* behavior = Behavior::CreateBehavior(behaviorId);

	RakNet::BitStream bitStream{};

	behavior->Calculate(entry.context, bitStream, entry.branchContext);

	GameMessages::DoClientProjectileImpact projectileImpact;
	projectileImpact.target = this->m_Parent->GetObjectID();

	projectileImpact.sBitStream.assign(reinterpret_cast<char*>(bitStream.GetData()), bitStream.GetNumberOfBytesUsed());
	projectileImpact.i64OwnerID = this->m_Parent->GetObjectID();
	projectileImpact.i64OrgID = entry.id;
	projectileImpact.i64TargetID = entry.branchContext.target;

	projectileImpact.Send(UNASSIGNED_SYSTEM_ADDRESS);

	entry.context->ExecuteUpdates();
}

void SkillComponent::HandleUnmanaged(const uint32_t behaviorId, const LWOOBJID target, LWOOBJID source) {
	BehaviorContext context{ source };

	context.unmanaged = true;
	context.caster = target;

	auto* behavior = Behavior::CreateBehavior(behaviorId);

	RakNet::BitStream bitStream{};

	behavior->Handle(&context, bitStream, { target });
}

void SkillComponent::CalculateUnmanaged(const uint32_t behaviorId, const LWOOBJID caster, const LWOOBJID target) {
	BehaviorContext context{ caster, true };

	context.unmanaged = true;
	context.caster = caster;

	auto* behavior = Behavior::CreateBehavior(behaviorId);

	RakNet::BitStream bitStream{};

	behavior->Calculate(&context, bitStream, { target });
}

void SkillComponent::HandleUnCast(const uint32_t behaviorId, const LWOOBJID target) {
	BehaviorContext context{ target };

	context.caster = target;

	auto* behavior = Behavior::CreateBehavior(behaviorId);

	behavior->UnCast(&context, { target });
}

SkillComponent::SkillComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {
	this->m_skillUid = 0;
}

SkillComponent::~SkillComponent() {
	Reset();
}

void SkillComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	if (bIsInitialUpdate) outBitStream.Write0();
}

/// <summary>
///		Get a unique skill ID for syncing behaviors to the client
/// </summary>
/// <returns>Unique skill ID</returns>
uint32_t SkillComponent::GetUniqueSkillId() {
	return ++this->m_skillUid;
}
