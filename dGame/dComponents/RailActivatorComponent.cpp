#include <algorithm>
#include "RailActivatorComponent.h"
#include "CDClientManager.h"
#include "CDRailActivatorComponent.h"
#include "Entity.h"
#include "GameMessages.h"
#include "EffectsMessages.h"
#include "CombatMessages.h"
#include "MovementMessages.h"
#include "QuickBuildComponent.h"
#include "Game.h"
#include "Logger.h"
#include "RenderComponent.h"
#include "EntityManager.h"
#include "eStateChangeType.h"

RailActivatorComponent::RailActivatorComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {
	const auto tableData = CDClientManager::GetTable<CDRailActivatorComponentTable>()->GetEntryByID(componentID);

	m_Path = parent->GetVar<std::u16string>(u"rail_path");
	m_PathDirection = parent->GetVar<bool>(u"rail_path_direction");
	m_PathStart = parent->GetVar<uint32_t>(u"rail_path_start");

	m_StartSound = tableData.startSound;
	m_loopSound = tableData.loopSound;
	m_StopSound = tableData.stopSound;

	m_StartAnimation = tableData.startAnimation;
	m_LoopAnimation = tableData.loopAnimation;
	m_StopAnimation = tableData.stopAnimation;

	m_StartEffect = tableData.startEffectID;
	m_LoopEffect = tableData.loopEffectID;
	m_StopEffect = tableData.stopEffectID;

	// The RailActivatorComponent row gives these (the client reads DamageImmune, NoAggro and ShowNameBillboard in
	// LWOPlayerForcedMovementComponent::LoadRailData 0x00c85dc0); a level key, when there, replaces the row's value
	// as StartRailMovement's flags replace the row's in msgStartRailMovement (0x00ccd9c0) unless bUseDB is set.
	const auto levelOr = [parent](const std::u16string& key, const bool tableValue) {
		return parent->HasVar(key) ? parent->GetVar<bool>(key) : tableValue;
	};
	m_DamageImmune = levelOr(u"rail_activator_damage_immune", tableData.damageImmune);
	m_NoAggro = levelOr(u"rail_no_aggro", tableData.noAggro);
	m_NotifyArrived = parent->GetVar<bool>(u"rail_notify_activator_arrived");
	m_ShowNameBillboard = levelOr(u"rail_show_name_billboard", tableData.showNameBillboard);
	m_UseDB = parent->GetVar<bool>(u"rail_use_db");
	m_Active = levelOr(u"rail_activator_active", true);
	m_CameraLocked = tableData.cameraLocked;
	m_CollisionEnabled = tableData.playerCollision;
}

RailActivatorComponent::~RailActivatorComponent() = default;

void RailActivatorComponent::OnUse(Entity* originator) {
	auto* quickBuildComponent = m_Parent->GetComponent<QuickBuildComponent>();
	if (quickBuildComponent != nullptr && quickBuildComponent->GetState() != eQuickBuildState::COMPLETED)
		return;

	if (quickBuildComponent != nullptr) {
		// Don't want it to be destroyed while a player is using it
		quickBuildComponent->SetResetTime(quickBuildComponent->GetResetTime() + 10.0f);
	}

	m_EntitiesOnRail.push_back(originator->GetObjectID());

	// Start the initial effects
	if (!m_StartEffect.second.empty()) {
		GameMessages::PlayFXEffect(originator->GetObjectID(), m_StartEffect.first, m_StartEffect.second, std::to_string(m_StartEffect.first)).Send(UNASSIGNED_SYSTEM_ADDRESS);
	}
	
	float animationLength = 0.5f;
	if (!m_StartAnimation.empty()) {
		animationLength = RenderComponent::PlayAnimation(originator, m_StartAnimation);
	}

	const auto originatorID = originator->GetObjectID();

	m_Parent->AddCallbackTimer(animationLength, [originatorID, this]() {
		auto* originator = Game::entityManager->GetEntity(originatorID);

		if (originator == nullptr) {
			return;
		}

		GameMessages::StartRailMovement startRail;
		startRail.target = originator->GetObjectID();
		startRail.pathName = m_Path;
		startRail.startSound = m_StartSound;
		startRail.loopSound = m_loopSound;
		startRail.stopSound = m_StopSound;
		startRail.pathStart = m_PathStart;
		startRail.goForward = m_PathDirection;
		startRail.bDamageImmune = m_DamageImmune;
		startRail.bNoAggro = m_NoAggro;
		startRail.bNotifyActor = m_NotifyArrived;
		startRail.bShowNameBillboard = m_ShowNameBillboard;
		startRail.bCameraLocked = m_CameraLocked;
		startRail.bCollisionEnabled = m_CollisionEnabled;
		startRail.bUseDB = m_UseDB;
		startRail.railActivatorComponentID = m_ComponentID;
		startRail.railActivatorObjectID = m_Parent->GetObjectID();
		startRail.Send(originator->GetSystemAddress());
		});
}

