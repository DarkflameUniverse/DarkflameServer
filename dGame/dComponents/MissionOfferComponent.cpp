/*
 * Darkflame Universe
 * Copyright 2019
 */

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include "MissionOfferComponent.h"
#include "ObjectMessages.h"
#include "CDClientManager.h"
#include "CDMissionsTable.h"
#include "CDMissionNPCComponentTable.h"
#include "GameMessages.h"
#include "MissionMessages.h"
#include "Entity.h"
#include "MissionComponent.h"
#include "Logger.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "MissionPrerequisites.h"
#include "eMissionState.h"

#include "CDComponentsRegistryTable.h"

OfferedMission::OfferedMission(const uint32_t missionId, const bool offersMission, const bool acceptsMission) {
	this->missionId = missionId;
	this->offersMission = offersMission;
	this->acceptsMission = acceptsMission;
}


uint32_t OfferedMission::GetMissionId() const {
	return this->missionId;
}

bool OfferedMission::GetOffersMission() const {
	return this->offersMission;
}

bool OfferedMission::GetAcceptsMission() const {
	return this->acceptsMission;
}

//------------------------ MissionOfferComponent below ------------------------

MissionOfferComponent::MissionOfferComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {
	RegisterMsg(&MissionOfferComponent::OnRequestUse);
}

// The MissionNPCComponent rows: the missions this offers and accepts
void MissionOfferComponent::LoadDataFromTemplate() {
	if (m_ComponentID != -1) {
		// Now lookup the missions in the MissionNPCComponent table
		auto* missionNpcComponentTable = CDClientManager::GetTable<CDMissionNPCComponentTable>();

		auto missions = missionNpcComponentTable->Query([this](const CDMissionNPCComponent& entry) {
			return entry.id == static_cast<unsigned>(m_ComponentID);
			});

		for (auto& mission : missions) {
			this->offeredMissions.emplace_back(mission.missionID, mission.offersMission, mission.acceptsMission);
		}
	}
}

namespace {
	std::string_view Trimmed(std::string_view text) {
		while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
		while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
		return text;
	}

	// "Why are we sending it twice, once to a non-player object?"
	// The first one (targeting the offerer) makes the client zoom into the object.
	// The second (targeting the player) actually makes the UI pop up so the mission can be offered.
	// Why is it like this? Because LU isn't just a clown, it's the entire circus.
	void OfferMissionToPlayer(const Entity& player, const int32_t missionID, const LWOOBJID offerer) {
		GameMessages::OfferMission offerMission;
		offerMission.missionID = missionID;
		offerMission.offerer = offerer;

		offerMission.target = offerer;
		offerMission.SendToClient(player.GetSystemAddress());

		offerMission.target = player.GetObjectID();
		offerMission.SendToClient(player.GetSystemAddress());
	}
}

bool MissionOfferComponent::OnRequestUse(GameMessages::RequestUseEvent& event) {
	if (event.msg.IsMissionChoice()) OfferMissions(event.user, event.msg.multiInteractID);
	else OfferMissions(event.user);
	return true;
}

std::optional<MissionOfferEntry> MissionOfferComponent::GetOfferEntry(const uint32_t missionID, const int32_t state, const bool cooldownRunning, const bool offers, const bool accepts, const bool alsoCompleted) {
	constexpr auto AVAILABLE = static_cast<int32_t>(eMissionState::AVAILABLE);
	constexpr auto ACTIVE = static_cast<int32_t>(eMissionState::ACTIVE);
	constexpr auto READY_TO_COMPLETE = static_cast<int32_t>(eMissionState::READY_TO_COMPLETE);
	constexpr auto COMPLETE = static_cast<int32_t>(eMissionState::COMPLETE);

	// Ready to turn in: only where it is accepted
	if (state & READY_TO_COMPLETE) {
		if (offers && accepts) return MissionOfferEntry{ 5, missionID, state, false };
		if (accepts) return MissionOfferEntry{ 0, missionID, state, true };
		return std::nullopt;
	}

	// In progress: shown by every giver that has it
	if (state & ACTIVE) return MissionOfferEntry{ 5, missionID, state, false };

	// Available: only where it is offered, and not while its cooldown runs
	if (state & AVAILABLE) {
		if (!offers || cooldownRunning) return std::nullopt;
		return MissionOfferEntry{ static_cast<uint8_t>((state & COMPLETE) ? 15 : 10), missionID, state, false };
	}

	if (alsoCompleted && (state & COMPLETE) && !cooldownRunning) return MissionOfferEntry{ 20, missionID, state, !offers && accepts };

	return std::nullopt;
}

void MissionOfferComponent::InsertByPriority(std::vector<MissionOfferEntry>& entries, const MissionOfferEntry& entry) {
	const auto after = std::ranges::find_if(entries, [&entry](const MissionOfferEntry& other) { return other.priority > entry.priority; });
	entries.insert(after, entry);
}

std::vector<uint32_t> MissionOfferComponent::ParseRandomPool(const std::string& randomPool) {
	std::vector<uint32_t> pool;
	for (const auto& token : GeneralUtils::SplitString(randomPool, ',')) {
		const auto sample = GeneralUtils::TryParse<uint32_t>(Trimmed(token));
		if (sample) pool.push_back(*sample);
	}
	return pool;
}

