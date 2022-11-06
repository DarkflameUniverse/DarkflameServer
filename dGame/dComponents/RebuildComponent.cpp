#include "RebuildComponent.h"
#include "Entity.h"
#include "DestroyableComponent.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "Game.h"
#include "dLogger.h"
#include "CharacterComponent.h"
#include "MissionComponent.h"
#include "MissionTaskType.h"

#include "dServer.h"
#include "PacketUtils.h"
#include "Spawner.h"
#include "MovingPlatformComponent.h"
#include "Preconditions.h"

#include "CppScripts.h"

RebuildComponent::RebuildComponent(Entity* entity) : Component(entity) {
	std::u16string checkPreconditions = entity->GetVar<std::u16string>(u"CheckPrecondition");

	if (!checkPreconditions.empty()) {
		m_Precondition = new PreconditionExpression(GeneralUtils::UTF16ToWTF8(checkPreconditions));
	}

	// Should a setting that has the build activator position exist, fetch that setting here and parse it for position.
	// It is assumed that the user who sets this setting uses the correct character delimiter (character 31 or in hex 0x1F)
	auto positionAsVector = GeneralUtils::SplitString(m_Parent->GetVarAsString(u"rebuild_activators"), 0x1F);
	if (positionAsVector.size() == 3 &&
		GeneralUtils::TryParse(positionAsVector[0], m_ActivatorPosition.x) &&
		GeneralUtils::TryParse(positionAsVector[1], m_ActivatorPosition.y) &&
		GeneralUtils::TryParse(positionAsVector[2], m_ActivatorPosition.z)) {
	} else {
		Game::logger->Log("RebuildComponent", "Failed to find activator position for lot %i.  Defaulting to parents position.", m_Parent->GetLOT());
		m_ActivatorPosition = m_Parent->GetPosition();
	}

	SpawnActivator();
}

RebuildComponent::~RebuildComponent() {
	delete m_Precondition;

	Entity* builder = GetBuilder();
	if (builder) {
		CancelRebuild(builder, eFailReason::REASON_BUILD_ENDED, true);
	}

	DespawnActivator();
}

void RebuildComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	if (m_Parent->GetComponent(COMPONENT_TYPE_DESTROYABLE) == nullptr) {
		if (bIsInitialUpdate) {
			outBitStream->Write(false);
		}

		outBitStream->Write(false);

		outBitStream->Write(false);
	}
	// If build state is completed and we've already serialized once in the completed state,
	// don't serializing this component anymore as this will cause the build to jump again.
	// If state changes, serialization will begin again.
	if (!m_StateDirty && m_State == REBUILD_COMPLETED) {
		outBitStream->Write0();
		outBitStream->Write0();
		return;
	}
	// BEGIN Scripted Activity
	outBitStream->Write1();

	Entity* builder = GetBuilder();

	if (builder) {
		outBitStream->Write((uint32_t)1);
		outBitStream->Write(builder->GetObjectID());

		for (int i = 0; i < 10; i++) {
			outBitStream->Write(0.0f);
		}
	} else {
		outBitStream->Write((uint32_t)0);
	}
	// END Scripted Activity

	outBitStream->Write1();

	outBitStream->Write<uint32_t>(m_State);

	outBitStream->Write(m_ShowResetEffect);
	outBitStream->Write(m_Activator != nullptr);

	outBitStream->Write(m_Timer);
	outBitStream->Write(m_TimerIncomplete);

	if (bIsInitialUpdate) {
		outBitStream->Write(false);
		outBitStream->Write(m_ActivatorPosition);
		outBitStream->Write(m_RepositionPlayer);
	}
	m_StateDirty = false;
}

