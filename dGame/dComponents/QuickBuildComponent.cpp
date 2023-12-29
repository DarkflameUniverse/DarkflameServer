#include "QuickBuildComponent.h"
#include "Entity.h"
#include "DestroyableComponent.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "Game.h"
#include "Logger.h"
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

QuickBuildComponent::QuickBuildComponent(Entity* parent, uint32_t id) : ActivityComponent(parent) {
	m_Parent = parent;

	CDRebuildComponentTable* rebCompTable = CDClientManager::Instance().GetTable<CDRebuildComponentTable>();
	std::vector<CDRebuildComponent> rebCompData = rebCompTable->Query([=](CDRebuildComponent entry) { return (entry.id == id); });

	if (rebCompData.size() > 0) {
		SetResetTime(rebCompData[0].reset_time);
		SetCompleteTime(rebCompData[0].complete_time);
		SetTakeImagination(rebCompData[0].take_imagination);
		SetActivityID(rebCompData[0].activityID);
		SetInterruptible(rebCompData[0].interruptible);
		SetSelfActivator(rebCompData[0].self_activator);
		SetPostImaginationCost(rebCompData[0].post_imagination_cost);
		SetTimeBeforeSmash(rebCompData[0].time_before_smash);
		const auto compTime = m_Parent->GetVar<float>(u"compTime");
		if (compTime > 0) SetCompleteTime(compTime);
	}
	std::u16string checkPreconditions = m_Parent->GetVar<std::u16string>(u"CheckPrecondition");

	if (!checkPreconditions.empty()) {
		m_Precondition = new PreconditionExpression(GeneralUtils::UTF16ToWTF8(checkPreconditions));
	}

	const auto activityID = m_Parent->GetVar<int32_t>(u"activityID");
	if (activityID > 0) SetActivityID(activityID);
	SetupActivity(GetActivityID());

	const auto rebuildResetTime = m_Parent->GetVar<float>(u"rebuild_reset_time");
	if (rebuildResetTime != 0.0f) {
		SetResetTime(rebuildResetTime);
		// Known bug with moving platform in FV that casues it to build at the end instead of the start.
		// This extends the smash time so players can ride up the lift.
		if (m_Parent->GetLOT() == 9483) SetResetTime(GetResetTime() + 25);
	}

	// Should a setting that has the build activator position exist, fetch that setting here and parse it for position.
	// It is assumed that the user who sets this setting uses the correct character delimiter (character 31 or in hex 0x1F)
	auto positionAsVector = GeneralUtils::SplitString(m_Parent->GetVarAsString(u"rebuild_activators"), 0x1F);
	if (positionAsVector.size() == 3 &&
		GeneralUtils::TryParse(positionAsVector[0], m_ActivatorPosition.x) &&
		GeneralUtils::TryParse(positionAsVector[1], m_ActivatorPosition.y) &&
		GeneralUtils::TryParse(positionAsVector[2], m_ActivatorPosition.z)) {
	} else if (
			m_Parent->CheckIfVarExists(u"position.x") &&
			m_Parent->CheckIfVarExists(u"position.y") &&
			m_Parent->CheckIfVarExists(u"position.z")) {
		m_ActivatorPosition.x = m_Parent->GetVar<float>(u"position.x");
		m_ActivatorPosition.y = m_Parent->GetVar<float>(u"position.y");
		m_ActivatorPosition.x = m_Parent->GetVar<float>(u"position.z");
	} else {
		m_ActivatorPosition = m_Parent->GetPosition();
	}


	const auto timeBeforeSmash = m_Parent->GetVar<float>(u"tmeSmsh");
	if (timeBeforeSmash > 0) SetTimeBeforeSmash(timeBeforeSmash);
	m_IsChoiceBuild = m_Parent->GetBoolean(u"is_ChoiceBuild");
	
	SpawnActivator();
}

QuickBuildComponent::~QuickBuildComponent() {
	delete m_Precondition;

	Entity* builder = GetBuilder();
	if (builder) {
		CancelQuickBuild(builder, eQuickBuildFailReason::BUILD_ENDED, true);
	}

	DespawnActivator();
}

void QuickBuildComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) {
	ActivityComponent::Serialize(outBitStream, bIsInitialUpdate);
	outBitStream->Write(m_StateDirty);
	if (m_StateDirty) {
		outBitStream->Write(m_State);
		outBitStream->Write(m_ShowResetEffect);
		outBitStream->Write(m_Activator != nullptr);
		outBitStream->Write(m_Timer);
		outBitStream->Write(m_TimerIncomplete);
		if (bIsInitialUpdate) {
			outBitStream->Write(m_IsChoiceBuild);
			if (m_IsChoiceBuild) outBitStream->Write(m_ResetTime);
			outBitStream->Write(m_ActivatorPosition);
			outBitStream->Write(m_RepositionPlayer);
		}
		if (!bIsInitialUpdate) m_StateDirty = false;
	}
}

void QuickBuildComponent::Update(float deltaTime) {
	m_Activator = GetActivator();

	switch (m_State) {
	case eQuickBuildState::OPEN: {
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
					m_StateDirty = true;
					Game::entityManager->SerializeEntity(m_Parent);
				}

				if (m_TimerIncomplete >= m_TimeBeforeSmash) {
					ClearActivityPlayerData();

					GameMessages::SendDie(m_Parent, LWOOBJID_EMPTY, LWOOBJID_EMPTY, true, eKillType::VIOLENT, u"", 0.0f, 0.0f, 0.0f, false, true);

					ResetQuickBuild(false);
				}
			}
		}

		break;
	}
	case eQuickBuildState::COMPLETED: {
		m_Timer += deltaTime;

		// For reset times < 0 this has to be handled manually
		if (m_ResetTime > 0) {
			if (m_Timer >= m_ResetTime - 4.0f) {
				if (!m_ShowResetEffect) {
					m_ShowResetEffect = true;
					m_StateDirty = true;
					Game::entityManager->SerializeEntity(m_Parent);
				}
			}

			if (m_Timer >= m_ResetTime) {

				GameMessages::SendDie(m_Parent, LWOOBJID_EMPTY, LWOOBJID_EMPTY, true, eKillType::VIOLENT, u"", 0.0f, 0.0f, 0.0f, false, true);

				ResetQuickBuild(false);
			}
		}
		break;
	}
	case eQuickBuildState::BUILDING:
	{
		Entity* builder = GetBuilder();

		if (!builder) {
			ResetQuickBuild(false);

			return;
		}

		m_TimeBeforeDrain -= deltaTime;
		m_Timer += deltaTime;
		m_TimerIncomplete = 0;
		m_ShowResetEffect = false;
		m_StateDirty = true;
		if (m_TimeBeforeDrain <= 0.0f) {
			m_TimeBeforeDrain = m_CompleteTime / static_cast<float>(m_TakeImagination);

			DestroyableComponent* destComp = builder->GetComponent<DestroyableComponent>();
			if (!destComp) break;

			++m_DrainedImagination;
			const int32_t imaginationCostRemaining = m_TakeImagination - m_DrainedImagination;

			const int32_t newImagination = destComp->GetImagination() - 1;
			destComp->SetImagination(newImagination);
			Game::entityManager->SerializeEntity(builder);

			if (newImagination <= 0 && imaginationCostRemaining > 0) {
				CancelQuickBuild(builder, eQuickBuildFailReason::OUT_OF_IMAGINATION, true);
				break;
			}
		}

		if (m_Timer >= m_CompleteTime && m_DrainedImagination >= m_TakeImagination) {
			CompleteQuickBuild(builder);
		}

		break;
	}
	case eQuickBuildState::INCOMPLETE: {
		m_TimerIncomplete += deltaTime;

		// For reset times < 0 this has to be handled manually
		if (m_TimeBeforeSmash > 0) {
			if (m_TimerIncomplete >= m_TimeBeforeSmash - 4.0f) {
				m_ShowResetEffect = true;
				m_StateDirty = true;
				Game::entityManager->SerializeEntity(m_Parent);
			}

			if (m_TimerIncomplete >= m_TimeBeforeSmash) {
				ClearActivityPlayerData();

				GameMessages::SendDie(m_Parent, LWOOBJID_EMPTY, LWOOBJID_EMPTY, true, eKillType::VIOLENT, u"", 0.0f, 0.0f, 0.0f, false, true);

				ResetQuickBuild(false);
			}
		}
		break;
	}
	case eQuickBuildState::RESETTING: break;
	}
}

