#include "NjSpinnerServer.h"

#include "BuffComponent.h"
#include "EffectsMessages.h"
#include "Entity.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "MovingPlatformComponent.h"
#include "Preconditions.h"
#include "ProximityMonitorComponent.h"
#include "RenderComponent.h"
#include "SkillComponent.h"
#include "Spawner.h"
#include "dZoneManager.h"

namespace {
	// L_NJ_SPINNER_SERVER's defaults when the level config sets nothing
	constexpr float DefaultSpinDistance = 3.5f;
	constexpr float DefaultDamageRadius = 8.0f;
	constexpr float DefaultPulseTime = 1.0f;
	constexpr float DefaultResetTime = 0.0f;
	constexpr float DamageStartDelay = 1.0f; // after reaching the active position, before the damage proximity starts
	const std::u16string DefaultSpawnFXType = u"spawner_activate";

	// The player's animations, and the spinner's
	const std::string StartAnim = "spinjitzu-staff-windup";
	const std::string RunAnim = "spinjitzu-staff-loop";
	const std::string EndAnim = "spinjitzu-staff-end";
	const std::string DownAnimModifier = "-down";
	const std::u16string UpAnim = u"up";
	const std::u16string DownAnim = u"down";
	const std::u16string IdleDownAnim = u"idle";
	const std::u16string IdleUpAnim = u"idle-up";

	const std::string AudioMovementBegin = "{5c30c263-00ae-42a2-80a3-2ae33c8f13fe}";
	const std::string AudioMovementEnd = "{40e86d71-084c-4149-884e-ab9b45b694dc}";
	const std::string AudioMovement = "{b91d2f01-1998-4ca3-bb5c-c25cf36c7a24}";
	const std::string AudioSpinning = "{8dcf670f-9fe9-4f14-b5ba-0a8e0943c2f6}";

	constexpr uint32_t BladeSkill = 971;
	constexpr uint32_t ArrowSkill = 972;

	std::string Text(Entity* self, const std::u16string& name) { return self->GetVarAsString(name); }
	float Number(Entity* self, const std::u16string& name, const float fallback) {
		return self->HasVar(name) ? self->GetVarAs<float>(name) : fallback;
	}
	bool Flag(Entity* self, const std::u16string& name) { return self->HasVar(name) && self->GetVar<bool>(name); }
	int32_t StoppedNode(Entity* self) { return self->GetVar<int32_t>(u"stoppedNode"); }
}

NjSpinnerServer::Element NjSpinnerServer::GetElement(Entity* self) {
	const auto element = Text(self, u"element_type");
	if (element == "fire") return { "spinner_interaction", 144 };
	if (element == "earth") return { "spinner_interaction", 145 };
	if (element == "ice") return { "spinner_interaction", 146 };
	if (element == "lightning") return { "spinner_interaction", 147 };
	if (element == "collision") return { "green-switch", 143 };
	if (element == "collision_off") return { "red-switch", 143 };
	return { "spinner_interaction", 143 }; // imagination
}

std::string NjSpinnerServer::Audio(Entity* self, const std::u16string& var, const std::string& variantDefault, const std::string& baseDefault) const {
	const auto set = Text(self, var);
	if (!set.empty()) return set;
	return variantDefault.empty() ? baseDefault : variantDefault;
}

void NjSpinnerServer::OnStartup(Entity* self) {
	// The variant scripts' defaults
	if (!self->HasVar(u"type") && !m_Defaults.type.empty()) self->SetVar<std::u16string>(u"type", m_Defaults.type);
	if (!self->HasVar(u"element_type") && !m_Defaults.elementType.empty()) self->SetVar<std::u16string>(u"element_type", m_Defaults.elementType);

	if (!self->HasVar(u"attached_path") && !Flag(self, u"platformIsSimpleMover")) {
		LOG("Spinner %llu (LOT %i) has no attached_path", self->GetObjectID(), self->GetLOT());
		return;
	}

	self->SetProximityRadius(Number(self, u"spin_distance", DefaultSpinDistance), "spin_distance");

	if (auto* const platform = self->GetComponent<MovingPlatformComponent>()) platform->StopPathing();

	const auto startNode = self->GetVarAs<int32_t>(u"attached_path_start");
	self->SetVar<int32_t>(u"stoppedNode", startNode);

	if (Text(self, u"element_type") != "collision" && !Flag(self, u"static") && !Flag(self, u"noninteract")) {
		self->SetNetworkVar<bool>(u"bInteractive", true);
		SetCapFX(self, true);
	} else if (!self->GetNetworkVar<bool>(u"bInteractive") || Flag(self, u"noninteract")) {
		self->SetNetworkVar<bool>(u"bInteractive", false);
		SetCapFX(self, false);
	}

	self->SetNetworkVar<int32_t>(u"element_precondition_ID", GetElement(self).precondition);

	if (startNode == 1) {
		ToggleActive(self, false);
		self->SetNetworkVar<std::u16string>(u"current_anim", IdleUpAnim);
		ToggleDamageProx(self, true);
		ToggleSpawnerNetwork(self, true);
		self->SetVar<bool>(u"movingUp", false);
	} else {
		self->SetVar<bool>(u"movingUp", true);
	}
}

