#pragma once
#include <string>
#include <chrono>
#include <csignal>
#include <functional>
#include <map>
#include "RakPeerInterface.h"
#include "ReplicaManager.h"
#include "NetworkIDManager.h"
#include "TrafficStats.h"

class Logger;
class dConfig;
struct ServerTraffic;
struct ProfileRequest;
struct ProfileResult;
enum class eServerDisconnectIdentifiers : uint32_t;
enum class ServiceType : uint16_t;

namespace Game {
	using signal_t = volatile std::sig_atomic_t;
}

class dServer {
public:
	// Default constructor should only used for testing!
	dServer() {};
	dServer(
		const std::string& ip,
		int port,
		int instanceID,
		int maxConnections,
		bool isInternal,
		bool useEncryption,
		Logger* logger,
		const std::string masterIP,
		int masterPort,
		ServiceType serverType,
		dConfig* config,
		Game::signal_t* shouldShutdown,
		const std::string& masterPassword,
		unsigned int zoneID = 0);
	~dServer();

	Packet* ReceiveFromMaster();
	Packet* Receive();
	void DeallocatePacket(Packet* packet);
	void DeallocateMasterPacket(Packet* packet);
	virtual void Send(RakNet::BitStream& bitStream, const SystemAddress& sysAddr, bool broadcast);
	void SendToMaster(RakNet::BitStream& bitStream);

	// Sees every packet Send sends, before it goes out (the dashboard's message inspector sets it only while it
	// captures, so sending costs nothing extra otherwise). Pass nullptr to remove it.
	using SendObserver = std::function<void(const RakNet::BitStream& bitStream, const SystemAddress& sysAddr, bool broadcast)>;
	void SetSendObserver(SendObserver observer) { mSendObserver = std::move(observer); }

	void Disconnect(const SystemAddress& sysAddr, eServerDisconnectIdentifiers disconNotifyID);

	// Where this server's traffic report goes every few seconds (see ServerTraffic.h). By default it is sent to
	// master; master sends its own to the dashboard, the dashboard keeps its own.
	using TrafficSink = std::function<void(ServerTraffic& report)>;
	void SetTrafficSink(TrafficSink sink) { mTrafficSink = std::move(sink); }

	// Where this server's profiling results go (see Profiling.h). By default they are sent to master; master sends its
	// own to the dashboard, the dashboard keeps its own.
	using ProfileSink = std::function<void(ProfileResult& result)>;
	void SetProfileSink(ProfileSink sink) { mProfileSink = std::move(sink); }
	// Starts or stops a profiling session of this server's main loop (master forwards the dashboard's request)
	void HandleProfileRequest(const ProfileRequest& request);

	// Names who is on a connection in the traffic report (a world fills in the player's account and character)
	using ConnectionIdentity = std::function<void(const SystemAddress& sysAddr, TrafficStats::Connection& connection)>;
	void SetConnectionIdentity(ConnectionIdentity identity) { mConnectionIdentity = std::move(identity); }

	bool IsConnected(const SystemAddress& sysAddr);
	const std::string& GetIP() const { return mIP; }
	const int GetPort() const { return mPort; }
	const int GetMaxConnections() const { return mMaxConnections; }
	const bool GetIsEncrypted() const { return mUseEncryption; }
	const bool GetIsInternal() const { return mIsInternal; }
	const bool GetIsOkay() const { return mIsOkay; }
	Logger* GetLogger() const { return mLogger; }
	const bool GetIsConnectedToMaster() const { return mMasterConnectionActive; }
	const unsigned int GetZoneID() const { return mZoneID; }
	const int GetInstanceID() const { return mInstanceID; }
	ReplicaManager* GetReplicaManager() { return mReplicaManager; }
	void UpdateReplica();
	void UpdateBandwidthLimit();
	void UpdateMaximumMtuSize();

	int GetPing(const SystemAddress& sysAddr) const;
	int GetLatestPing(const SystemAddress& sysAddr) const;

	NetworkIDManager* GetNetworkIDManager() { return mNetIDManager; }

	ServiceType GetServerType() const { return mServerType; }

	[[nodiscard]]
	std::chrono::steady_clock::duration GetUptime() const {
		return std::chrono::steady_clock::now() - mStartTime;
	}

private:
	struct LinkCounters {
		uint64_t datagramsSent{}, datagramsReceived{}, bitsSent{}, bitsReceived{}, resends{};
	};
	bool Startup();
	// Traffic diagnostics (TrafficStats): count one packet, and send the report when it is due
	void CountTraffic(const Packet* packet, TrafficStats::Peer peer);
	void CountTraffic(const RakNet::BitStream& bitStream, bool broadcast, const SystemAddress& sysAddr, TrafficStats::Peer peer);
	// Who mPeer's connections are: other servers on master and chat (the worlds connect to chat), players elsewhere
	TrafficStats::Peer PeerOfConnections() const;
	void ReportTraffic();
	void SendProfileResult(ProfileResult& result);
	// Main thread, frame timing: the slow frame threshold (slow_frame_ms) and the log line of a slow frame
	void ConfigureProfiler();
	// Adds the peer's connections to the report's link statistics (changes since the last report)
	void AddLinkStats(RakPeerInterface* peer, uint64_t peerIndex, ServerTraffic& report, uint64_t& pingSum, std::map<uint64_t, LinkCounters>& seen);
	void Shutdown();
	void SetupForMasterConnection();
	bool ConnectToMaster();

protected:
	Logger* mLogger = nullptr;
	dConfig* mConfig = nullptr;
	RakPeerInterface* mPeer = nullptr;
	ReplicaManager* mReplicaManager = nullptr;
	NetworkIDManager* mNetIDManager = nullptr;

	/**
	 * Whether or not to shut down the server.  Pointer to Game::lastSignal.
	 */
	Game::signal_t* mShouldShutdown = nullptr;
	SocketDescriptor mSocketDescriptor;
	std::string mIP;
	// Local address the sockets are bound to (bind_ip), empty for all interfaces
	std::string mBindAddress;
	int mPort;
	int mMaxConnections;
	unsigned int mZoneID;
	int mInstanceID;
	bool mUseEncryption;
	bool mIsInternal;
	bool mIsOkay;
	bool mMasterConnectionActive;
	ServiceType mServerType;

	RakPeerInterface* mMasterPeer = nullptr;
	SocketDescriptor mMasterSocketDescriptor;
	SystemAddress mMasterSystemAddress;
	std::string mMasterIP;
	int mMasterPort;
	std::chrono::steady_clock::time_point mStartTime = std::chrono::steady_clock::now();
	std::string mMasterPassword;
	SendObserver mSendObserver;

	TrafficSink mTrafficSink;
	ProfileSink mProfileSink;
	ConnectionIdentity mConnectionIdentity;
	// RakNet's per-connection statistics are totals since the connection opened; the last ones seen, for deltas
	std::map<uint64_t, LinkCounters> mLinkCounters;
};
