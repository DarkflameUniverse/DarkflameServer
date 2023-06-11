#include "QuickBuildComponent.h"
#include "Entity.h"
#include "DestroyableComponent.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "Game.h"
#include "dLogger.h"
#include "CharacterComponent.h"
#include "MissionComponent.h"
#include "eMissionTaskType.h"
#include "eTriggerEventType.h"
#include "eQuickBuildFailReason.h"
#include "eTerminateType.h"
#include "eGameActivity.h"

#include "dServer.h"
#include "PacketUtils.h"
#include "Spawner.h"
#include "MovingPlatformComponent.h"
#include "Preconditions.h"
#include "Loot.h"
#include "TeamManager.h"
#include "RenderComponent.h"
#include "CDRebuildComponentTable.h"

#include "CppScripts.h"

QuickBuildComponent::QuickBuildComponent(Entity* entity, uint32_t componentId) : Component(entity) {
	m_ComponentId = componentId;
	std::u16string checkPreconditions = entity->GetVar<std::u16string>(u"CheckPrecondition");

	if (!checkPreconditions.empty()) {
		m_Precondition = new PreconditionExpression(GeneralUtils::UTF16ToWTF8(checkPreconditions));
	}

	// Should a setting that has the build activator position exist, fetch that setting here and parse it for position.
	// It is assumed that the user who sets this setting uses the correct character delimiter (character 31 or in hex 0x1F)
	auto positionAsVector = GeneralUtils::SplitString(m_ParentEntity->GetVarAsString(u"rebuild_activators"), 0x1F);
	if (positionAsVector.size() == 3 &&
		GeneralUtils::TryParse(positionAsVector[0], m_ActivatorPosition.x) &&
		GeneralUtils::TryParse(positionAsVector[1], m_ActivatorPosition.y) &&
		GeneralUtils::TryParse(positionAsVector[2], m_ActivatorPosition.z)) {
	} else {
		Game::logger->Log("QuickBuildComponent", "Failed to find activator position for lot %i.  Defaulting to parents position.", m_ParentEntity->GetLOT());
		m_ActivatorPosition = m_ParentEntity->GetPosition();
	}

	SpawnActivator();
}

QuickBuildComponent::~QuickBuildComponent() {
	delete m_Precondition;

	Entity* builder = GetBuilder();
	if (builder) {
		CancelRebuild(builder, eQuickBuildFailReason::BUILD_ENDED, true);
	}

	DespawnActivator();
}

void QuickBuildComponent::LoadConfigData() {
	const auto rebuildResetTime = m_ParentEntity->GetVar<float>(u"rebuild_reset_time");

	if (rebuildResetTime != 0.0f) {
		SetResetTime(rebuildResetTime);

		if (m_ParentEntity->GetLOT() == 9483) // Look away!
		{
			SetResetTime(GetResetTime() + 25);
		}
	}

	const auto activityID = m_ParentEntity->GetVar<int32_t>(u"activityID");

	if (activityID > 0) SetActivityId(activityID);

	const auto compTime = m_ParentEntity->GetVar<float>(u"compTime");

	if (compTime > 0) SetCompleteTime(compTime);
}

void QuickBuildComponent::LoadTemplateData() {
	auto* rebCompTable = CDClientManager::Instance().GetTable<CDRebuildComponentTable>();
	std::vector<CDRebuildComponent> rebCompData = rebCompTable->Query([this](CDRebuildComponent entry) { return (entry.id == this->m_ComponentId); });
	if (rebCompData.empty()) return;
	const auto& quickbuildData = rebCompData.at(0);
	SetResetTime(quickbuildData.reset_time);
	SetCompleteTime(quickbuildData.complete_time);
	SetTakeImagination(quickbuildData.take_imagination);
	SetInterruptible(quickbuildData.interruptible);
	SetSelfActivator(quickbuildData.self_activator);
	SetActivityId(quickbuildData.activityID);
	SetPostImaginationCost(quickbuildData.post_imagination_cost);
	SetTimeBeforeSmash(quickbuildData.time_before_smash);
}

void QuickBuildComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	if (!m_ParentEntity->GetComponent<DestroyableComponent>()) {
		if (bIsInitialUpdate) {
			outBitStream->Write(false);
		}

		outBitStream->Write(false);

		outBitStream->Write(false);
	}
	// If build state is completed and we've already serialized once in the completed state,
	// don't serializing this component anymore as this will cause the build to jump again.
	// If state changes, serialization will begin again.
	if (!m_StateDirty && m_State == eRebuildState::COMPLETED) {
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

	outBitStream->Write(m_State);

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

void QuickBuildComponent::Update(float deltaTime) {
	m_Activator = GetActivator();

	// Serialize the quickbuild every so often, fixes the odd bug where the quickbuild is not buildable
	/*if (m_SoftTimer > 0.0f) {
		m_SoftTimer -= deltaTime;
	}
	else {
		m_SoftTimer = 5.0f;

		EntityManager::Instance()->SerializeEntity(m_ParentEntity);
	}*/

	switch (m_State) {
	case eRebuildState::OPEN: {
		SpawnActivator();
		m_TimeBeforeDrain = 0;

		auto* spawner = m_ParentEntity->GetSpawner();
		const bool isSmashGroup = spawner != nullptr ? spawner->GetIsSpawnSmashGroup() : false;

		if (isSmashGroup) {
			m_TimerIncomplete += deltaTime;

			// For reset times < 0 this has to be handled manually
			if (m_TimeBeforeSmash > 0) {
				if (m_TimerIncomplete >= m_TimeBeforeSmash - 4.0f) {
					m_ShowResetEffect = true;

					EntityManager::Instance()->SerializeEntity(m_ParentEntity);
				}

				if (m_TimerIncomplete >= m_TimeBeforeSmash) {
					m_Builder = LWOOBJID_EMPTY;

					GameMessages::SendDieNoImplCode(m_ParentEntity, LWOOBJID_EMPTY, LWOOBJID_EMPTY, eKillType::VIOLENT, u"", 0.0f, 0.0f, 0.0f, false, true);

					ResetRebuild(false);
				}
			}
		}

		break;
	}
	case eRebuildState::COMPLETED: {
		m_Timer += deltaTime;

		// For reset times < 0 this has to be handled manually
		if (m_ResetTime > 0) {
			if (m_Timer >= m_ResetTime - 4.0f) {
				if (!m_ShowResetEffect) {
					m_ShowResetEffect = true;

					EntityManager::Instance()->SerializeEntity(m_ParentEntity);
				}
			}

			if (m_Timer >= m_ResetTime) {

				GameMessages::SendDieNoImplCode(m_ParentEntity, LWOOBJID_EMPTY, LWOOBJID_EMPTY, eKillType::VIOLENT, u"", 0.0f, 0.0f, 0.0f, false, true);

				ResetRebuild(false);
			}
		}
		break;
	}
	case eRebuildState::BUILDING:
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

			auto* destComp = builder->GetComponent<DestroyableComponent>();
			if (!destComp) break;

			int newImagination = destComp->GetImagination();
			if (newImagination <= 0) {
				CancelRebuild(builder, eQuickBuildFailReason::OUT_OF_IMAGINATION, true);
				break;
			}

			++m_DrainedImagination;
			--newImagination;
			destComp->SetImagination(newImagination);
			EntityManager::Instance()->SerializeEntity(builder);


		}

		if (m_Timer >= m_CompleteTime && m_DrainedImagination >= m_TakeImagination) {
			CompleteRebuild(builder);
		}

		break;
	}
	case eRebuildState::INCOMPLETE: {
		m_TimerIncomplete += deltaTime;

		// For reset times < 0 this has to be handled manually
		if (m_TimeBeforeSmash > 0) {
			if (m_TimerIncomplete >= m_TimeBeforeSmash - 4.0f) {
				m_ShowResetEffect = true;

				EntityManager::Instance()->SerializeEntity(m_ParentEntity);
			}

			if (m_TimerIncomplete >= m_TimeBeforeSmash) {
				m_Builder = LWOOBJID_EMPTY;

				GameMessages::SendDieNoImplCode(m_ParentEntity, LWOOBJID_EMPTY, LWOOBJID_EMPTY, eKillType::VIOLENT, u"", 0.0f, 0.0f, 0.0f, false, true);

				ResetRebuild(false);
			}
		}
		break;
	}
	case eRebuildState::RESETTING: break;
	}
}

void QuickBuildComponent::OnUse(Entity* originator) {
	if (GetBuilder() != nullptr || m_State == eRebuildState::COMPLETED) {
		return;
	}

	if (m_Precondition != nullptr && !m_Precondition->Check(originator)) {
		return;
	}

	StartRebuild(originator);
}

