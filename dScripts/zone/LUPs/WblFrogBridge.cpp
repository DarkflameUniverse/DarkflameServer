#include "WblFrogBridge.h"

#include "Entity.h"
#include "EntityManager.h"
#include "eMissionState.h"
#include "MissionComponent.h"
#include "MovingPlatformComponent.h"
#include "ProximityMonitorComponent.h"

namespace {
	constexpr uint32_t BRIDGE_MISSION = 946;
}

void WblFrogBridge::OnStartup(Entity* self) {
	self->SetProximityRadius(25.0f, "frog");
	self->SetVar<bool>(u"TongueOut", false);
}

void WblFrogBridge::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (name == "frog" && status == "ENTER") StickOutTongue(self);
}

// Piece 1-4: the first object of group FrogBridge01..04
void WblFrogBridge::MovePiece(const int piece, const uint32_t waypoint) {
	const auto pieces = Game::entityManager->GetEntitiesInGroup("FrogBridge0" + std::to_string(piece));
	if (pieces.empty()) return;
	if (auto* movingPlatform = pieces.front()->GetComponent<MovingPlatformComponent>()) movingPlatform->GotoWaypoint(waypoint, true);
}

void WblFrogBridge::StickOutTongue(Entity* self) {
	if (self->GetVar<bool>(u"TongueOut")) return;
	auto* proximity = self->GetComponent<ProximityMonitorComponent>();
	if (!proximity) return;
	for (const auto id : proximity->GetProximityObjects("frog")) {
		auto* player = Game::entityManager->GetEntity(id);
		auto* missions = player ? player->GetComponent<MissionComponent>() : nullptr;
		if (!missions || missions->GetMissionState(BRIDGE_MISSION) < eMissionState::COMPLETE) continue;
		for (int piece = 1; piece <= 4; piece++) MovePiece(piece, 1);
		self->SetVar<bool>(u"TongueOut", true);
		self->AddTimer("tongueWait", 10.0f);
		return;
	}
}

void WblFrogBridge::OnTimerDone(Entity* self, std::string timerName) {
	// Back one piece a second, from the tip, then 7 seconds before the frog looks for players again
	if (timerName == "tongueWait") {
		self->AddTimer("back4", 1.0f);
		MovePiece(4, 0);
	} else if (timerName == "back4") {
		self->AddTimer("back3", 1.0f);
		MovePiece(3, 0);
	} else if (timerName == "back3") {
		self->AddTimer("back2", 1.0f);
		MovePiece(2, 0);
	} else if (timerName == "back2") {
		self->AddTimer("tongueIn", 7.0f);
		MovePiece(1, 0);
	} else if (timerName == "tongueIn") {
		self->SetVar<bool>(u"TongueOut", false);
		StickOutTongue(self);
	}
}