void NjSpinnerServer::SetCapFX(Entity* self, const bool on) {
	if (Flag(self, u"static")) return;
	const auto cap = GetElement(self);
	if (on) {
		GameMessages::PlayFXEffect(self->GetObjectID(), -1, GeneralUtils::ASCIIToUTF16(cap.capFX), "element_" + cap.capFX).Send(UNASSIGNED_SYSTEM_ADDRESS);
		self->SetVar<bool>(u"bCapShown", true);
	} else {
		GameMessages::StopFXEffect(self->GetObjectID(), true, "element_" + cap.capFX).Send(UNASSIGNED_SYSTEM_ADDRESS);
		self->SetVar<bool>(u"bCapShown", false);
	}
}

void NjSpinnerServer::PlayAnim(Entity* self, Entity* player, const std::string& animation) {
	const auto time = RenderComponent::GetAnimationTime(player, animation);
	if (time <= 0.0f) return;
	const auto name = self->GetVar<bool>(u"movingUp") ? animation : animation + DownAnimModifier;
	RenderComponent::PlayAnimation(player, name, 2.0f);
	self->AddTimer("AnimDone_" + animation, time);
}

void NjSpinnerServer::OnUse(Entity* self, Entity* user) {
	// The use requirements: not in use, the object's CheckPrecondition, the element's precondition
	if (self->GetNetworkVar<bool>(u"bIsInUse")) return;
	const auto preconditions = Text(self, u"CheckPrecondition");
	if (!preconditions.empty() && !PreconditionExpression(preconditions).Check(user)) return;
	const auto element = GetElement(self).precondition;
	if (element != -1 && !Preconditions::Check(user, element)) return;

	if (!self->GetVar<bool>(u"bCapShown")) return;

	self->SetNetworkVar<bool>(u"bFreezePlayer_" + GeneralUtils::to_u16string(user->GetObjectID()), true);
	self->SetNetworkVar<bool>(u"bIsInUse", true);
	GameMessages::PlayFXEffect(user->GetObjectID(), 5499, u"on-anim", "tornado").Send(UNASSIGNED_SYSTEM_ADDRESS);
	GameMessages::PlayFXEffect(user->GetObjectID(), 5502, u"on-anim", "staff").Send(UNASSIGNED_SYSTEM_ADDRESS);

	self->SetVar<LWOOBJID>(u"userID", user->GetObjectID());
	PlayAnim(self, user, StartAnim);
}

void NjSpinnerServer::SetActiveState(Entity* self, const bool active, const bool playerTriggered) {
	self->SetVar<bool>(u"bActive", active);
	if (playerTriggered) PlayCinematic(self, active);

	if (active) {
		// An arrow spinner shoots every skill_pulse_time while it is up
		if (Text(self, u"type") == "arrow") self->AddTimer("Pulse_Skill", Number(self, u"skill_pulse_time", DefaultPulseTime));
		self->SetVar<bool>(u"movingUp", true);
		self->SetNetworkVar<std::u16string>(u"current_anim", UpAnim);
	} else {
		self->SetVar<bool>(u"movingUp", false);
		self->SetNetworkVar<std::u16string>(u"current_anim", DownAnim);
	}
}

void NjSpinnerServer::ToggleActive(Entity* self, const bool playerTriggered) {
	SetActiveState(self, !self->GetVar<bool>(u"bActive"), playerTriggered);
}

