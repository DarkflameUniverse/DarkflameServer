#pragma once

#include <chrono>
#include <functional>
#include <algorithm>
#include <optional>
#include "SpareBackoff.h"
#include <map>
#include <set>
#include <vector>
#include "dCommonVars.h"
#include "IServerOperations.h"
#include "RakNetTypes.h"
#include "Stamps.h"
#include "dZMCommon.h"
#include "Logger.h"
#include "master/InstanceMigration.h"

struct Player {
	LWOOBJID id;
	SystemAddress addr;
};

struct PendingInstanceRequest {
	uint64_t id;
	bool mythranShift;
	SystemAddress sysAddr;
	Stamps stamps; // The login stamps that came with the request; they go back in the response
};

class Instance {
public:
	Instance(const std::string& ip, uint32_t port, LWOMAPID mapID, LWOINSTANCEID instanceID, LWOCLONEID cloneID, int softCap, int hardCap, bool isPrivate = false, std::string password = "") {
		m_IP = ip;
		m_Port = port;
		m_ZoneID = LWOZONEID(mapID, instanceID, cloneID);
		m_MaxClientsSoftCap = softCap;
		m_MaxClientsHardCap = hardCap;
		m_StartedHardCap = hardCap;
		m_CurrentClientCount = 0;
		m_IsPrivate = isPrivate;
		m_Password = password;
		m_Shutdown = false; //by default we are not shutting down
		m_PendingAffirmations = {};
		m_PendingRequests = {};
		m_Ready = false;
		m_IsShuttingDown = false;
	}

	const std::string& GetIP() const { return m_IP; }
	uint32_t GetPort() const { return m_Port; }
	const LWOZONEID& GetZoneID() const { return m_ZoneID; }

	LWOMAPID GetMapID() const { return m_ZoneID.GetMapID(); }
	LWOINSTANCEID GetInstanceID() const { return m_ZoneID.GetInstanceID(); }
	LWOCLONEID GetCloneID() const { return m_ZoneID.GetCloneID(); }

	bool GetIsPrivate() const { return m_IsPrivate; }
	const std::string& GetPassword() const { return m_Password; }

	bool GetIsReady() const { return m_Ready; }
	void SetIsReady(bool value) { m_Ready = value; }
	bool GetIsShuttingDown() const { return m_IsShuttingDown; }
	void SetIsShuttingDown(bool value) { m_IsShuttingDown = value; }
	// Its players are being moved to another instance (InstanceMigration.h): nobody new is sent here
	bool GetIsDraining() const { return m_IsDraining; }
	void SetIsDraining(bool value) { m_IsDraining = value; }
	// Started before a live update, or on zone files that changed since: nobody new is sent here, and it stops once
	// empty (OutdatedInstances.h)
	bool GetIsOutdated() const { return m_IsOutdated; }
	void SetIsOutdated(bool value) { m_IsOutdated = value; }
	// A property instance waiting for the outdated instance of the same property to stop before its world server is
	// started (so the two never both save it); requests for it wait as pending requests
	bool GetIsWaitingForOld() const { return m_WaitingForOld; }
	void SetIsWaitingForOld(bool value) { m_WaitingForOld = value; }
	// When its players were last told an update is waiting (outdated properties)
	std::optional<std::chrono::steady_clock::time_point> GetLastUpdateNotice() const { return m_LastUpdateNotice; }
	void SetLastUpdateNotice(std::chrono::steady_clock::time_point value) { m_LastUpdateNotice = value; }
	// Seats held for players being moved in; they count towards the caps until the move is over
	int GetReserved() const { return m_Reserved; }
	void SetReserved(int value) { m_Reserved = std::max(0, value); }
	std::vector<PendingInstanceRequest>& GetPendingRequests() { return m_PendingRequests; }
	std::vector<PendingInstanceRequest>& GetPendingAffirmations() { return m_PendingAffirmations; }

	int GetHardCap() const { return m_MaxClientsHardCap; }
	int GetSoftCap() const { return m_MaxClientsSoftCap; }
	int GetCurrentClientCount() const { return m_CurrentClientCount; }

	// New caps for a running instance: the hard cap can't go above what its world server was started with
	void SetCaps(int softCap, int hardCap) {
		m_MaxClientsHardCap = std::min(hardCap, m_StartedHardCap);
		m_MaxClientsSoftCap = std::min(softCap, m_MaxClientsHardCap);
	}

	void SetAffirmationTimeout(const uint32_t value) { m_AffirmationTimeout = value; }
	uint32_t GetAffirmationTimeout() const { return m_AffirmationTimeout; }

	void AddPlayer(Player player) { /*m_Players.push_back(player);*/ m_CurrentClientCount++; }
	void RemovePlayer(Player player) {
		m_CurrentClientCount--;
		if (m_CurrentClientCount < 0) m_CurrentClientCount = 0;
		/*for (size_t i = 0; i < m_Players.size(); ++i)
			if (m_Players[i].addr == player.addr) m_Players.erase(m_Players.begin() + i);*/
	}

	void SetSysAddr(SystemAddress sysAddr) { m_SysAddr = sysAddr; }
	const SystemAddress& GetSysAddr() const { return m_SysAddr; }

	void SetShutdownComplete(bool value);
	bool GetShutdownComplete() const;

	void Shutdown();

	bool IsFull(bool isFriendTransfer) const;