void RebuildComponent::Update(float deltaTime) {
	m_Activator = GetActivator();

	// Serialize the quickbuild every so often, fixes the odd bug where the quickbuild is not buildable
	/*if (m_SoftTimer > 0.0f) {
		m_SoftTimer -= deltaTime;
	}
	else {
		m_SoftTimer = 5.0f;

		EntityManager::Instance()->SerializeEntity(m_Parent);
	}*/

	switch (m_State) {
	case REBUILD_OPEN: {
		SpawnActivator();
		m_TimeBeforeDrain = 0;

		auto* spawner = m_Parent->GetSpawner();
		const bool isSmashGroup = spawner != nullptr ? spawner->GetIsSpawnSmashGroup() : false;

		if (isSmashGroup) {
			m_TimerIncomplete += deltaTime;

			// For reset times < 0 this has to be handled manually
			if (m_TimeBeforeSmash > 0) {
				if (m_TimerIncomplete >= m_TimeBeforeSmash - 4.0f) {
					m_ShowResetEffect = true;

					EntityManager::Instance()->SerializeEntity(m_Parent);
				}

				if (m_TimerIncomplete >= m_TimeBeforeSmash) {
					m_Builder = LWOOBJID_EMPTY;

					GameMessages::SendDieNoImplCode(m_Parent, LWOOBJID_EMPTY, LWOOBJID_EMPTY, eKillType::VIOLENT, u"", 0.0f, 0.0f, 0.0f, false, true);

					ResetRebuild(false);
				}
			}
		}

		break;
	}
	case REBUILD_COMPLETED: {
		m_Timer += deltaTime;

		// For reset times < 0 this has to be handled manually
		if (m_ResetTime > 0) {
			if (m_Timer >= m_ResetTime - 4.0f) {
				if (!m_ShowResetEffect) {
					m_ShowResetEffect = true;

					EntityManager::Instance()->SerializeEntity(m_Parent);
				}
			}

			if (m_Timer >= m_ResetTime) {

				GameMessages::SendDieNoImplCode(m_Parent, LWOOBJID_EMPTY, LWOOBJID_EMPTY, eKillType::VIOLENT, u"", 0.0f, 0.0f, 0.0f, false, true);

				ResetRebuild(false);
			}
		}
		break;
	}
	case REBUILD_BUILDING:
	{
		Entity* builder = GetBuilder();

		if (builder == nullptr) {
			ResetRebuild(false);

			return;
		}

		m_TimeBeforeDrain -= deltaTime;
		m_Timer += deltaTime;
		m_TimerIncomplete = 0;
		m_ShowResetEffect = false;

		if (m_TimeBeforeDrain <= 0.0f) {
			m_TimeBeforeDrain = m_CompleteTime / static_cast<float>(m_TakeImagination);

			DestroyableComponent* destComp = builder->GetComponent<DestroyableComponent>();
			if (!destComp) break;

			int newImagination = destComp->GetImagination() - 1;

			destComp->SetImagination(newImagination);
			EntityManager::Instance()->SerializeEntity(builder);

			++m_DrainedImagination;

			if (newImagination == 0 && m_DrainedImagination < m_TakeImagination) {
				CancelRebuild(builder, eFailReason::REASON_OUT_OF_IMAGINATION, true);

				break;
			}
		}

		if (m_Timer >= m_CompleteTime && m_DrainedImagination >= m_TakeImagination) {
			CompleteRebuild(builder);
		}

		break;
	}
	case REBUILD_INCOMPLETE: {
		m_TimerIncomplete += deltaTime;

		// For reset times < 0 this has to be handled manually
		if (m_TimeBeforeSmash > 0) {
			if (m_TimerIncomplete >= m_TimeBeforeSmash - 4.0f) {
				m_ShowResetEffect = true;

				EntityManager::Instance()->SerializeEntity(m_Parent);
			}

			if (m_TimerIncomplete >= m_TimeBeforeSmash) {
				m_Builder = LWOOBJID_EMPTY;

				GameMessages::SendDieNoImplCode(m_Parent, LWOOBJID_EMPTY, LWOOBJID_EMPTY, eKillType::VIOLENT, u"", 0.0f, 0.0f, 0.0f, false, true);

				ResetRebuild(false);
			}
		}
		break;
	}
	}
}

void RebuildComponent::OnUse(Entity* originator) {
	if (GetBuilder() != nullptr || m_State == REBUILD_COMPLETED) {
		return;
	}

	if (m_Precondition != nullptr && !m_Precondition->Check(originator)) {
		return;
	}

	StartRebuild(originator);
}