void NjSpinnerServer::ToggleSpawnerNetwork(Entity* self, const bool active) {
	std::vector<std::string> networks;
	for (int32_t i = 1; self->HasVar(u"spawnNw" + GeneralUtils::to_u16string(i)); i++) networks.push_back(Text(self, u"spawnNw" + GeneralUtils::to_u16string(i)));
	if (networks.empty()) return;

	if (active && !Flag(self, u"NoSpawnerFX")) {
		const auto fx = Text(self, u"spawnFXType");
		GameMessages::PlayFXEffect(self->GetObjectID(), -1, fx.empty() ? DefaultSpawnFXType : GeneralUtils::ASCIIToUTF16(fx), "spawnFX").Send(UNASSIGNED_SYSTEM_ADDRESS);
	}

	for (const auto& network : networks) {
		const auto spawners = Game::zoneManager->GetSpawnersByName(network);
		if (spawners.empty()) {
			LOG("Spinner %llu: spawner network %s doesn't exist", self->GetObjectID(), network.c_str());
			continue;
		}
		for (auto* const spawner : spawners) {
			if (active) {
				spawner->Activate();
				self->SetVar<int32_t>(u"TotalSpawnedAlive", self->GetVar<int32_t>(u"TotalSpawnedAlive") + spawner->m_Info.maxToSpawn);
				// Lua's NotifySpawnerOfDeath request, once per network
				const auto watched = u"watching_" + GeneralUtils::ASCIIToUTF16(network);
				if (self->GetVar<bool>(watched)) continue;
				self->SetVar<bool>(watched, true);
				const auto selfId = self->GetObjectID();
				spawner->AddSpawnedEntityDieCallback([this, selfId, network]() {
					if (auto* const spinner = Game::entityManager->GetEntity(selfId)) OnSpawnedDied(spinner, network);
				});
			} else {
				spawner->DestroyAllEntities();
				spawner->Deactivate();
			}
		}
	}
}

// notifyNotifySpawnerOfDeath: when everything the networks spawned is dead, the spinner goes down
void NjSpinnerServer::OnSpawnedDied(Entity* self, const std::string& network) {
	if (!self->HasVar(u"TotalSpawnedAlive")) return;
	const auto alive = self->GetVar<int32_t>(u"TotalSpawnedAlive") - 1;
	if (alive < 0) return;
	self->SetVar<int32_t>(u"TotalSpawnedAlive", alive);

	for (auto* const spawner : Game::zoneManager->GetSpawnersByName(network)) {
		if (spawner->GetAmountSpawned() == 0) {
			spawner->Deactivate();
			spawner->Reset();
		}
	}
	if (alive > 0) return;
	ActivateSpinner(self, false, false);
}

void NjSpinnerServer::PlayCinematic(Entity* self, const bool on) {
	auto* const player = Game::entityManager->GetEntity(self->GetVar<LWOOBJID>(u"userID"));
	if (!player) return;
	const auto cinematic = self->GetVar<std::u16string>(on ? u"on_cinematic" : u"off_cinematic");
	if (cinematic.empty()) return;
	if (auto* const buffs = player->GetComponent<BuffComponent>()) buffs->RemoveBuff(60);
	GameMessages::PlayCinematic message;
	message.target = player->GetObjectID();
	message.pathName = cinematic;
	message.Send(player->GetSystemAddress());
}

void NjSpinnerServer::CastSkill(Entity* self) {
	const auto type = Text(self, u"type");
	if (type.empty() || type == "elevator" || !self->GetVar<bool>(u"bActive")) return;
	auto* const skills = self->GetComponent<SkillComponent>();
	if (skills) skills->CastSkill(type == "arrow" ? ArrowSkill : BladeSkill);
}

void NjSpinnerServer::ToggleSpinner(Entity* self, const bool playerTriggered) {
	ActivateSpinner(self, StoppedNode(self) != 1, playerTriggered);
}

