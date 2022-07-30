#include <algorithm>
#include "RailActivatorComponent.h"
#include "CDClientManager.h"
#include "CDRailActivatorComponent.h"
#include "Entity.h"
#include "GameMessages.h"
#include "RebuildComponent.h"
#include "Game.h"
#include "dLogger.h"

RailActivatorComponent::RailActivatorComponent(Entity* parent, int32_t componentID) : Component(parent) {
	m_ComponentID = componentID;
	const auto tableData = CDClientManager::Instance()
		->GetTable<CDRailActivatorComponentTable>("RailActivatorComponent")->GetEntryByID(componentID);

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

	m_DamageImmune = parent->GetVar<bool>(u"rail_activator_damage_immune");
	m_NoAggro = parent->GetVar<bool>(u"rail_no_aggro");
	m_NotifyArrived = parent->GetVar<bool>(u"rail_notify_activator_arrived");
	m_ShowNameBillboard = parent->GetVar<bool>(u"rail_show_name_billboard");
	m_UseDB = parent->GetVar<bool>(u"rail_use_db");
	m_CameraLocked = tableData.cameraLocked;
	m_CollisionEnabled = tableData.playerCollision;
}

RailActivatorComponent::~RailActivatorComponent() = default;

void RailActivatorComponent::OnUse(Entity* originator) {
	auto* rebuildComponent = m_Parent->GetComponent<RebuildComponent>();
	if (rebuildComponent != nullptr && rebuildComponent->GetState() != REBUILD_COMPLETED)
		return;

	if (rebuildComponent != nullptr) {
		// Don't want it to be destroyed while a player is using it
		rebuildComponent->SetResetTime(rebuildComponent->GetResetTime() + 10.0f);
	}

	m_EntitiesOnRail.push_back(originator->GetObjectID());

	// Start the initial effects
	if (!m_StartEffect.second.empty()) {
		GameMessages::SendPlayFXEffect(originator->GetObjectID(), m_StartEffect.first, m_StartEffect.second,
			std::to_string(m_StartEffect.first));
	}

	if (!m_StartAnimation.empty()) {
		GameMessages::SendPlayAnimation(originator, m_StartAnimation);
	}

	float animationLength;

	if (m_StartAnimation == u"whirlwind-rail-up-earth") {
		animationLength = 1.5f;
	} else if (m_StartAnimation == u"whirlwind-rail-up-lightning") {
		animationLength = 0.5f;
	} else if (m_StartAnimation == u"whirlwind-rail-up-ice") {
		animationLength = 0.5f;
	} else if (m_StartAnimation == u"whirlwind-rail-up-fire") {
		animationLength = 0.5f;
	} else {
		animationLength = 0.5f;
	}

	const auto originatorID = originator->GetObjectID();

	m_Parent->AddCallbackTimer(animationLength, [originatorID, this]() {
		auto* originator = EntityManager::Instance()->GetEntity(originatorID);

		if (originator == nullptr) {
			return;
		}

		GameMessages::SendStartRailMovement(originator->GetObjectID(), m_Path, m_StartSound,
			m_loopSound, m_StopSound, originator->GetSystemAddress(),
			m_PathStart, m_PathDirection, m_DamageImmune, m_NoAggro, m_NotifyArrived,
			m_ShowNameBillboard, m_CameraLocked, m_CollisionEnabled, m_UseDB, m_ComponentID,
			m_Parent->GetObjectID());
		});
}

void RailActivatorComponent::OnRailMovementReady(Entity* originator) const {
	// Stun the originator
	GameMessages::SendSetStunned(originator->GetObjectID(), PUSH, originator->GetSystemAddress(), LWOOBJID_EMPTY,
		true, true, true, true, true, true, true
	);

	if (std::find(m_EntitiesOnRail.begin(), m_EntitiesOnRail.end(), originator->GetObjectID()) != m_EntitiesOnRail.end()) {
		// Stop the initial effects
		if (!m_StartEffect.second.empty()) {
			GameMessages::SendStopFXEffect(originator, false, std::to_string(m_StartEffect.first));
		}

		// Start the looping effects
		if (!m_LoopEffect.second.empty()) {
			GameMessages::SendPlayFXEffect(originator->GetObjectID(), m_LoopEffect.first, m_LoopEffect.second,
				std::to_string(m_LoopEffect.first));
		}

		if (!m_LoopAnimation.empty()) {
			GameMessages::SendPlayAnimation(originator, m_LoopAnimation);
		}

		GameMessages::SendSetRailMovement(originator->GetObjectID(), m_PathDirection, m_Path, m_PathStart,
			originator->GetSystemAddress(), m_ComponentID,
			m_Parent->GetObjectID());
	}
}

void RailActivatorComponent::OnCancelRailMovement(Entity* originator) {
	// Remove the stun from the originator
	GameMessages::SendSetStunned(originator->GetObjectID(), POP, originator->GetSystemAddress(), LWOOBJID_EMPTY,
		true, true, true, true, true, true, true
	);

	auto* rebuildComponent = m_Parent->GetComponent<RebuildComponent>();

	if (rebuildComponent != nullptr) {
		// Set back reset time
		rebuildComponent->SetResetTime(rebuildComponent->GetResetTime() - 10.0f);
	}

	if (std::find(m_EntitiesOnRail.begin(), m_EntitiesOnRail.end(), originator->GetObjectID()) != m_EntitiesOnRail.end()) {
		// Stop the looping effects
		if (!m_LoopEffect.second.empty()) {
			GameMessages::SendStopFXEffect(originator, false, std::to_string(m_LoopEffect.first));
		}

		// Start the end effects
		if (!m_StopEffect.second.empty()) {
			GameMessages::SendPlayFXEffect(originator->GetObjectID(), m_StopEffect.first, m_StopEffect.second,
				std::to_string(m_StopEffect.first));
		}

		if (!m_StopAnimation.empty()) {
			GameMessages::SendPlayAnimation(originator, m_StopAnimation);
		}

		// Remove the player after they've signalled they're done railing
		m_EntitiesOnRail.erase(std::remove(m_EntitiesOnRail.begin(), m_EntitiesOnRail.end(),
			originator->GetObjectID()), m_EntitiesOnRail.end());
	}
}