void QuickBuildComponent::SpawnActivator() {
	if (!m_SelfActivator || m_ActivatorPosition != NiPoint3::ZERO) {
		if (!m_Activator) {
			EntityInfo info;

			info.lot = 6604;
			info.spawnerID = m_ParentEntity->GetObjectID();
			info.pos = m_ActivatorPosition == NiPoint3::ZERO ? m_ParentEntity->GetPosition() : m_ActivatorPosition;

			m_Activator = EntityManager::Instance()->CreateEntity(info, nullptr, m_ParentEntity);
			if (m_Activator) {
				m_ActivatorId = m_Activator->GetObjectID();
				EntityManager::Instance()->ConstructEntity(m_Activator);
			}

			EntityManager::Instance()->SerializeEntity(m_ParentEntity);
		}
	}
}

void QuickBuildComponent::DespawnActivator() {
	if (m_Activator) {
		EntityManager::Instance()->DestructEntity(m_Activator);

		m_Activator->ScheduleKillAfterUpdate();

		m_Activator = nullptr;

		m_ActivatorId = LWOOBJID_EMPTY;
	}
}

Entity* QuickBuildComponent::GetActivator() {
	return EntityManager::Instance()->GetEntity(m_ActivatorId);
}

NiPoint3 QuickBuildComponent::GetActivatorPosition() {
	return m_ActivatorPosition;
}

float QuickBuildComponent::GetResetTime() {
	return m_ResetTime;
}

float QuickBuildComponent::GetCompleteTime() {
	return m_CompleteTime;
}

int QuickBuildComponent::GetTakeImagination() {
	return m_TakeImagination;
}

bool QuickBuildComponent::GetInterruptible() {
	return m_Interruptible;
}

bool QuickBuildComponent::GetSelfActivator() {
	return m_SelfActivator;
}

std::vector<int> QuickBuildComponent::GetCustomModules() {
	return m_CustomModules;
}

int QuickBuildComponent::GetActivityId() {
	return m_ActivityId;
}

int QuickBuildComponent::GetPostImaginationCost() {
	return m_PostImaginationCost;
}

float QuickBuildComponent::GetTimeBeforeSmash() {
	return m_TimeBeforeSmash;
}

eRebuildState QuickBuildComponent::GetState() {
	return m_State;
}

Entity* QuickBuildComponent::GetBuilder() const {
	auto* builder = EntityManager::Instance()->GetEntity(m_Builder);

	return builder;
}

bool QuickBuildComponent::GetRepositionPlayer() const {
	return m_RepositionPlayer;
}

void QuickBuildComponent::SetActivatorPosition(NiPoint3 value) {
	m_ActivatorPosition = value;
}

void QuickBuildComponent::SetResetTime(float value) {
	m_ResetTime = value;
}

void QuickBuildComponent::SetCompleteTime(float value) {
	if (value < 0) {
		m_CompleteTime = 4.5f;
	} else {
		m_CompleteTime = value;
	}
}

void QuickBuildComponent::SetTakeImagination(int value) {
	m_TakeImagination = value;
}

void QuickBuildComponent::SetInterruptible(bool value) {
	m_Interruptible = value;
}

void QuickBuildComponent::SetSelfActivator(bool value) {
	m_SelfActivator = value;
}

void QuickBuildComponent::SetCustomModules(std::vector<int> value) {
	m_CustomModules = value;
}

void QuickBuildComponent::SetActivityId(int value) {
	m_ActivityId = value;
}

void QuickBuildComponent::SetPostImaginationCost(int value) {
	m_PostImaginationCost = value;
}

void QuickBuildComponent::SetTimeBeforeSmash(float value) {
	if (value < 0) {
		m_TimeBeforeSmash = 10.0f;
	} else {
		m_TimeBeforeSmash = value;
	}
}

void QuickBuildComponent::SetRepositionPlayer(bool value) {
	m_RepositionPlayer = value;
}

