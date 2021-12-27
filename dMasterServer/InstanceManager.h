#pragma once
#include <vector>
#include "dCommonVars.h"
#include "RakNetTypes.h"
#include "dZMCommon.h"
#include "dLogger.h"
#include "nlohmann/json.hpp"

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
	std::vector<PendingInstanceRequest>& GetPendingRequests() { return m_PendingRequests; }
	std::vector<PendingInstanceRequest>& GetPendingAffirmations() { return m_PendingAffirmations; }
	
	int GetHardCap() const { return m_MaxClientsHardCap; }
	int GetSoftCap() const { return m_MaxClientsSoftCap; }
	int GetCurrentClientCount() const { return m_CurrentClientCount; }

	void SetAffirmationTimeout(const uint32_t value) { m_AffirmationTimeout = value; }
	uint32_t GetAffirmationTimeout() const { return m_AffirmationTimeout; }

	void AddPlayer() {
		m_CurrentClientCount++; 
	}
	
	void RemovePlayer() { 
		m_CurrentClientCount--;
	}

	void SetSysAddr(SystemAddress sysAddr) { m_SysAddr = sysAddr; }
	const SystemAddress& GetSysAddr() const { return m_SysAddr; }

	void SetShutdownComplete(bool value);
	bool GetShutdownComplete() const;
	bool GetShutdownRequested() const { return m_ShutdownRequested; }
	
	void Shutdown();

	nlohmann::json GetJson();

private:
	std::string m_IP;
	uint32_t m_Port;
	LWOZONEID m_ZoneID;
	int m_MaxClientsSoftCap;
	int m_MaxClientsHardCap;
	int m_CurrentClientCount;
	SystemAddress m_SysAddr;
	bool m_Ready;
	std::vector<PendingInstanceRequest> m_PendingRequests;
	std::vector<PendingInstanceRequest> m_PendingAffirmations;
	bool m_ShutdownRequested = false;

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
	std::vector<SystemAddress> GetShutdownInstances() const;
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

	SystemAddress GetChatSysAddr() { return m_ChatAddress; }
	void SetChatSysAddr(SystemAddress sysAddr) { m_ChatAddress = sysAddr; }

private:
	dLogger* mLogger;
	std::string mExternalIP;
	std::vector<Instance*> m_Instances;
	std::vector<SystemAddress> m_ShutdownInstances;
	unsigned short m_LastPort;
	LWOINSTANCEID m_LastInstanceID;
	SystemAddress m_ChatAddress;

	//Private functions:
	bool IsInstanceFull(Instance* instance, bool isFriendTransfer);
	int GetSoftCap(LWOMAPID mapID);
	int GetHardCap(LWOMAPID mapID);
};