void RailActivatorComponent::OnRailMovementReady(Entity* originator) const {
	// Stun the originator
	GameMessages::SetStunned stun;
	stun.target = originator->GetObjectID();
	stun.StateChangeType = eStateChangeType::PUSH;
	stun.bCantAttack = true;
	stun.bCantEquip = true;
	stun.bCantInteract = true;
	stun.bCantJump = true;
	stun.bCantMove = true;
	stun.bCantTurn = true;
	stun.bCantUseItem = true;
	stun.Send(originator->GetSystemAddress());

	if (std::find(m_EntitiesOnRail.begin(), m_EntitiesOnRail.end(), originator->GetObjectID()) != m_EntitiesOnRail.end()) {
		// Stop the initial effects
		if (!m_StartEffect.second.empty()) {
			GameMessages::StopFXEffect(originator->GetObjectID(), false, std::to_string(m_StartEffect.first)).Send(UNASSIGNED_SYSTEM_ADDRESS);
		}

		// Start the looping effects
		if (!m_LoopEffect.second.empty()) {
			GameMessages::PlayFXEffect(originator->GetObjectID(), m_LoopEffect.first, m_LoopEffect.second, std::to_string(m_LoopEffect.first)).Send(UNASSIGNED_SYSTEM_ADDRESS);
		}

		if (!m_LoopAnimation.empty()) {
			RenderComponent::PlayAnimation(originator, m_LoopAnimation);
		}

		GameMessages::SetRailMovement setRail;
		setRail.target = originator->GetObjectID();
		setRail.pathGoForward = m_PathDirection;
		setRail.pathName = m_Path;
		setRail.pathStart = m_PathStart;
		setRail.railActivatorComponentID = m_ComponentID;
		setRail.railActivatorObjectID = m_Parent->GetObjectID();
		setRail.Send(originator->GetSystemAddress());
	}
}

void RailActivatorComponent::OnCancelRailMovement(Entity* originator) {
	// Remove the stun from the originator
	GameMessages::SetStunned stun;
	stun.target = originator->GetObjectID();
	stun.StateChangeType = eStateChangeType::POP;
	stun.bCantAttack = true;
	stun.bCantEquip = true;
	stun.bCantInteract = true;
	stun.bCantJump = true;
	stun.bCantMove = true;
	stun.bCantTurn = true;
	stun.bCantUseItem = true;
	stun.Send(originator->GetSystemAddress());

	auto* quickBuildComponent = m_Parent->GetComponent<QuickBuildComponent>();

	if (quickBuildComponent != nullptr) {
		// Set back reset time
		quickBuildComponent->SetResetTime(quickBuildComponent->GetResetTime() - 10.0f);
	}

	if (std::find(m_EntitiesOnRail.begin(), m_EntitiesOnRail.end(), originator->GetObjectID()) != m_EntitiesOnRail.end()) {
		// Stop the looping effects
		if (!m_LoopEffect.second.empty()) {
			GameMessages::StopFXEffect(originator->GetObjectID(), false, std::to_string(m_LoopEffect.first)).Send(UNASSIGNED_SYSTEM_ADDRESS);
		}

		// Start the end effects
		if (!m_StopEffect.second.empty()) {
			GameMessages::PlayFXEffect(originator->GetObjectID(), m_StopEffect.first, m_StopEffect.second, std::to_string(m_StopEffect.first)).Send(UNASSIGNED_SYSTEM_ADDRESS);
		}

		if (!m_StopAnimation.empty()) {
			RenderComponent::PlayAnimation(originator, m_StopAnimation);
		}

		// Remove the player after they've signalled they're done railing
		m_EntitiesOnRail.erase(std::remove(m_EntitiesOnRail.begin(), m_EntitiesOnRail.end(),
			originator->GetObjectID()), m_EntitiesOnRail.end());
	}
}
