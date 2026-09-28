#include "InstanceManager.h"
#include <string>
#include <algorithm>
#include <chrono>
#include "Game.h"
#include "dServer.h"
#include "Logger.h"
#include "dConfig.h"
#include "CDClientDatabase.h"
#include "CDClientManager.h"
#include "CDZoneTableTable.h"
#include "Database.h"
#include "MasterPackets.h"
#include "BitStreamUtils.h"
#include "ServiceType.h"
#include "MessageType/Master.h"

#include "Start.h"

using std::make_unique;

namespace {
	const InstancePtr g_Empty{ nullptr };

	// Where the player of request goes: instance
	void SendZoneTransferResponse(const PendingInstanceRequest& request, const Instance& instance) {
		const auto& zoneId = instance.GetZoneID();
		MasterPackets::RequestZoneTransferResponse response;
		response.requestID = request.id;
		response.mythranShift = request.mythranShift;
		response.zoneID = zoneId.GetMapID();
		response.zoneInstance = zoneId.GetInstanceID();
		response.zoneClone = zoneId.GetCloneID();
		response.serverPort = static_cast<uint16_t>(instance.GetPort());
		response.serverIP = LUString(instance.GetIP(), 255);
		response.stamps = request.stamps;
		response.stamps.Add(eStamps::PASSPORT_AUTH_WORLD_SESSION_CONFIRM_TO_AUTH, zoneId.GetInstanceID());
		MasterPackets::SendTo(request.sysAddr, response);
	}
}

InstanceManager::InstanceManager(const std::string& externalIP) : mExternalIP{ externalIP } {
	m_LastPort =
		GeneralUtils::TryParse<uint16_t>(Game::config->GetValue("world_port_start")).value_or(m_LastPort);
	m_LastInstanceID = LWOINSTANCEID_INVALID;
}

const InstancePtr& InstanceManager::GetInstance(LWOMAPID mapID, bool isFriendTransfer, LWOCLONEID cloneID) {
	LOG("Searching for an instance for mapID %i/%i", mapID, cloneID);
	auto& instance = FindInstance(mapID, isFriendTransfer, cloneID);
	if (instance) return instance;

	return CreateInstance(mapID, cloneID);
}

const InstancePtr& InstanceManager::CreateInstance(LWOMAPID mapID, LWOCLONEID cloneID) {
	// If we are shutting down, return a nullptr so a new instance is not created.
	if (m_IsShuttingDown) {
		LOG("Tried to create a new instance map/instance/clone %i/%i/%i, but Master is shutting down.",
			mapID,
			m_LastInstanceID + 1,
			cloneID);
		return g_Empty;
	}
	int softCap = 8;
	int maxPlayers = 12;

	if (mapID == 0) {
		softCap = 999;
		maxPlayers = softCap;
	} else {
		softCap = GetSoftCap(mapID);
		maxPlayers = GetHardCap(mapID);
	}

	uint32_t port = GetFreePort();
	auto newInstance = make_unique<Instance>(mExternalIP, port, mapID, ++m_LastInstanceID, cloneID, softCap, maxPlayers);

	//Start the actual process:
	StartWorldServer(mapID, port, m_LastInstanceID, maxPlayers, cloneID);

	m_Instances.push_back(std::move(newInstance));
	if (m_OnInstancesChanged) m_OnInstancesChanged();

	if (m_Instances.back()) {
		LOG("Created new instance: %i/%i/%i with min/max %i/%i", mapID, m_LastInstanceID, cloneID, softCap, maxPlayers);
		return m_Instances.back();
	} else LOG("Failed to create a new instance!");

	return g_Empty;
}

bool InstanceManager::IsPortInUse(uint32_t port) {
	for (const auto& i : m_Instances) {
		if (i && i->GetPort() == port) {
			return true;
		}
	}

	return false;
}

