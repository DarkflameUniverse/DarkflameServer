#pragma once
#include <vector>
#include "dCommonVars.h"
#include "RakNetTypes.h"
#include "dZMCommon.h"
#include "dLogger.h"

struct Player {
	LWOOBJID id;
	SystemAddress addr;
};

struct PendingInstanceRequest {
	uint64_t id;
	bool mythranShift;
	SystemAddress sysAddr;
};

class Instance {
public:
	Instance(const std::string& ip, uint32_t port, LWOMAPID mapID, LWOINSTANCEID instanceID, LWOCLONEID cloneID, int softCap, int hardCap, bool isPrivate = false, std::string password = "") {
		m_IP = ip;
		m_Port = port;
		m_ZoneID = LWOZONEID(mapID, instanceID, cloneID);
		m_MaxClientsSoftCap = softCap;
		m_MaxClientsHardCap = hardCap;
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
	std::vector<PendingInstanceRequest>& GetPendingRequests() { return m_PendingRequests; }
	std::vector<PendingInstanceRequest>& GetPendingAffirmations() { return m_PendingAffirmations; }

	int GetHardCap() const { return m_MaxClientsHardCap; }
	int GetSoftCap() const { return m_MaxClientsSoftCap; }
	int GetCurrentClientCount() const { return m_CurrentClientCount; }

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

private:
	std::string m_IP;
	uint32_t m_Port;
	LWOZONEID m_ZoneID;
	int m_MaxClientsSoftCap;
	int m_MaxClientsHardCap;
	int m_CurrentClientCount;
	std::vector<Player> m_Players;
	SystemAddress m_SysAddr;
	bool m_Ready;
	bool m_IsShuttingDown;
	std::vector<PendingInstanceRequest> m_PendingRequests;
	std::vector<PendingInstanceRequest> m_PendingAffirmations;

	uint32_t m_AffirmationTimeout;

	bool m_IsPrivate;
	std::string m_Password;

	bool m_Shutdown;

	//Private functions:
};

class InstanceManager {
public:
	InstanceManager(dLogger* logger, const std::string& externalIP);
	~InstanceManager();

	Instance* GetInstance(LWOMAPID mapID, bool isFriendTransfer, LWOCLONEID cloneID); //Creates an instance if none found
	bool IsPortInUse(uint32_t port);
	uint32_t GetFreePort();

	void AddPlayer(SystemAddress systemAddr, LWOMAPID mapID, LWOINSTANCEID instanceID);
	void RemovePlayer(SystemAddress systemAddr, LWOMAPID mapID, LWOINSTANCEID instanceID);

	std::vector<Instance*> GetInstances() const;
	void AddInstance(Instance* instance);
	void RemoveInstance(Instance* instance);

	void ReadyInstance(Instance* instance);
	void RequestAffirmation(Instance* instance, const PendingInstanceRequest& request);
	void AffirmTransfer(Instance* instance, uint64_t transferID);

	void RedirectPendingRequests(Instance* instance);

	Instance* GetInstanceBySysAddr(SystemAddress& sysAddr);

	Instance* FindInstance(LWOMAPID mapID, bool isFriendTransfer, LWOCLONEID cloneId = 0);
	Instance* FindInstance(LWOMAPID mapID, LWOINSTANCEID instanceID);

	Instance* CreatePrivateInstance(LWOMAPID mapID, LWOCLONEID cloneID, const std::string& password);
	Instance* FindPrivateInstance(const std::string& password);

private:
	dLogger* mLogger;
	std::string mExternalIP;
	std::vector<Instance*> m_Instances;
	unsigned short m_LastPort;
	LWOINSTANCEID m_LastInstanceID;

	//Private functions:
	bool IsInstanceFull(Instance* instance, bool isFriendTransfer);
	int GetSoftCap(LWOMAPID mapID);
	int GetHardCap(LWOMAPID mapID);
};