void QuickBuildComponent::StartRebuild(Entity* user) {
	if (m_State == eRebuildState::OPEN || m_State == eRebuildState::COMPLETED || m_State == eRebuildState::INCOMPLETE) {
		m_Builder = user->GetObjectID();

		auto* character = user->GetComponent<CharacterComponent>();
		character->SetCurrentActivity(eGameActivity::QUICKBUILDING);

		EntityManager::Instance()->SerializeEntity(user);

		GameMessages::SendRebuildNotifyState(m_ParentEntity, m_State, eRebuildState::BUILDING, user->GetObjectID());
		GameMessages::SendEnableRebuild(m_ParentEntity, true, false, false, eQuickBuildFailReason::NOT_GIVEN, 0.0f, user->GetObjectID());

		m_State = eRebuildState::BUILDING;
		m_StateDirty = true;
		EntityManager::Instance()->SerializeEntity(m_ParentEntity);

		auto* movingPlatform = m_ParentEntity->GetComponent<MovingPlatformComponent>();
		if (movingPlatform != nullptr) {
			movingPlatform->OnRebuildInitilized();
		}

		for (auto* script : CppScripts::GetEntityScripts(m_ParentEntity)) {
			script->OnRebuildStart(m_ParentEntity, user);
		}

		// Notify scripts and possible subscribers
		for (auto* script : CppScripts::GetEntityScripts(m_ParentEntity))
			script->OnRebuildNotifyState(m_ParentEntity, m_State);
		for (const auto& cb : m_RebuildStateCallbacks)
			cb(m_State);
	}
}

void QuickBuildComponent::CompleteRebuild(Entity* user) {
	if (user == nullptr) {
		return;
	}

	auto* characterComponent = user->GetComponent<CharacterComponent>();
	if (characterComponent != nullptr) {
		characterComponent->SetCurrentActivity(eGameActivity::NONE);
		characterComponent->TrackRebuildComplete();
	} else {
		Game::logger->Log("QuickBuildComponent", "Some user tried to finish the rebuild but they didn't have a character somehow.");
		return;
	}

	EntityManager::Instance()->SerializeEntity(user);

	GameMessages::SendRebuildNotifyState(m_ParentEntity, m_State, eRebuildState::COMPLETED, user->GetObjectID());
	GameMessages::SendPlayFXEffect(m_ParentEntity, 507, u"create", "BrickFadeUpVisCompleteEffect", LWOOBJID_EMPTY, 0.4f, 1.0f, true);
	GameMessages::SendEnableRebuild(m_ParentEntity, false, false, true, eQuickBuildFailReason::NOT_GIVEN, m_ResetTime, user->GetObjectID());
	GameMessages::SendTerminateInteraction(user->GetObjectID(), eTerminateType::FROM_INTERACTION, m_ParentEntity->GetObjectID());


	m_State = eRebuildState::COMPLETED;
	m_StateDirty = true;
	m_Timer = 0.0f;
	m_DrainedImagination = 0;

	EntityManager::Instance()->SerializeEntity(m_ParentEntity);

	// Removes extra item requirements, isn't live accurate.
	// In live, all items were removed at the start of the quickbuild, then returned if it was cancelled.
	// TODO: fix?
	if (m_Precondition != nullptr) {
		m_Precondition->Check(user, true);
	}

	DespawnActivator();

	// Set owner override so that entities smashed by this quickbuild will result in the builder getting rewards.
	m_ParentEntity->SetOwnerOverride(user->GetObjectID());

	auto* builder = GetBuilder();

	if (builder) {
		auto* team = TeamManager::Instance()->GetTeam(builder->GetObjectID());
		if (team) {
			for (const auto memberId : team->members) { // progress missions for all team members
				auto* member = EntityManager::Instance()->GetEntity(memberId);
				if (member) {
					auto* missionComponent = member->GetComponent<MissionComponent>();
					if (missionComponent) missionComponent->Progress(eMissionTaskType::ACTIVITY, m_ActivityId);
				}
			}
		} else {
			auto* missionComponent = builder->GetComponent<MissionComponent>();
			if (missionComponent) missionComponent->Progress(eMissionTaskType::ACTIVITY, m_ActivityId);
		}
		LootGenerator::Instance().DropActivityLoot(builder, m_ParentEntity, m_ActivityId, 1);
	}

	// Notify scripts
	for (auto* script : CppScripts::GetEntityScripts(m_ParentEntity)) {
		script->OnRebuildComplete(m_ParentEntity, user);
		script->OnRebuildNotifyState(m_ParentEntity, m_State);
	}

	// Notify subscribers
	for (const auto& callback : m_RebuildStateCallbacks)
		callback(m_State);
	for (const auto& callback : m_RebuildCompleteCallbacks)
		callback(user);

	m_ParentEntity->TriggerEvent(eTriggerEventType::REBUILD_COMPLETE, user);

	auto* movingPlatform = m_ParentEntity->GetComponent<MovingPlatformComponent>();
	if (movingPlatform != nullptr) {
		movingPlatform->OnCompleteRebuild();
	}

	// Set flag
	auto* character = user->GetCharacter();

	if (character != nullptr) {
		const auto flagNumber = m_ParentEntity->GetVar<int32_t>(u"quickbuild_single_build_player_flag");

		if (flagNumber != 0) {
			character->SetPlayerFlag(flagNumber, true);
		}
	}
	RenderComponent::PlayAnimation(user, u"rebuild-celebrate", 1.09f);
}