void NjSpinnerServer::ActivateSpinner(Entity* self, const bool activate, const bool playerTriggered) {
	const int32_t gotoNode = activate ? 1 : 0;
	if (gotoNode == StoppedNode(self)) return;

	self->CancelAllTimers();
	self->SetNetworkVar<bool>(u"bIsInUse", true);
	self->SetVar<bool>(u"bTraveling", true);

	if (playerTriggered) {
		const auto resetTime = Number(self, u"reset_time", DefaultResetTime);
		if (Flag(self, u"once_only")) SetCapFX(self, false);
		else if (resetTime > 0.0f) self->SetVar<bool>(u"startResetOnArrived", true);
		else self->SetVar<bool>(u"resetOnArrived", true);
	} else {
		self->SetVar<bool>(u"resetOnArrived", true);
	}

	if (Text(self, u"element_type") == "collision" && self->GetVar<bool>(u"bCapShown")) {
		SetCapFX(self, false);
		self->SetVar<std::u16string>(u"element_type", u"collision_off");
		SetCapFX(self, true);
	}

	if (auto* const platform = self->GetComponent<MovingPlatformComponent>()) platform->GotoWaypoint(gotoNode, true);
	self->SetVar<int32_t>(u"stoppedNode", gotoNode);

	GameMessages::PlayNDAudioEmitter(self->GetObjectID(), Audio(self, u"audioMovementBeginGUID", m_Defaults.audioMovementBegin, AudioMovementBegin)).Send(UNASSIGNED_SYSTEM_ADDRESS);
	self->SetNetworkVar<std::string>(u"StopAudio", "");
	self->SetNetworkVar<std::string>(u"PlayAudio", Audio(self, u"audioMovementGUID", m_Defaults.audioMovement, AudioMovement));

	SetActiveState(self, activate, playerTriggered);
}

void NjSpinnerServer::ToggleDamageProx(Entity* self, const bool on) {
	// Only a blade spinner hurts through a proximity
	if (Text(self, u"type") != "blade") return;
	const auto audio = Audio(self, u"audioSpinningGUID", m_Defaults.audioSpinning, AudioSpinning);
	if (on) {
		self->SetProximityRadius(Number(self, u"damage_radius", DefaultDamageRadius), "damage");
		self->SetNetworkVar<std::string>(u"StopAudio2", "");
		self->SetNetworkVar<std::string>(u"PlayAudio2", audio);
	} else {
		if (auto* const proximity = self->GetComponent<ProximityMonitorComponent>()) proximity->UnsetProximityRadius("damage");
		self->SetNetworkVar<std::string>(u"PlayAudio2", "");
		self->SetNetworkVar<std::string>(u"StopAudio2", audio);
	}
}

void NjSpinnerServer::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	// The level config names what each incoming event does (event1 -> 0:activate)
	const auto eventType = Text(self, GeneralUtils::ASCIIToUTF16(args));
	if (eventType.empty()) return;

	const bool canInteract = Text(self, u"element_type") != "collision" && !Flag(self, u"static");
	if (eventType == "activate") ActivateSpinner(self, true, false);
	else if (eventType == "deactivate") ActivateSpinner(self, false, false);
	else if (eventType == "toggle_activate") ToggleSpinner(self, false);
	else if (eventType == "interact") {
		if (canInteract) self->SetNetworkVar<bool>(u"bInteractive", true);
		SetCapFX(self, true);
	} else if (eventType == "noninteract") {
		if (canInteract) self->SetNetworkVar<bool>(u"bInteractive", false);
		SetCapFX(self, false);
	} else if (eventType == "toggle_interact") {
		const bool interactive = !self->GetNetworkVar<bool>(u"bInteractive");
		if (canInteract) self->SetNetworkVar<bool>(u"bInteractive", interactive);
		SetCapFX(self, interactive);
	}

	if (auto* const player = Game::entityManager->GetEntity(self->GetVar<LWOOBJID>(u"userID"))) PlayAnim(self, player, EndAnim);
}

void NjSpinnerServer::SendEvent(Entity* self, const bool on) {
	const auto name = Text(self, on ? u"name_activated_event" : u"name_deactivated_event");
	const auto group = Text(self, on ? u"group_activated_event" : u"group_deactivated_event");
	if (name.empty() || group.empty()) return;
	for (auto* const object : Game::entityManager->GetEntitiesInGroup(group)) {
		if (object) object->OnFireEventServerSide(self, name);
	}
}