void RebuildComponent::SpawnActivator() {
	if (!m_SelfActivator || m_ActivatorPosition != NiPoint3::ZERO) {
		if (!m_Activator) {
			EntityInfo info;

			info.lot = 6604;
			info.spawnerID = m_Parent->GetObjectID();
			info.pos = m_ActivatorPosition == NiPoint3::ZERO ? m_Parent->GetPosition() : m_ActivatorPosition;

			m_Activator = EntityManager::Instance()->CreateEntity(info, nullptr, m_Parent);
			if (m_Activator) {
				m_ActivatorId = m_Activator->GetObjectID();
				EntityManager::Instance()->ConstructEntity(m_Activator);
			}

			EntityManager::Instance()->SerializeEntity(m_Parent);
		}
	}
}

void RebuildComponent::DespawnActivator() {
	if (m_Activator) {
		EntityManager::Instance()->DestructEntity(m_Activator);

		m_Activator->ScheduleKillAfterUpdate();

		m_Activator = nullptr;

		m_ActivatorId = LWOOBJID_EMPTY;
	}
}

Entity* RebuildComponent::GetActivator() {
	return EntityManager::Instance()->GetEntity(m_ActivatorId);
}

NiPoint3 RebuildComponent::GetActivatorPosition() {
	return m_ActivatorPosition;
}

float RebuildComponent::GetResetTime() {
	return m_ResetTime;
}

float RebuildComponent::GetCompleteTime() {
	return m_CompleteTime;
}

int RebuildComponent::GetTakeImagination() {
	return m_TakeImagination;
}

bool RebuildComponent::GetInterruptible() {
	return m_Interruptible;
}

bool RebuildComponent::GetSelfActivator() {
	return m_SelfActivator;
}

std::vector<int> RebuildComponent::GetCustomModules() {
	return m_CustomModules;
}

int RebuildComponent::GetActivityId() {
	return m_ActivityId;
}

int RebuildComponent::GetPostImaginationCost() {
	return m_PostImaginationCost;
}

float RebuildComponent::GetTimeBeforeSmash() {
	return m_TimeBeforeSmash;
}

eRebuildState RebuildComponent::GetState() {
	return m_State;
}

Entity* RebuildComponent::GetBuilder() const {
	auto* builder = EntityManager::Instance()->GetEntity(m_Builder);

	return builder;
}

bool RebuildComponent::GetRepositionPlayer() const {
	return m_RepositionPlayer;
}

void RebuildComponent::SetActivatorPosition(NiPoint3 value) {
	m_ActivatorPosition = value;
}

void RebuildComponent::SetResetTime(float value) {
	m_ResetTime = value;
}

void RebuildComponent::SetCompleteTime(float value) {
	if (value < 0) {
		m_CompleteTime = 4.5f;
	} else {
		m_CompleteTime = value;
	}
}

void RebuildComponent::SetTakeImagination(int value) {
	m_TakeImagination = value;
}

void RebuildComponent::SetInterruptible(bool value) {
	m_Interruptible = value;
}

void RebuildComponent::SetSelfActivator(bool value) {
	m_SelfActivator = value;
}

void RebuildComponent::SetCustomModules(std::vector<int> value) {
	m_CustomModules = value;
}

void RebuildComponent::SetActivityId(int value) {
	m_ActivityId = value;
}

void RebuildComponent::SetPostImaginationCost(int value) {
	m_PostImaginationCost = value;
}

void RebuildComponent::SetTimeBeforeSmash(float value) {
	if (value < 0) {
		m_TimeBeforeSmash = 10.0f;
	} else {
		m_TimeBeforeSmash = value;
	}
}

void RebuildComponent::SetRepositionPlayer(bool value) {
	m_RepositionPlayer = value;
}