uint32_t InstanceManager::GetFreePort() {
	uint32_t port = m_LastPort;
	std::vector<uint32_t> usedPorts;
	for (const auto& i : m_Instances) {
		usedPorts.push_back(i->GetPort());
	}

	std::sort(usedPorts.begin(), usedPorts.end());

	int portIdx = 0;
	while (portIdx < usedPorts.size() && port == usedPorts[portIdx]) {
		//increment by 3 since each instance uses 3 ports (instance, world-server, world-chat)
		port += 3;
		portIdx++;
	}

	return port;
}

void InstanceManager::AddPlayer(SystemAddress systemAddr, LWOMAPID mapID, LWOINSTANCEID instanceID) {
	const auto& inst = FindInstance(mapID, instanceID);
	if (inst) {
		Player player;
		player.addr = systemAddr;
		player.id = 0; //TODO: Update this to include the LWOOBJID of the player's character.
		inst->AddPlayer(player);
	}
}

void InstanceManager::RemovePlayer(SystemAddress systemAddr, LWOMAPID mapID, LWOINSTANCEID instanceID) {
	const auto& inst = FindInstance(mapID, instanceID);
	if (inst) {
		Player player;
		player.addr = systemAddr;
		player.id = 0; //TODO: Update this to include the LWOOBJID of the player's character.
		inst->RemovePlayer(player);
	}
}

const std::vector<InstancePtr>& InstanceManager::GetInstances() const {
	return m_Instances;
}

void InstanceManager::AddInstance(InstancePtr& instance) {
	if (instance == nullptr) return;

	m_Instances.push_back(std::move(instance));
	if (m_OnInstancesChanged) m_OnInstancesChanged();
}

void InstanceManager::RemoveInstance(const InstancePtr& instance) {
	for (uint32_t i = 0; i < m_Instances.size(); ++i) {
		if (m_Instances[i] == instance) {
			instance->SetShutdownComplete(true);

			if (!Game::ShouldShutdown()) RedirectPendingRequests(instance);

			m_Instances.erase(m_Instances.begin() + i);
			if (m_OnInstancesChanged) m_OnInstancesChanged();

			break;
		}
	}
}

void InstanceManager::ReadyInstance(const InstancePtr& instance) {
	instance->SetIsReady(true);
	if (m_OnInstancesChanged) m_OnInstancesChanged();

	auto& pending = instance->GetPendingRequests();

	for (const auto& request : pending) {
		const auto& zoneId = instance->GetZoneID();

		LOG("Responding to pending request %llu -> %i (%i)", request, zoneId.GetMapID(), zoneId.GetCloneID());

		SendZoneTransferResponse(request, *instance);
	}

	pending.clear();
}

void InstanceManager::RequestAffirmation(const InstancePtr& instance, const PendingInstanceRequest& request) {
	instance->GetPendingAffirmations().push_back(request);

	MasterPackets::AffirmTransferRequest affirm;
	affirm.requestID = request.id;
	MasterPackets::SendTo(instance->GetSysAddr(), affirm);

	LOG("Sent affirmation request %llu to %i/%i", request.id,
		static_cast<int>(instance->GetZoneID().GetMapID()),
		static_cast<int>(instance->GetZoneID().GetCloneID())
	);
}

void InstanceManager::AffirmTransfer(const InstancePtr& instance, const uint64_t transferID) {
	auto& pending = instance->GetPendingAffirmations();

	for (auto i = 0u; i < pending.size(); ++i) {
		const auto& request = pending[i];

		if (request.id != transferID) continue;

		SendZoneTransferResponse(request, *instance);

		pending.erase(pending.begin() + i);

		break;
	}
}

void InstanceManager::RedirectPendingRequests(const InstancePtr& instance) {
	const auto& zoneId = instance->GetZoneID();

	for (const auto& request : instance->GetPendingAffirmations()) {
		const auto& in = Game::im->GetInstance(zoneId.GetMapID(), false, zoneId.GetCloneID());

		if (in && !in->GetIsReady()) // Instance not ready, make a pending request
		{
			in->GetPendingRequests().push_back(request);

			continue;
		}

		Game::im->RequestAffirmation(in, request);
	}
}