void QuickBuildComponent::OnUse(Entity* originator) {
	if (GetBuilder() != nullptr || m_State == eQuickBuildState::COMPLETED) {
		return;
	}

	if (m_Precondition != nullptr && !m_Precondition->Check(originator)) {
		return;
	}

	StartQuickBuild(originator);
}

void QuickBuildComponent::SpawnActivator() {
	if (!m_SelfActivator || m_ActivatorPosition != NiPoint3::ZERO) {
		if (!m_Activator) {
			EntityInfo info;

			info.lot = 6604;
			info.spawnerID = m_Parent->GetObjectID();
			info.pos = m_ActivatorPosition == NiPoint3::ZERO ? m_Parent->GetPosition() : m_ActivatorPosition;

			m_Activator = Game::entityManager->CreateEntity(info, nullptr, m_Parent);
			if (m_Activator) {
				m_ActivatorId = m_Activator->GetObjectID();
				Game::entityManager->ConstructEntity(m_Activator);
			}

			Game::entityManager->SerializeEntity(m_Parent);
		}
	}
}

void QuickBuildComponent::DespawnActivator() {
	if (m_Activator) {
		Game::entityManager->DestructEntity(m_Activator);

		m_Activator->ScheduleKillAfterUpdate();

		m_Activator = nullptr;

		m_ActivatorId = LWOOBJID_EMPTY;
	}
}