void RebuildComponent::StartRebuild(Entity* user) {
	if (m_State == eRebuildState::REBUILD_OPEN || m_State == eRebuildState::REBUILD_COMPLETED || m_State == eRebuildState::REBUILD_INCOMPLETE) {
		m_Builder = user->GetObjectID();

		auto* character = user->GetComponent<CharacterComponent>();
		character->SetCurrentActivity(eGameActivities::ACTIVITY_QUICKBUILDING);

		EntityManager::Instance()->SerializeEntity(user);

		GameMessages::SendRebuildNotifyState(m_Parent, m_State, eRebuildState::REBUILD_BUILDING, user->GetObjectID());
		GameMessages::SendEnableRebuild(m_Parent, true, false, false, eFailReason::REASON_NOT_GIVEN, 0.0f, user->GetObjectID());

		m_State = eRebuildState::REBUILD_BUILDING;
		m_StateDirty = true;
		EntityManager::Instance()->SerializeEntity(m_Parent);

		auto* movingPlatform = m_Parent->GetComponent<MovingPlatformComponent>();
		if (movingPlatform != nullptr) {
			movingPlatform->OnRebuildInitilized();
		}

		for (auto* script : CppScripts::GetEntityScripts(m_Parent)) {
			script->OnRebuildStart(m_Parent, user);
		}

		// Notify scripts and possible subscribers
		for (auto* script : CppScripts::GetEntityScripts(m_Parent))
			script->OnRebuildNotifyState(m_Parent, m_State);
		for (const auto& cb : m_RebuildStateCallbacks)
			cb(m_State);
	}
}

void RebuildComponent::CompleteRebuild(Entity* user) {
	if (user == nullptr) {
		return;
	}

	auto* characterComponent = user->GetComponent<CharacterComponent>();
	if (characterComponent != nullptr) {
		characterComponent->SetCurrentActivity(eGameActivities::ACTIVITY_NONE);
		characterComponent->TrackRebuildComplete();
	} else {
		Game::logger->Log("RebuildComponent", "Some user tried to finish the rebuild but they didn't have a character somehow.");
		return;
	}

	EntityManager::Instance()->SerializeEntity(user);

	GameMessages::SendRebuildNotifyState(m_Parent, m_State, eRebuildState::REBUILD_COMPLETED, user->GetObjectID());
	GameMessages::SendPlayFXEffect(m_Parent, 507, u"create", "BrickFadeUpVisCompleteEffect", LWOOBJID_EMPTY, 0.4f, 1.0f, true);
	GameMessages::SendEnableRebuild(m_Parent, false, false, true, eFailReason::REASON_NOT_GIVEN, m_ResetTime, user->GetObjectID());
	GameMessages::SendTerminateInteraction(user->GetObjectID(), eTerminateType::FROM_INTERACTION, m_Parent->GetObjectID());


	m_State = eRebuildState::REBUILD_COMPLETED;
	m_StateDirty = true;
	m_Timer = 0.0f;
	m_DrainedImagination = 0;

	EntityManager::Instance()->SerializeEntity(m_Parent);

	// Removes extra item requirements, isn't live accurate.
	// In live, all items were removed at the start of the quickbuild, then returned if it was cancelled.
	// TODO: fix?
	if (m_Precondition != nullptr) {
		m_Precondition->Check(user, true);
	}

	DespawnActivator();

	// Set owner override so that entities smashed by this quickbuild will result in the builder getting rewards.
	m_Parent->SetOwnerOverride(user->GetObjectID());

	auto* builder = GetBuilder();

	if (builder != nullptr) {
		auto* missionComponent = builder->GetComponent<MissionComponent>();
		if (missionComponent != nullptr) {
			missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_ACTIVITY, m_ActivityId);
		}

		LootGenerator::Instance().DropActivityLoot(builder, m_Parent, m_ActivityId, 1);
	}

	// Notify scripts
	for (auto* script : CppScripts::GetEntityScripts(m_Parent)) {
		script->OnRebuildComplete(m_Parent, user);
		script->OnRebuildNotifyState(m_Parent, m_State);
	}

	// Notify subscribers
	for (const auto& callback : m_RebuildStateCallbacks)
		callback(m_State);
	for (const auto& callback : m_RebuildCompleteCallbacks)
		callback(user);

	auto* movingPlatform = m_Parent->GetComponent<MovingPlatformComponent>();
	if (movingPlatform != nullptr) {
		movingPlatform->OnCompleteRebuild();
	}

	// Set flag
	auto* character = user->GetCharacter();

	if (character != nullptr) {
		const auto flagNumber = m_Parent->GetVar<int32_t>(u"quickbuild_single_build_player_flag");

		if (flagNumber != 0) {
			character->SetPlayerFlag(flagNumber, true);
		}
	}
	GameMessages::SendPlayAnimation(user, u"rebuild-celebrate", 1.09f);
}