const OfferedMission* MissionOfferComponent::GetOfferedMission(const uint32_t missionID) const {
	const auto offered = std::ranges::find_if(offeredMissions, [missionID](const OfferedMission& other) { return other.GetMissionId() == missionID; });
	return offered == offeredMissions.end() ? nullptr : &*offered;
}

std::optional<MissionOfferEntry> MissionOfferComponent::GetEntryFor(const MissionComponent& missionComponent, const OfferedMission& offered, const bool alsoCompleted) {
	const auto missionID = offered.GetMissionId();
	const auto state = missionComponent.GetClientMissionState(missionID);
	return GetOfferEntry(missionID, state, missionComponent.IsCooldownRunning(missionID), offered.GetOffersMission(), offered.GetAcceptsMission(), alsoCompleted);
}

std::vector<MissionOfferEntry> MissionOfferComponent::GetMissionsForPlayer(const MissionComponent& missionComponent, const bool alsoCompleted) const {
	// The client keeps the giver's missions by mission ID
	std::map<uint32_t, const OfferedMission*> byID;
	for (const auto& offered : offeredMissions) byID.emplace(offered.GetMissionId(), &offered);

	std::vector<MissionOfferEntry> entries;
	for (const auto& [missionID, offered] : byID) {
		CDMissions info{};
		if (!Mission::IsValidMission(missionID, info)) continue;

		if (!info.isRandom) {
			if (const auto entry = GetEntryFor(missionComponent, *offered, alsoCompleted)) InsertByPriority(entries, *entry);
			continue;
		}

		// A random mission (0x00c5ffd0): of its pool, the missions this giver has. One to turn in here first, then one
		// in progress, else a random one
		std::vector<MissionOfferEntry> pool;
		for (const auto sample : ParseRandomPool(info.randomPool)) {
			const auto sampleOffered = byID.find(sample);
			if (sampleOffered == byID.end()) continue;
			if (const auto entry = GetEntryFor(missionComponent, *sampleOffered->second, alsoCompleted)) InsertByPriority(pool, *entry);
		}
		if (pool.empty()) continue;

		const auto withPriority = [&pool](const uint8_t priority) {
			return std::ranges::find_if(pool, [priority](const MissionOfferEntry& entry) { return entry.priority == priority; });
		};
		auto chosen = withPriority(0);
		if (chosen == pool.end()) chosen = withPriority(5);
		if (chosen == pool.end()) chosen = pool.begin() + GeneralUtils::GenerateRandomNumber<size_t>(0, pool.size() - 1);
		InsertByPriority(entries, *chosen);
	}

	return entries;
}

// The server offers the first mission of the giver's list (live: one OfferMission per use), or the one picked from the
// client's multi interaction menu
void MissionOfferComponent::OfferMissions(Entity* entity, const uint32_t specifiedMissionId) {
	if (!entity) return;
	auto* missionComponent = entity->GetComponent<MissionComponent>();

	if (!missionComponent) {
		LOG("Unable to get mission component for Entity %llu", entity->GetObjectID());
		return;
	}

	const auto entries = GetMissionsForPlayer(*missionComponent, false);

	if (specifiedMissionId == 0) {
		if (!entries.empty()) OfferMissionToPlayer(*entity, entries.front().missionID, m_Parent->GetObjectID());
		return;
	}

	if (std::ranges::any_of(entries, [specifiedMissionId](const MissionOfferEntry& entry) { return entry.missionID == specifiedMissionId; })) {
		OfferMissionToPlayer(*entity, specifiedMissionId, m_Parent->GetObjectID());
		return;
	}

	// The client rolls its own mission of a random pool; offer that one if it is in a pool of this giver and listable
	const auto* const sampleOffered = GetOfferedMission(specifiedMissionId);
	if (!sampleOffered || !GetEntryFor(*missionComponent, *sampleOffered, false)) return;
	for (const auto& offered : offeredMissions) {
		CDMissions info{};
		if (!Mission::IsValidMission(offered.GetMissionId(), info) || !info.isRandom) continue;
		const auto pool = ParseRandomPool(info.randomPool);
		if (std::ranges::find(pool, specifiedMissionId) != pool.end()) {
			OfferMissionToPlayer(*entity, specifiedMissionId, m_Parent->GetObjectID());
			return;
		}
	}
}

// LWOMissionOfferComponent's RequestLinkedMission handler (0x00cd6eb0): the giver's list without the mission the window
// was for; a new mission at its head is offered. Live offered nothing after an accept, and the next mission after a
// turn in, once per RequestLinkedMission.
void MissionOfferComponent::OfferLinkedMission(Entity* entity, const uint32_t missionID) {
	if (!entity) return;
	const auto* const missionComponent = entity->GetComponent<MissionComponent>();
	if (!missionComponent) return;

	const auto next = GetLinkedMission(GetMissionsForPlayer(*missionComponent, false), missionID);
	if (next) OfferMissionToPlayer(*entity, *next, m_Parent->GetObjectID());
}

std::optional<uint32_t> MissionOfferComponent::GetLinkedMission(std::vector<MissionOfferEntry> entries, const uint32_t missionID) {
	if (!entries.empty() && entries.front().missionID == missionID) entries.erase(entries.begin());
	if (entries.empty()) return std::nullopt;

	const auto state = entries.front().state;
	if ((state & static_cast<int32_t>(eMissionState::AVAILABLE)) && !(state & static_cast<int32_t>(eMissionState::COMPLETE))) {
		return entries.front().missionID;
	}
	return std::nullopt;
}
