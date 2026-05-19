#include "BouncerComponent.h"

#include "EntityManager.h"
#include "dZoneManager.h"
#include "SwitchComponent.h"
#include "Game.h"
#include "Logger.h"
#include "GameMessages.h"
#include "BitStream.h"
#include "eTriggerEventType.h"
#include "Amf3.h"

BouncerComponent::BouncerComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {
	m_PetEnabled = false;
	m_PetBouncerEnabled = false;
	m_PetSwitchLoaded = false;
	m_Destination = GeneralUtils::TryParse<NiPoint3>(
		GeneralUtils::SplitString(m_Parent->GetVarAsString(u"bouncer_destination"), '\x1f'))
		.value_or(NiPoint3Constant::ZERO);
	m_Speed = GeneralUtils::TryParse<float>(m_Parent->GetVarAsString(u"bouncer_speed")).value_or(-1.0f);
	m_UsesHighArc = GeneralUtils::TryParse<bool>(m_Parent->GetVarAsString(u"bouncer_uses_high_arc")).value_or(false);
	m_LockControls = GeneralUtils::TryParse<bool>(m_Parent->GetVarAsString(u"lock_controls")).value_or(false);
	m_IgnoreCollision = !GeneralUtils::TryParse<bool>(m_Parent->GetVarAsString(u"ignore_collision")).value_or(true);
	m_StickLanding = GeneralUtils::TryParse<bool>(m_Parent->GetVarAsString(u"stickLanding")).value_or(false);
	m_UsesGroupName = GeneralUtils::TryParse<bool>(m_Parent->GetVarAsString(u"uses_group_name")).value_or(false);
	m_GroupName = m_Parent->GetVarAsString(u"grp_name");
	m_MinNumTargets = GeneralUtils::TryParse<int32_t>(m_Parent->GetVarAsString(u"num_targets_to_activate")).value_or(1);
	m_CinematicPath = m_Parent->GetVarAsString(u"attached_cinematic_path");

	if (parent->GetLOT() == 7625) {
		LookupPetSwitch();
	}

	RegisterMsg(&BouncerComponent::MsgGetObjectReportInfo);
}

BouncerComponent::~BouncerComponent() {
}

void BouncerComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	outBitStream.Write(m_PetEnabled);
	if (m_PetEnabled) {
		outBitStream.Write(m_PetBouncerEnabled);
	}
}

Entity* BouncerComponent::GetParentEntity() const {
	return m_Parent;
}

void BouncerComponent::SetPetEnabled(bool value) {
	m_PetEnabled = value;

	Game::entityManager->SerializeEntity(m_Parent);
}

void BouncerComponent::SetPetBouncerEnabled(bool value) {
	m_PetBouncerEnabled = value;

	GameMessages::SendBouncerActiveStatus(m_Parent->GetObjectID(), value, UNASSIGNED_SYSTEM_ADDRESS);

	Game::entityManager->SerializeEntity(m_Parent);

	if (value) {
		m_Parent->TriggerEvent(eTriggerEventType::PET_ON_SWITCH, m_Parent);
		GameMessages::SendPlayFXEffect(m_Parent->GetObjectID(), 1513, u"create", "PetOnSwitch", LWOOBJID_EMPTY, 1, 1, true);
	} else {
		m_Parent->TriggerEvent(eTriggerEventType::PET_OFF_SWITCH, m_Parent);
		GameMessages::SendStopFXEffect(m_Parent, true, "PetOnSwitch");
	}

}

bool BouncerComponent::GetPetEnabled() const {
	return m_PetEnabled;
}

bool BouncerComponent::GetPetBouncerEnabled() const {
	return m_PetBouncerEnabled;
}

void BouncerComponent::LookupPetSwitch() {
	const auto& groups = m_Parent->GetGroups();

	for (const auto& group : groups) {
		const auto& entities = Game::entityManager->GetEntitiesInGroup(group);

		for (auto* entity : entities) {
			auto* switchComponent = entity->GetComponent<SwitchComponent>();

			if (switchComponent != nullptr) {
				switchComponent->SetPetBouncer(this);

				m_PetSwitchLoaded = true;
				m_PetEnabled = true;

				Game::entityManager->SerializeEntity(m_Parent);

				LOG("Loaded pet bouncer");
			}
		}
	}

	if (!m_PetSwitchLoaded) {
		LOG("Failed to load pet bouncer");

		m_Parent->AddCallbackTimer(0.5f, [this]() {
			LookupPetSwitch();
			});
	}
}

bool BouncerComponent::MsgGetObjectReportInfo(GameMessages::GetObjectReportInfo& reportInfo) {
	auto& cmptType = reportInfo.info->PushDebug("Bouncer");
	cmptType.PushDebug<AMFIntValue>("Component ID") = GetComponentID();
	auto& destPos = cmptType.PushDebug("Destination Position");
	if (m_Destination != NiPoint3Constant::ZERO) {
		destPos.PushDebug(m_Destination);
	} else {
		destPos.PushDebug("<font color=\'#FF0000\'>WARNING:</font> Bouncer has no target position, is likely missing config data");
	}


	if (m_Speed == -1.0f) {
		cmptType.PushDebug("<font color=\'#FF0000\'>WARNING:</font> Bouncer has no speed value, is likely missing config data");
	} else {
		cmptType.PushDebug<AMFDoubleValue>("Bounce Speed") = m_Speed;
	}
	cmptType.PushDebug<AMFStringValue>("Bounce trajectory arc") = m_UsesHighArc ? "High Arc" : "Low Arc";
	cmptType.PushDebug<AMFBoolValue>("Collision Enabled") = m_IgnoreCollision;
	cmptType.PushDebug<AMFBoolValue>("Stick Landing") = m_StickLanding;
	cmptType.PushDebug<AMFBoolValue>("Locks character's controls") = m_LockControls;
	if (!m_CinematicPath.empty()) cmptType.PushDebug<AMFStringValue>("Cinematic Camera Path (plays during bounce)") = m_CinematicPath;

	auto* switchComponent = m_Parent->GetComponent<SwitchComponent>();
	auto& respondsToFactions = cmptType.PushDebug("Responds to Factions");
	if (!switchComponent || switchComponent->GetFactionsToRespondTo().empty()) respondsToFactions.PushDebug("Faction 1");
	else {
		for (const auto faction : switchComponent->GetFactionsToRespondTo()) {
			respondsToFactions.PushDebug(("Faction " + std::to_string(faction)));
		}
	}

	cmptType.PushDebug<AMFBoolValue>("Uses a group name for interactions") = m_UsesGroupName;
	if (!m_UsesGroupName) {
		if (m_MinNumTargets > 1) {
			cmptType.PushDebug("<font color=\'#FF0000\'>WARNING:</font> Bouncer has a required number of objects to activate, but no group for interactions.");
		}

		if (!m_GroupName.empty()) {
			cmptType.PushDebug("<font color=\'#FF0000\'>WARNING:</font> Has a group name for interactions , but is marked to not use that name.");
		}
	} else {
		if (m_GroupName.empty()) {
			cmptType.PushDebug("<font color=\'#FF0000\'>WARNING:</font> Set to use a group name for inter actions, but no group name is assigned");
		}
		cmptType.PushDebug<AMFIntValue>("Number of interactions to activate bouncer") = m_MinNumTargets;
	}

	return true;
}