	// What instance migrations and live updates plan with
	InstanceMigration::InstanceView View() const {
		InstanceMigration::InstanceView view;
		view.zoneId = GetMapID();
		view.instanceId = GetInstanceID();
		view.cloneId = GetCloneID();
		view.players = GetCurrentClientCount();
		view.softCap = GetSoftCap();
		view.hardCap = GetHardCap();
		view.reserved = GetReserved();
		view.ready = GetIsReady();
		view.isPrivate = GetIsPrivate();
		view.shuttingDown = GetIsShuttingDown() || GetShutdownComplete();
		view.draining = GetIsDraining();
		view.outdated = GetIsOutdated();
		return view;
	}

private:
	std::string m_IP{};
	uint32_t m_Port{};
	LWOZONEID m_ZoneID{};
	int m_MaxClientsSoftCap{};
	int m_MaxClientsHardCap{};
	int m_StartedHardCap{}; // the -maxclients its world server was started with
	int m_CurrentClientCount{};
	std::vector<Player> m_Players{};
	SystemAddress m_SysAddr{};
	bool m_Ready{};
	bool m_IsShuttingDown{};
	bool m_IsDraining{};
	bool m_IsOutdated{};
	bool m_WaitingForOld{};
	std::optional<std::chrono::steady_clock::time_point> m_LastUpdateNotice{};
	int m_Reserved{};
	std::vector<PendingInstanceRequest> m_PendingRequests{};
	std::vector<PendingInstanceRequest> m_PendingAffirmations{};

	uint32_t m_AffirmationTimeout{};

	bool m_IsPrivate{};
	std::string m_Password{};

	bool m_Shutdown{};

	//Private functions:
};

using InstancePtr = std::unique_ptr<Instance>;

class InstanceManager {
public:
	InstanceManager(const std::string& externalIP);

	const InstancePtr& GetInstance(LWOMAPID mapID, bool isFriendTransfer, LWOCLONEID cloneID); //Creates an instance if none found
	bool IsPortInUse(uint32_t port);
	uint32_t GetFreePort();

	void AddPlayer(SystemAddress systemAddr, LWOMAPID mapID, LWOINSTANCEID instanceID);
	void RemovePlayer(SystemAddress systemAddr, LWOMAPID mapID, LWOINSTANCEID instanceID);

	const std::vector<InstancePtr>& GetInstances() const;
	void AddInstance(InstancePtr& instance);

	// Called on the main thread whenever a world is launched, becomes ready or is removed (for the dashboard's list)
	void SetOnInstancesChanged(std::function<void()> callback) { m_OnInstancesChanged = std::move(callback); }
	void RemoveInstance(const InstancePtr& instance);

	void ReadyInstance(const InstancePtr& instance);
	void RequestAffirmation(const InstancePtr& instance, const PendingInstanceRequest& request);
	void AffirmTransfer(const InstancePtr& instance, uint64_t transferID);

	void RedirectPendingRequests(const InstancePtr& instance);

	const InstancePtr& GetInstanceBySysAddr(const SystemAddress& sysAddr);

	const InstancePtr& FindInstance(LWOMAPID mapID, bool isFriendTransfer, LWOCLONEID cloneId = 0);
	const InstancePtr& FindInstance(LWOMAPID mapID, LWOINSTANCEID instanceID);
	const InstancePtr& FindInstanceWithPrivate(LWOMAPID mapID, LWOINSTANCEID instanceID);

	const InstancePtr& CreatePrivateInstance(LWOMAPID mapID, LWOCLONEID cloneID, const std::string& password);
	const InstancePtr& FindPrivateInstance(const std::string& password);
	void SetIsShuttingDown(bool value) { this->m_IsShuttingDown = value; };
	void PruneUnreadyInstances();

	// Start a new public instance of a zone even when one with room is running (instance migrations)
	const InstancePtr& StartNewInstance(LWOMAPID mapID, LWOCLONEID cloneID) { return CreateInstance(mapID, cloneID); }

	/**
	 * Player caps and spare instances set per zone on the dashboard (zone_limits). Caps apply to public instances
	 * started from now on, and running ones get the new caps too (a hard cap only as high as they were started with).
	 */
	void LoadZoneLimits();

	// Start an instance of each zone that has fewer instances with room than its spare_instances (one per call)
	void KeepSpareInstances();

	/**
	 * Outdated instances (OutdatedInstances.h): reminds players on outdated properties that an update is waiting, and
	 * stops outdated instances nobody is in or on the way to (not public instances of prestart_worlds zones: the live
	 * update or reload replaces those). Called every frame; does its work once a second.
	 */
	void UpdateOutdatedInstances();

	// Mark the running instances which() picks outdated (old binary or old zone files); returns how many were new
	uint32_t MarkOutdated(const std::function<bool(const Instance&)>& which);

private:
	std::string mExternalIP;
	std::vector<std::unique_ptr<Instance>> m_Instances;
	uint16_t m_LastPort = 3000;
	LWOINSTANCEID m_LastInstanceID;

	/**
	 * Whether or not the master server is currently shutting down.
	 */
	bool m_IsShuttingDown = false;

	std::map<LWOMAPID, IServerOperations::ZoneLimit> m_ZoneLimits;
	std::map<LWOMAPID, SpareBackoff> m_SpareBackoff;

	//Private functions:
	int GetSoftCap(LWOMAPID mapID);
	int GetHardCap(LWOMAPID mapID);
	const InstancePtr& CreateInstance(LWOMAPID mapID, LWOCLONEID cloneID);
	// Start the world servers of property instances whose outdated predecessor is gone
	void StartWaitingInstances();
	std::function<void()> m_OnInstancesChanged;
	std::chrono::steady_clock::time_point m_NextOutdatedCheck{};
};