void RebuildComponent::ResetRebuild(bool failed) {
	Entity* builder = GetBuilder();

	if (m_State == eRebuildState::REBUILD_BUILDING && builder) {
		GameMessages::SendEnableRebuild(m_Parent, false, false, failed, eFailReason::REASON_NOT_GIVEN, m_ResetTime, builder->GetObjectID());

		if (failed) {
			GameMessages::SendPlayAnimation(builder, u"rebuild-fail");
		}
	}

	GameMessages::SendRebuildNotifyState(m_Parent, m_State, eRebuildState::REBUILD_RESETTING, LWOOBJID_EMPTY);

	m_State = eRebuildState::REBUILD_RESETTING;
	m_StateDirty = true;
	m_Timer = 0.0f;
	m_TimerIncomplete = 0.0f;
	m_ShowResetEffect = false;
	m_DrainedImagination = 0;

	EntityManager::Instance()->SerializeEntity(m_Parent);

	// Notify scripts and possible subscribers
	for (auto* script : CppScripts::GetEntityScripts(m_Parent))
		script->OnRebuildNotifyState(m_Parent, m_State);
	for (const auto& cb : m_RebuildStateCallbacks)
		cb(m_State);

	m_Parent->ScheduleKillAfterUpdate();

	if (m_Activator) {
		m_Activator->ScheduleKillAfterUpdate();
	}
}

void RebuildComponent::CancelRebuild(Entity* entity, eFailReason failReason, bool skipChecks) {
	if (m_State != eRebuildState::REBUILD_COMPLETED || skipChecks) {

		m_Builder = LWOOBJID_EMPTY;

		const auto entityID = entity != nullptr ? entity->GetObjectID() : LWOOBJID_EMPTY;

		// Notify the client that a state has changed
		GameMessages::SendRebuildNotifyState(m_Parent, m_State, eRebuildState::REBUILD_INCOMPLETE, entityID);
		GameMessages::SendEnableRebuild(m_Parent, false, true, false, failReason, m_Timer, entityID);

		// Now terminate any interaction with the rebuild
		GameMessages::SendTerminateInteraction(entityID, eTerminateType::FROM_INTERACTION, m_Parent->GetObjectID());
		GameMessages::SendTerminateInteraction(m_Parent->GetObjectID(), eTerminateType::FROM_INTERACTION, m_Parent->GetObjectID());

		// Now update the component itself
		m_State = eRebuildState::REBUILD_INCOMPLETE;
		m_StateDirty = true;

		// Notify scripts and possible subscribers
		for (auto* script : CppScripts::GetEntityScripts(m_Parent))
			script->OnRebuildNotifyState(m_Parent, m_State);
		for (const auto& cb : m_RebuildStateCallbacks)
			cb(m_State);

		EntityManager::Instance()->SerializeEntity(m_Parent);
	}

	if (entity == nullptr) {
		return;
	}

	CharacterComponent* characterComponent = entity->GetComponent<CharacterComponent>();
	if (characterComponent) {
		characterComponent->SetCurrentActivity(eGameActivities::ACTIVITY_NONE);
		EntityManager::Instance()->SerializeEntity(entity);
	}
}

void RebuildComponent::AddRebuildCompleteCallback(const std::function<void(Entity* user)>& callback) {
	m_RebuildCompleteCallbacks.push_back(callback);
}

void RebuildComponent::AddRebuildStateCallback(const std::function<void(eRebuildState state)>& callback) {
	m_RebuildStateCallbacks.push_back(callback);
}