void NjSpinnerServer::OnWaypointReached(Entity* self, uint32_t waypointIndex) {
	self->SetVar<bool>(u"bTraveling", false);

	if (static_cast<int32_t>(waypointIndex) == StoppedNode(self) && self->GetNetworkVar<bool>(u"bIsInUse")) {
		if (self->GetVar<bool>(u"resetOnArrived")) {
			self->SetNetworkVar<bool>(u"bIsInUse", false);
			self->SetVar<bool>(u"resetOnArrived", false);
			if (Text(self, u"element_type") == "collision_off") {
				SetCapFX(self, false);
				self->SetVar<std::u16string>(u"element_type", u"collision");
			}
			SetCapFX(self, true);
		} else if (self->GetVar<bool>(u"startResetOnArrived")) {
			self->AddTimer("Reset", Number(self, u"reset_time", DefaultResetTime));
		}
	}

	const bool active = self->GetVar<bool>(u"bActive");
	if (active) {
		self->AddTimer("Start_Damage", DamageStartDelay);
		ToggleSpawnerNetwork(self, true);
		self->SetNetworkVar<std::u16string>(u"current_anim", IdleUpAnim);
	} else {
		ToggleDamageProx(self, false);
		ToggleSpawnerNetwork(self, false);
		self->SetNetworkVar<std::u16string>(u"current_anim", IdleDownAnim);
	}

	const auto movement = Audio(self, u"audioMovementGUID", m_Defaults.audioMovement, AudioMovement);
	GameMessages::PlayNDAudioEmitter(self->GetObjectID(), Audio(self, u"audioMovementEndGUID", m_Defaults.audioMovementEnd, AudioMovementEnd)).Send(UNASSIGNED_SYSTEM_ADDRESS);
	self->SetNetworkVar<std::string>(u"PlayAudio", "");
	self->SetNetworkVar<std::string>(u"StopAudio", movement);

	if (auto* const player = Game::entityManager->GetEntity(self->GetVar<LWOOBJID>(u"userID"))) PlayAnim(self, player, EndAnim);

	SendEvent(self, active);
}

void NjSpinnerServer::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (name == "damage" && self->GetVar<bool>(u"bActive")) {
		if (status == "ENTER") {
			if (Text(self, u"type") == "blade") {
				CastSkill(self);
				self->CancelTimer("Pulse_Skill");
				self->AddTimer("Pulse_Skill", 0.5f);
				self->SetVar<float>(u"pulseInterval", 0.5f);
			}
		} else if (status == "LEAVE") {
			auto* const proximity = self->GetComponent<ProximityMonitorComponent>();
			if (!proximity || proximity->GetProximityObjects("damage").empty()) self->CancelTimer("Pulse_Skill");
		}
	} else if (name == "spin_distance") {
		const auto userId = self->GetVar<LWOOBJID>(u"userID");
		if (status == "LEAVE") {
			if (userId != LWOOBJID_EMPTY && entering && entering->GetObjectID() == userId) {
				PlayAnim(self, entering, EndAnim);
				if (self->GetNetworkVar<bool>(u"bIsInUse") && !self->GetVar<bool>(u"bTraveling")) self->SetNetworkVar<bool>(u"bIsInUse", false);
			}
		} else if (status == "ENTER" && Text(self, u"element_type") == "collision" && !self->GetVar<bool>(u"bTraveling") && self->GetVar<bool>(u"bCapShown")) {
			ToggleSpinner(self, true);
		}
	}
}

void NjSpinnerServer::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "Pulse_Skill") {
		// An activity timer with an update interval: it keeps pulsing until stopped
		if (self->GetVar<bool>(u"bActive") && !self->GetVar<bool>(u"bTraveling")) CastSkill(self);
		const auto interval = self->HasVar(u"pulseInterval") ? self->GetVar<float>(u"pulseInterval") : Number(self, u"skill_pulse_time", DefaultPulseTime);
		self->AddTimer("Pulse_Skill", interval);
	} else if (timerName == "Reset") {
		ToggleSpinner(self, false);
	} else if (timerName.starts_with("AnimDone_")) {
		const auto animation = timerName.substr(9);
		auto* const player = Game::entityManager->GetEntity(self->GetVar<LWOOBJID>(u"userID"));
		if (!player) return;
		if (animation == StartAnim) {
			ToggleSpinner(self, true);
			RenderComponent::PlayAnimation(player, RunAnim, 2.0f);
		} else if (animation == RunAnim) {
			PlayAnim(self, player, EndAnim);
		} else if (animation == EndAnim) {
			self->SetNetworkVar<bool>(u"bFreezePlayer_" + GeneralUtils::to_u16string(player->GetObjectID()), false);
			self->SetVar<LWOOBJID>(u"userID", LWOOBJID_EMPTY);
			GameMessages::StopFXEffect(player->GetObjectID(), true, "tornado").Send(UNASSIGNED_SYSTEM_ADDRESS);
			GameMessages::StopFXEffect(player->GetObjectID(), true, "staff").Send(UNASSIGNED_SYSTEM_ADDRESS);
			self->SetVar<bool>(u"movingUp", !self->GetVar<bool>(u"movingUp"));
		}
	} else if (timerName == "Start_Damage") {
		ToggleDamageProx(self, true);
	}
}