Entity* QuickBuildComponent::GetActivator() {
	return Game::entityManager->GetEntity(m_ActivatorId);
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

int QuickBuildComponent::GetPostImaginationCost() {
	return m_PostImaginationCost;
}

float QuickBuildComponent::GetTimeBeforeSmash() {
	return m_TimeBeforeSmash;
}

eQuickBuildState QuickBuildComponent::GetState() {
	return m_State;
}

Entity* QuickBuildComponent::GetBuilder() const {
	const auto players = GetActivityPlayers();
	if (players.empty()) return nullptr;
	auto* builder = Game::entityManager->GetEntity(players[0]->playerID);

	return builder;
}

bool QuickBuildComponent::GetRepositionPlayer() const {
	return m_RepositionPlayer;
}

void QuickBuildComponent::SetActivatorPosition(NiPoint3 value) {
	m_ActivatorPosition = value;
}

void QuickBuildComponent::SetResetTime(float value) {
	if (value < 0) m_ResetTime = 20.0f;
	else m_ResetTime = value;
}

void QuickBuildComponent::SetCompleteTime(float value) {
	if (value < 0) m_CompleteTime = 4.5f;
	else m_CompleteTime = value;
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

void QuickBuildComponent::SetPostImaginationCost(int value) {
	m_PostImaginationCost = value;
}

void QuickBuildComponent::SetTimeBeforeSmash(float value) {
	if (value < 0) m_TimeBeforeSmash = 10.0f;
	else m_TimeBeforeSmash = value;
}

void QuickBuildComponent::SetRepositionPlayer(bool value) {
	m_RepositionPlayer = value;
}

void QuickBuildComponent::StartQuickBuild(Entity* user) {
	if (m_State == eQuickBuildState::OPEN || m_State == eQuickBuildState::COMPLETED || m_State == eQuickBuildState::INCOMPLETE) {
		// user->GetObjectID();
		AddActivityPlayerData(user->GetObjectID());

		auto* character = user->GetComponent<CharacterComponent>();
		character->SetCurrentActivity(eGameActivity::QUICKBUILDING);

		Game::entityManager->SerializeEntity(user);

		GameMessages::SendQuickBuildNotifyState(m_Parent, m_State, eQuickBuildState::BUILDING, user->GetObjectID());
		GameMessages::SendEnableQuickBuild(m_Parent, true, false, false, eQuickBuildFailReason::NOT_GIVEN, 0.0f, user->GetObjectID());

		m_State = eQuickBuildState::BUILDING;
		m_StateDirty = true;
		Game::entityManager->SerializeEntity(m_Parent);

		auto* movingPlatform = m_Parent->GetComponent<MovingPlatformComponent>();
		if (movingPlatform != nullptr) {
			movingPlatform->OnQuickBuildInitilized();
		}

		for (auto* script : CppScripts::GetEntityScripts(m_Parent)) {
			script->OnQuickBuildStart(m_Parent, user);
		}

		// Notify scripts and possible subscribers
		for (auto* script : CppScripts::GetEntityScripts(m_Parent))
			script->OnQuickBuildNotifyState(m_Parent, m_State);
		for (const auto& cb : m_QuickBuildStateCallbacks)
			cb(m_State);
	}
}

void QuickBuildComponent::CompleteQuickBuild(Entity* user) {
	if (user == nullptr) {
		return;
	}

	auto* characterComponent = user->GetComponent<CharacterComponent>();
	if (characterComponent != nullptr) {
		characterComponent->SetCurrentActivity(eGameActivity::NONE);
		characterComponent->TrackQuickBuildComplete();
	} else {
		LOG("Some user tried to finish the rebuild but they didn't have a character somehow.");
		return;
	}

	Game::entityManager->SerializeEntity(user);

	GameMessages::SendQuickBuildNotifyState(m_Parent, m_State, eQuickBuildState::COMPLETED, user->GetObjectID());
	GameMessages::SendPlayFXEffect(m_Parent, 507, u"create", "BrickFadeUpVisCompleteEffect", LWOOBJID_EMPTY, 0.4f, 1.0f, true);
	GameMessages::SendEnableQuickBuild(m_Parent, false, false, true, eQuickBuildFailReason::NOT_GIVEN, m_ResetTime, user->GetObjectID());
	GameMessages::SendTerminateInteraction(user->GetObjectID(), eTerminateType::FROM_INTERACTION, m_Parent->GetObjectID());


	m_State = eQuickBuildState::COMPLETED;
	m_StateDirty = true;
	m_Timer = 0.0f;
	m_DrainedImagination = 0;

	Game::entityManager->SerializeEntity(m_Parent);

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

	if (builder) {
		auto* team = TeamManager::Instance()->GetTeam(builder->GetObjectID());
		if (team) {
			for (const auto memberId : team->members) { // progress missions for all team members
				auto* member = Game::entityManager->GetEntity(memberId);
				if (member) {
					auto* missionComponent = member->GetComponent<MissionComponent>();
					if (missionComponent) missionComponent->Progress(eMissionTaskType::ACTIVITY, GetActivityID());
				}
			}
		} else {
			auto* missionComponent = builder->GetComponent<MissionComponent>();
			if (missionComponent) missionComponent->Progress(eMissionTaskType::ACTIVITY, GetActivityID());
		}
		Loot::DropActivityLoot(builder, m_Parent, GetActivityID(), 1);
	}

	// Notify scripts
	for (auto* script : CppScripts::GetEntityScripts(m_Parent)) {
		script->OnQuickBuildComplete(m_Parent, user);
		script->OnQuickBuildNotifyState(m_Parent, m_State);
	}

	// Notify subscribers
	for (const auto& callback : m_QuickBuildStateCallbacks)
		callback(m_State);
	for (const auto& callback : m_QuickBuildCompleteCallbacks)
		callback(user);

	m_Parent->TriggerEvent(eTriggerEventType::REBUILD_COMPLETE, user);

	auto* movingPlatform = m_Parent->GetComponent<MovingPlatformComponent>();
	if (movingPlatform != nullptr) {
		movingPlatform->OnCompleteQuickBuild();
	}

	// Set flag
	auto* character = user->GetCharacter();

	if (character != nullptr) {
		const auto flagNumber = m_Parent->GetVar<int32_t>(u"quickbuild_single_build_player_flag");

		if (flagNumber != 0) {
			character->SetPlayerFlag(flagNumber, true);
		}
	}
	RenderComponent::PlayAnimation(user, u"rebuild-celebrate", 1.09f);
}

void QuickBuildComponent::ResetQuickBuild(bool failed) {
	Entity* builder = GetBuilder();

	if (m_State == eQuickBuildState::BUILDING && builder) {
		GameMessages::SendEnableQuickBuild(m_Parent, false, false, failed, eQuickBuildFailReason::NOT_GIVEN, m_ResetTime, builder->GetObjectID());

		if (failed) {
			RenderComponent::PlayAnimation(builder, u"rebuild-fail");
		}
	}

	GameMessages::SendQuickBuildNotifyState(m_Parent, m_State, eQuickBuildState::RESETTING, LWOOBJID_EMPTY);

	m_State = eQuickBuildState::RESETTING;
	m_StateDirty = true;
	m_Timer = 0.0f;
	m_TimerIncomplete = 0.0f;
	m_ShowResetEffect = false;
	m_DrainedImagination = 0;

	Game::entityManager->SerializeEntity(m_Parent);

	// Notify scripts and possible subscribers
	for (auto* script : CppScripts::GetEntityScripts(m_Parent))
		script->OnQuickBuildNotifyState(m_Parent, m_State);
	for (const auto& cb : m_QuickBuildStateCallbacks)
		cb(m_State);

	m_Parent->ScheduleKillAfterUpdate();

	if (m_Activator) {
		m_Activator->ScheduleKillAfterUpdate();
	}
}

void QuickBuildComponent::CancelQuickBuild(Entity* entity, eQuickBuildFailReason failReason, bool skipChecks) {
	if (m_State != eQuickBuildState::COMPLETED || skipChecks) {
		RemoveActivityPlayerData(entity->GetObjectID());

		const auto entityID = entity != nullptr ? entity->GetObjectID() : LWOOBJID_EMPTY;

		// Notify the client that a state has changed
		GameMessages::SendQuickBuildNotifyState(m_Parent, m_State, eQuickBuildState::INCOMPLETE, entityID);
		GameMessages::SendEnableQuickBuild(m_Parent, false, true, false, failReason, m_Timer, entityID);

		// Now terminate any interaction with the rebuild
		GameMessages::SendTerminateInteraction(entityID, eTerminateType::FROM_INTERACTION, m_Parent->GetObjectID());
		GameMessages::SendTerminateInteraction(m_Parent->GetObjectID(), eTerminateType::FROM_INTERACTION, m_Parent->GetObjectID());

		// Now update the component itself
		m_State = eQuickBuildState::INCOMPLETE;
		m_StateDirty = true;

		// Notify scripts and possible subscribers
		for (auto* script : CppScripts::GetEntityScripts(m_Parent))
			script->OnQuickBuildNotifyState(m_Parent, m_State);
		for (const auto& cb : m_QuickBuildStateCallbacks)
			cb(m_State);

		Game::entityManager->SerializeEntity(m_Parent);
	}

	if (entity == nullptr) {
		return;
	}

	CharacterComponent* characterComponent = entity->GetComponent<CharacterComponent>();
	if (characterComponent) {
		characterComponent->SetCurrentActivity(eGameActivity::NONE);
		Game::entityManager->SerializeEntity(entity);
	}
}

void QuickBuildComponent::AddQuickBuildCompleteCallback(const std::function<void(Entity* user)>& callback) {
	m_QuickBuildCompleteCallbacks.push_back(callback);
}

void QuickBuildComponent::AddQuickBuildStateCallback(const std::function<void(eQuickBuildState state)>& callback) {
	m_QuickBuildStateCallbacks.push_back(callback);
}