void QuickBuildComponent::ResetRebuild(bool failed) {
	Entity* builder = GetBuilder();

	if (m_State == eRebuildState::BUILDING && builder) {
		GameMessages::SendEnableRebuild(m_ParentEntity, false, false, failed, eQuickBuildFailReason::NOT_GIVEN, m_ResetTime, builder->GetObjectID());

		if (failed) {
			RenderComponent::PlayAnimation(builder, u"rebuild-fail");
		}
	}

	GameMessages::SendRebuildNotifyState(m_ParentEntity, m_State, eRebuildState::RESETTING, LWOOBJID_EMPTY);

	m_State = eRebuildState::RESETTING;
	m_StateDirty = true;
	m_Timer = 0.0f;
	m_TimerIncomplete = 0.0f;
	m_ShowResetEffect = false;
	m_DrainedImagination = 0;

	EntityManager::Instance()->SerializeEntity(m_ParentEntity);

	// Notify scripts and possible subscribers
	for (auto* script : CppScripts::GetEntityScripts(m_ParentEntity))
		script->OnRebuildNotifyState(m_ParentEntity, m_State);
	for (const auto& cb : m_RebuildStateCallbacks)
		cb(m_State);

	m_ParentEntity->ScheduleKillAfterUpdate();

	if (m_Activator) {
		m_Activator->ScheduleKillAfterUpdate();
	}
}

void QuickBuildComponent::CancelRebuild(Entity* entity, eQuickBuildFailReason failReason, bool skipChecks) {
	if (m_State != eRebuildState::COMPLETED || skipChecks) {

		m_Builder = LWOOBJID_EMPTY;

		const auto entityID = entity != nullptr ? entity->GetObjectID() : LWOOBJID_EMPTY;

		// Notify the client that a state has changed
		GameMessages::SendRebuildNotifyState(m_ParentEntity, m_State, eRebuildState::INCOMPLETE, entityID);
		GameMessages::SendEnableRebuild(m_ParentEntity, false, true, false, failReason, m_Timer, entityID);

		// Now terminate any interaction with the rebuild
		GameMessages::SendTerminateInteraction(entityID, eTerminateType::FROM_INTERACTION, m_ParentEntity->GetObjectID());
		GameMessages::SendTerminateInteraction(m_ParentEntity->GetObjectID(), eTerminateType::FROM_INTERACTION, m_ParentEntity->GetObjectID());

		// Now update the component itself
		m_State = eRebuildState::INCOMPLETE;
		m_StateDirty = true;

		// Notify scripts and possible subscribers
		for (auto* script : CppScripts::GetEntityScripts(m_ParentEntity))
			script->OnRebuildNotifyState(m_ParentEntity, m_State);
		for (const auto& cb : m_RebuildStateCallbacks)
			cb(m_State);

		EntityManager::Instance()->SerializeEntity(m_ParentEntity);
	}

	if (entity == nullptr) {
		return;
	}

	auto* characterComponent = entity->GetComponent<CharacterComponent>();
	if (characterComponent) {
		characterComponent->SetCurrentActivity(eGameActivity::NONE);
		EntityManager::Instance()->SerializeEntity(entity);
	}
}

void QuickBuildComponent::AddRebuildCompleteCallback(const std::function<void(Entity* user)>& callback) {
	m_RebuildCompleteCallbacks.push_back(callback);
}

void QuickBuildComponent::AddRebuildStateCallback(const std::function<void(eRebuildState state)>& callback) {
	m_RebuildStateCallbacks.push_back(callback);
}