const InstancePtr& InstanceManager::GetInstanceBySysAddr(const SystemAddress& sysAddr) {
	for (const auto& instance : m_Instances) {
		if (instance && instance->GetSysAddr() == sysAddr) {
			return instance;
		}
	}

	return g_Empty;
}

const InstancePtr& InstanceManager::FindInstance(LWOMAPID mapID, bool isFriendTransfer, LWOCLONEID cloneId) {
	for (const auto& i : m_Instances) {
		if (i && InstanceMigration::AcceptsNewPlayers(i->View(), mapID, cloneId, isFriendTransfer)) {
			return i;
		}
	}

	return g_Empty;
}

const InstancePtr& InstanceManager::FindInstance(LWOMAPID mapID, LWOINSTANCEID instanceID) {
	for (const auto& i : m_Instances) {
		if (i && i->GetMapID() == mapID && i->GetInstanceID() == instanceID && !i->GetIsPrivate() && !i->GetShutdownComplete() && !i->GetIsShuttingDown()) {
			return i;
		}
	}

	return g_Empty;
}

const InstancePtr& InstanceManager::FindInstanceWithPrivate(LWOMAPID mapID, LWOINSTANCEID instanceID) {
	for (const auto& i : m_Instances) {
		if (i && i->GetMapID() == mapID && i->GetInstanceID() == instanceID && !i->GetShutdownComplete() && !i->GetIsShuttingDown()) {
			return i;
		}
	}

	return g_Empty;
}

const InstancePtr& InstanceManager::CreatePrivateInstance(LWOMAPID mapID, LWOCLONEID cloneID, const std::string& password) {
	const auto& instance = FindPrivateInstance(password);

	if (instance != nullptr) {
		return instance;
	}

	if (m_IsShuttingDown) {
		LOG("Tried to create a new private instance map/instance/clone %i/%i/%i, but Master is shutting down.",
			mapID,
			m_LastInstanceID + 1,
			cloneID);
		return g_Empty;
	}

	int maxPlayers = 999;

	uint32_t port = GetFreePort();
	auto newInstance = make_unique<Instance>(mExternalIP, port, mapID, ++m_LastInstanceID, cloneID, maxPlayers, maxPlayers, true, password);

	//Start the actual process:
	StartWorldServer(mapID, port, m_LastInstanceID, maxPlayers, cloneID);

	m_Instances.push_back(std::move(newInstance));
	if (m_OnInstancesChanged) m_OnInstancesChanged();

	if (m_Instances.back()) return m_Instances.back();
	else LOG("Failed to create a new instance!");

	return g_Empty;
}

const InstancePtr& InstanceManager::FindPrivateInstance(const std::string& password) {
	for (const auto& instance : m_Instances) {
		if (!instance) continue;

		// A private instance being replaced (live update) takes nobody new: its replacement has the same password
		if (!instance->GetIsPrivate() || instance->GetIsDraining() || instance->GetIsShuttingDown() || instance->GetShutdownComplete()) {
			continue;
		}

		LOG("Checking private zone password match (result: %d)", password == instance->GetPassword());

		if (instance->GetPassword() == password) {
			return instance;
		}
	}

	return g_Empty;
}

int InstanceManager::GetSoftCap(LWOMAPID mapID) {
	const auto limit = m_ZoneLimits.find(mapID);
	if (limit != m_ZoneLimits.end() && limit->second.softCap) return std::min(static_cast<int>(*limit->second.softCap), GetHardCap(mapID));

	const CDZoneTable* zone = CDZoneTableTable::Query(mapID);

	// Default to 8 which is the cap for most worlds.
	return std::min(zone ? static_cast<int>(zone->population_soft_cap) : 8, GetHardCap(mapID));
}

int InstanceManager::GetHardCap(LWOMAPID mapID) {
	const auto limit = m_ZoneLimits.find(mapID);
	if (limit != m_ZoneLimits.end() && limit->second.hardCap) return static_cast<int>(*limit->second.hardCap);

	const CDZoneTable* zone = CDZoneTableTable::Query(mapID);

	// Default to 12 which is the cap for most worlds.
	return zone ? zone->population_hard_cap : 12;
}

