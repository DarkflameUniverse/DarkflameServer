#include "InstanceManager.h"
#include <string>
#include "Game.h"
#include "dServer.h"
#include "Logger.h"
#include "dConfig.h"
#include "CDClientDatabase.h"
#include "CDClientManager.h"
#include "CDZoneTableTable.h"
#include "MasterPackets.h"
#include "BitStreamUtils.h"
#include "ServiceType.h"
#include "MessageType/Master.h"

#include "Start.h"

using std::make_unique;

namespace {
	const InstancePtr g_Empty{ nullptr };
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

	// If we are shutting down, return a nullptr so a new instance is not created.
	if (m_IsShuttingDown) {
		LOG("Tried to create a new instance map/instance/clone %i/%i/%i, but Master is shutting down.",
			mapID,
			m_LastInstanceID + 1,
			cloneID);
		return g_Empty;
	}
	//TODO: Update this so that the IP is read from a configuration file instead

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
}

void InstanceManager::RemoveInstance(const InstancePtr& instance) {
	for (uint32_t i = 0; i < m_Instances.size(); ++i) {
		if (m_Instances[i] == instance) {
			instance->SetShutdownComplete(true);

			if (!Game::ShouldShutdown()) RedirectPendingRequests(instance);

			m_Instances.erase(m_Instances.begin() + i);

			break;
		}
	}
}

void InstanceManager::ReadyInstance(const InstancePtr& instance) {
	instance->SetIsReady(true);

	auto& pending = instance->GetPendingRequests();

	for (const auto& request : pending) {
		const auto& zoneId = instance->GetZoneID();

		LOG("Responding to pending request %llu -> %i (%i)", request, zoneId.GetMapID(), zoneId.GetCloneID());

		MasterPackets::SendZoneTransferResponse(
			Game::server,
			request.sysAddr,
			request.id,
			request.mythranShift,
			zoneId.GetMapID(),
			zoneId.GetInstanceID(),
			zoneId.GetCloneID(),
			instance->GetIP(),
			instance->GetPort()
		);
	}

	pending.clear();
}

void InstanceManager::RequestAffirmation(const InstancePtr& instance, const PendingInstanceRequest& request) {
	instance->GetPendingAffirmations().push_back(request);

	CBITSTREAM;

	BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::AFFIRM_TRANSFER_REQUEST);

	bitStream.Write(request.id);

	Game::server->Send(bitStream, instance->GetSysAddr(), false);

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

		const auto& zoneId = instance->GetZoneID();

		MasterPackets::SendZoneTransferResponse(
			Game::server,
			request.sysAddr,
			request.id,
			request.mythranShift,
			zoneId.GetMapID(),
			zoneId.GetInstanceID(),
			zoneId.GetCloneID(),
			instance->GetIP(),
			instance->GetPort()
		);

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

const InstancePtr& InstanceManager::GetInstanceBySysAddr(SystemAddress& sysAddr) {
	for (const auto& instance : m_Instances) {
		if (instance && instance->GetSysAddr() == sysAddr) {
			return instance;
		}
	}

	return g_Empty;
}

const InstancePtr& InstanceManager::FindInstance(LWOMAPID mapID, bool isFriendTransfer, LWOCLONEID cloneId) {
	for (const auto& i : m_Instances) {
		if (i && i->GetMapID() == mapID && i->GetCloneID() == cloneId && !i->IsFull(isFriendTransfer) && !i->GetIsPrivate() && !i->GetShutdownComplete() && !i->GetIsShuttingDown()) {
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

	if (m_Instances.back()) return m_Instances.back();
	else LOG("Failed to create a new instance!");

	return g_Empty;
}

const InstancePtr& InstanceManager::FindPrivateInstance(const std::string& password) {
	for (const auto& instance : m_Instances) {
		if (!instance) continue;

		if (!instance->GetIsPrivate()) {
			continue;
		}

		LOG("Password: %s == %s => %d", password.c_str(), instance->GetPassword().c_str(), password == instance->GetPassword());

		if (instance->GetPassword() == password) {
			return instance;
		}
	}

	return g_Empty;
}

int InstanceManager::GetSoftCap(LWOMAPID mapID) {
	const CDZoneTable* zone = CDZoneTableTable::Query(mapID);

	// Default to 8 which is the cap for most worlds.
	return zone ? zone->population_soft_cap : 8;
}

int InstanceManager::GetHardCap(LWOMAPID mapID) {
	const CDZoneTable* zone = CDZoneTableTable::Query(mapID);

	// Default to 12 which is the cap for most worlds.
	return zone ? zone->population_hard_cap : 12;
}

void Instance::SetShutdownComplete(const bool value) {
	m_Shutdown = value;
}

bool Instance::GetShutdownComplete() const {
	return m_Shutdown;
}

void Instance::Shutdown() {
	CBITSTREAM;

	BitStreamUtils::WriteHeader(bitStream, ServiceType::MASTER, MessageType::Master::SHUTDOWN);

	Game::server->Send(bitStream, this->m_SysAddr, false);

	LOG("Triggered world shutdown for zone/clone/instance %i/%i/%i", GetMapID(), GetCloneID(), GetInstanceID());
}


bool Instance::IsFull(bool isFriendTransfer) const {
	if (!isFriendTransfer && GetSoftCap() > GetCurrentClientCount())
		return false;
	else if (isFriendTransfer && GetHardCap() > GetCurrentClientCount())
		return false;

	return true;
}