void InstanceManager::LoadZoneLimits() {
	std::vector<IServerOperations::ZoneLimit> rows;
	try {
		rows = Database::Get()->GetZoneLimits();
	} catch (const std::exception& ex) {
		LOG("Could not load zone limits, using the client's caps: %s", ex.what());
		return;
	}
	m_ZoneLimits.clear();
	for (auto& row : rows) {
		if (row.zoneId == 0) continue; // character selection is never capped
		LOG("Zone %u: soft cap %s, hard cap %s, %u spare instance(s)", row.zoneId, row.softCap ? std::to_string(*row.softCap).c_str() : "default",
			row.hardCap ? std::to_string(*row.hardCap).c_str() : "default", row.spareInstances);
		m_ZoneLimits[row.zoneId] = std::move(row);
	}
	for (const auto& instance : m_Instances) {
		if (!instance || instance->GetIsPrivate() || instance->GetMapID() == 0) continue;
		instance->SetCaps(GetSoftCap(instance->GetMapID()), GetHardCap(instance->GetMapID()));
	}
}

void InstanceManager::KeepSpareInstances() {
	if (m_IsShuttingDown) return;
	const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	for (const auto& [mapID, limit] : m_ZoneLimits) {
		if (limit.spareInstances == 0) continue;
		// Follow the spare we started last: one that stops soon after starting counts as a failure and delays the next
		auto& backoff = m_SpareBackoff[mapID];
		if (const auto watched = backoff.Watched()) {
			const auto it = std::find_if(m_Instances.begin(), m_Instances.end(), [&](const InstancePtr& instance) {
				return instance && instance->GetMapID() == mapID && instance->GetInstanceID() == watched && !instance->GetShutdownComplete();
			});
			if (it == m_Instances.end()) {
				backoff.Lost(now);
				LOG("Spare instance %u of zone %u stopped soon after starting; waiting %lld s before starting another", watched, mapID,
					static_cast<long long>(SpareBackoff::Delay(backoff.Failures())));
			} else {
				backoff.Running((*it)->GetIsReady(), now);
			}
		}
		if (!backoff.CanStart(now)) continue;
		uint32_t withRoom = 0;
		for (const auto& instance : m_Instances) {
			// One still starting counts: it has room, and starting another would double up
			if (instance && instance->GetMapID() == mapID && instance->GetCloneID() == 0 && !instance->GetIsPrivate() &&
				!instance->GetShutdownComplete() && !instance->GetIsShuttingDown() && !instance->IsFull(false)) withRoom++;
		}
		if (withRoom >= limit.spareInstances) continue;
		LOG("Zone %u has %u instance(s) with room and should have %u; starting one", mapID, withRoom, limit.spareInstances);
		const auto& started = CreateInstance(mapID, 0);
		if (started) backoff.Started(started->GetInstanceID());
	}
}

void InstanceManager::PruneUnreadyInstances() {
	for (int i = static_cast<int>(m_Instances.size()) - 1; i >= 0; i--) {
		if (!m_Instances[i]->GetIsReady()) m_Instances.erase(m_Instances.cbegin() + i);
	}
}

void Instance::SetShutdownComplete(const bool value) {
	m_Shutdown = value;
}

bool Instance::GetShutdownComplete() const {
	return m_Shutdown;
}

void Instance::Shutdown() {
	MasterPackets::SendTo(this->m_SysAddr, MasterPackets::Shutdown());

	LOG("Triggered world shutdown for zone/clone/instance %i/%i/%i", GetMapID(), GetCloneID(), GetInstanceID());
}


bool Instance::IsFull(bool isFriendTransfer) const {
	// Seats held for players being moved in count as taken
	const int load = GetCurrentClientCount() + GetReserved();
	if (!isFriendTransfer && GetSoftCap() > load)
		return false;
	else if (isFriendTransfer && GetHardCap() > load)
		return false;

	return true;
}
