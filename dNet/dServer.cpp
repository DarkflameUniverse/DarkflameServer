#define _VARIADIC_MAX 10
#include "dServer.h"
#include "dNetCommon.h"
#include "Logger.h"
#include "dConfig.h"

#include "RakNetworkFactory.h"
#include "MessageIdentifiers.h"
#include "ServiceType.h"
#include "MessageType/Server.h"
#include "MessageType/Master.h"

#include "BinaryPathFinder.h"
#include "BitStreamUtils.h"
#include "CommonPackets.h"
#include "MasterPackets.h"
#include "ZoneInstanceManager.h"
#include "StringifiedEnum.h"
#include "GeneralUtils.h"
#include "TrafficStats.h"
#include "RakNetStatistics.h"
#include "master/ServerTraffic.h"

//! Replica Constructor class
class ReplicaConstructor : public ReceiveConstructionInterface {
public:
	ReplicaReturnResult ReceiveConstruction(RakNet::BitStream* inBitStream, RakNetTime timestamp, NetworkID networkID, NetworkIDObject* existingObject, SystemAddress senderId, ReplicaManager* caller) {
		return REPLICA_PROCESSING_DONE;
	}
} ConstructionCB;

//! Replica Download Sender class
class ReplicaSender : public SendDownloadCompleteInterface {
public:
	ReplicaReturnResult SendDownloadComplete(RakNet::BitStream* outBitStream, RakNetTime currentTime, SystemAddress senderId, ReplicaManager* caller) {
		return REPLICA_PROCESSING_DONE;
	}
} SendDownloadCompleteCB;

//! Replica Download Receiver class
class ReplicaReceiever : public ReceiveDownloadCompleteInterface {
public:
	ReplicaReturnResult ReceiveDownloadComplete(RakNet::BitStream* inBitStream, SystemAddress senderId, ReplicaManager* caller) {
		return REPLICA_PROCESSING_DONE;
	}
} ReceiveDownloadCompleteCB;

dServer::dServer(
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
	Game::signal_t* lastSignal,
	const std::string& masterPassword,
	unsigned int zoneID) {
	mIP = ip;
	mPort = port;
	mZoneID = zoneID;
	mInstanceID = instanceID;
	mMaxConnections = maxConnections;
	mIsInternal = isInternal;
	mUseEncryption = useEncryption;
	mLogger = logger;
	mMasterIP = masterIP;
	mMasterPort = masterPort;
	mMasterConnectionActive = false;
	mNetIDManager = nullptr;
	mReplicaManager = nullptr;
	mServerType = serverType;
	mConfig = config;
	mMasterPassword = masterPassword;
	mShouldShutdown = lastSignal;
	//Attempt to start our server here:
	mIsOkay = Startup();

	//Forcibly log to both the console and our file what ip, port and possibly zoneID / instanceID we're running on:
	bool prevLogSetting = mLogger->GetLogToConsole();
	mLogger->SetLogToConsole(true);

	if (mIsOkay) {
		LOG("Bound to %s (bind_ip)", mBindAddress.empty() ? "all interfaces" : mBindAddress.c_str());
		if (zoneID == 0)
			LOG("%s Server is listening on %s:%i with encryption: %i", StringifiedEnum::ToString(serverType).data(), ip.c_str(), port, int(useEncryption));
		else
			LOG("%s Server is listening on %s:%i with encryption: %i, running zone %i / %i", StringifiedEnum::ToString(serverType).data(), ip.c_str(), port, int(useEncryption), zoneID, instanceID);
	} else {
		LOG("FAILED TO START SERVER ON IP/PORT: %s:%i", ip.c_str(), port);
#ifdef DARKFLAME_PLATFORM_LINUX
		if (mServerType == ServiceType::AUTH) {
			const auto cwd = BinaryPathFinder::GetBinaryDir();
			LOG("Try running the following command before launching again:\n    sudo setcap 'cap_net_bind_service=+ep' \"%s/AuthServer\"", cwd.string().c_str());
		}
#endif
		return;
	}

	mLogger->SetLogToConsole(prevLogSetting);

	//Connect to master if we are not master:
	if (serverType != ServiceType::MASTER) {
		SetupForMasterConnection();
		if (!ConnectToMaster()) {
			LOG("Failed ConnectToMaster!");
		}
	}

	//Set up Replica if we're a world server:
	if (serverType == ServiceType::WORLD) {
		mNetIDManager = new NetworkIDManager();
		mNetIDManager->SetIsNetworkIDAuthority(true);

		mReplicaManager = new ReplicaManager();
		mReplicaManager->SetAutoParticipateNewConnections(false);
		mReplicaManager->SetAutoConstructToNewParticipants(false);
		mReplicaManager->SetAutoSerializeInScope(true);
		mReplicaManager->SetReceiveConstructionCB(&ConstructionCB);
		mReplicaManager->SetDownloadCompleteCB(&SendDownloadCompleteCB, &ReceiveDownloadCompleteCB);

		mPeer->AttachPlugin(mReplicaManager);
		mPeer->SetNetworkIDManager(mNetIDManager);
	}
}

dServer::~dServer() {
	Shutdown();
}

Packet* dServer::ReceiveFromMaster() {
	if (!mMasterPeer) return nullptr;
	if (!mMasterConnectionActive) ConnectToMaster();

	Packet* packet = mMasterPeer->Receive();
	CountTraffic(packet);
	if (packet) {
		if (packet->length < 1) { mMasterPeer->DeallocatePacket(packet); return nullptr; }

		switch (packet->data[0]) {
		case ID_DISCONNECTION_NOTIFICATION:
			[[fallthrough]];
		case ID_CONNECTION_LOST: {
			LOG("Lost our connection to master, shutting DOWN!");
			mMasterConnectionActive = false;
			// ConnectToMaster(); // We'll just shut down now
			break;
		}
		case ID_CONNECTION_REQUEST_ACCEPTED: {
			LOG("Established connection to master: ServiceType (%s), Zone (%i), Instance (%i)", StringifiedEnum::ToString(this->GetServerType()).data(), this->GetZoneID(), this->GetInstanceID());
			mMasterConnectionActive = true;
			mMasterSystemAddress = packet->systemAddress;
			LOG("SendServerInfo called for server type %i", static_cast<int>(GetServerType()));
			MasterPackets::ServerInfo info;
			info.port = GetPort();
			info.zoneID = GetZoneID();
			info.instanceID = GetInstanceID();
			info.serverType = GetServerType();
			info.ip = LUString(GetIP());
			MasterPackets::SendToMaster(info, this);
			break;
		}
		case ID_USER_PACKET_ENUM: {
			RakNet::BitStream inStream(packet->data, packet->length, false);
			LUBitStream header;
			if (header.ReadHeader(inStream) && header.connectionType == ServiceType::MASTER) {
				// What every server does with these; the rest goes back to the server's own handlers
				switch (static_cast<MessageType::Master>(header.internalPacketID)) {
				case MessageType::Master::REQUEST_ZONE_TRANSFER_RESPONSE: {
					MasterPackets::RequestZoneTransferResponse response;
					if (response.Deserialize(inStream)) ZoneInstanceManager::Instance()->HandleRequestZoneTransferResponse(response);
					else LOG("Dropped a zone transfer response from master that failed to read");
					break;
				}
				case MessageType::Master::SHUTDOWN:
					*mShouldShutdown = -2;
					break;

				case MessageType::Master::CONFIG_RELOAD:
					LOG("Reloading settings (changed on the dashboard)");
					if (mConfig) mConfig->ReloadConfig();
					break;

				// When we handle these packets in World instead dServer, we just return the packet's pointer.
				default:
					return packet;
				}
			}
			break;
		}
		}

		mMasterPeer->DeallocatePacket(packet);
	}

	return nullptr;
}

Packet* dServer::Receive() {
	Packet* packet = mPeer->Receive();
	CountTraffic(packet);
	return packet;
}

void dServer::DeallocatePacket(Packet* packet) {
	mPeer->DeallocatePacket(packet);
}

void dServer::DeallocateMasterPacket(Packet* packet) {
	mMasterPeer->DeallocatePacket(packet);
}

void dServer::Send(RakNet::BitStream& bitStream, const SystemAddress& sysAddr, bool broadcast) {
	if (mSendObserver) mSendObserver(bitStream, sysAddr, broadcast);
	CountTraffic(bitStream, broadcast, sysAddr);
	mPeer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE_ORDERED, 0, sysAddr, broadcast);
}

void dServer::SendToMaster(RakNet::BitStream& bitStream) {
	if (!mMasterConnectionActive) ConnectToMaster();
	CountTraffic(bitStream, false, mMasterSystemAddress);
	mMasterPeer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE_ORDERED, 0, mMasterSystemAddress, false);
}

void dServer::Disconnect(const SystemAddress& sysAddr, eServerDisconnectIdentifiers disconNotifyID) {
	CommonPackets::DisconnectNotify notify;
	notify.disconnectID = disconNotifyID;
	RakNet::BitStream bitStream;
	notify.WritePacket(bitStream);
	CountTraffic(bitStream, false, sysAddr);
	mPeer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE_ORDERED, 0, sysAddr, false);

	mPeer->CloseConnection(sysAddr, true);
}

bool dServer::IsConnected(const SystemAddress& sysAddr) {
	return mPeer->IsConnected(sysAddr);
}

bool dServer::Startup() {
	// bind_ip picks the local interface the sockets listen on; players are still sent external_ip
	const auto bindIP = mConfig->GetValue("bind_ip");
	const auto bindAddress = GeneralUtils::ParseBindAddress(bindIP);
	if (!bindAddress) {
		LOG("bind_ip \"%s\" is not an IPv4 address (leave it empty to listen on all interfaces)", bindIP.c_str());
		return false;
	}
	mBindAddress = *bindAddress;

	mSocketDescriptor = SocketDescriptor(uint16_t(mPort), mBindAddress.c_str());
	mPeer = RakNetworkFactory::GetRakPeerInterface();

	if (!mPeer) return false;

	if (mUseEncryption) mPeer->InitializeSecurity(nullptr, nullptr, nullptr, nullptr);
	if (!mPeer->Startup(mMaxConnections, 10, &mSocketDescriptor, 1)) return false;

	if (mIsInternal) {
		mPeer->SetIncomingPassword(mMasterPassword.c_str(), mMasterPassword.size());
	} else {
		UpdateBandwidthLimit();
		UpdateMaximumMtuSize();
		mPeer->SetIncomingPassword(NET_PASSWORD_EXTERNAL, strnlen(NET_PASSWORD_EXTERNAL, sizeof(NET_PASSWORD_EXTERNAL)));
	}

	mPeer->SetMaximumIncomingConnections(mMaxConnections);

	return true;
}

void dServer::UpdateMaximumMtuSize() {
	mPeer->SetMTUSize(mConfig->GetValue<int32_t>("maximum_mtu_size", 1228));
}

void dServer::UpdateBandwidthLimit() {
	mPeer->SetPerConnectionOutgoingBandwidthLimit(mConfig->GetValue<int32_t>("maximum_outgoing_bandwidth", 0));
}

void dServer::Shutdown() {
	if (mPeer) {
		mPeer->Shutdown(1000);
		RakNetworkFactory::DestroyRakPeerInterface(mPeer);
	}

	if (mNetIDManager) {
		delete mNetIDManager;
		mNetIDManager = nullptr;
	}

	if (mReplicaManager) {
		delete mReplicaManager;
		mReplicaManager = nullptr;
	}

	if (mServerType != ServiceType::MASTER && mMasterPeer) {
		mMasterPeer->Shutdown(1000);
		RakNetworkFactory::DestroyRakPeerInterface(mMasterPeer);
	}
}

void dServer::SetupForMasterConnection() {
	mMasterSocketDescriptor = SocketDescriptor(uint16_t(mPort + 1), mBindAddress.c_str());
	mMasterPeer = RakNetworkFactory::GetRakPeerInterface();
	bool ret = mMasterPeer->Startup(1, 30, &mMasterSocketDescriptor, 1);
	if (!ret) LOG("Failed MasterPeer Startup!");
}

bool dServer::ConnectToMaster() {
	//LOG("Connection to Master %s:%d", mMasterIP.c_str(), mMasterPort);
	return mMasterPeer->Connect(mMasterIP.c_str(), mMasterPort, mMasterPassword.c_str(), mMasterPassword.size());
}

void dServer::UpdateReplica() {
	mReplicaManager->Update(mPeer);
}

int dServer::GetPing(const SystemAddress& sysAddr) const {
	return mPeer->GetAveragePing(sysAddr);
}

int dServer::GetLatestPing(const SystemAddress& sysAddr) const {
	return mPeer->GetLastPing(sysAddr);
}

void dServer::CountTraffic(const Packet* packet) {
	const auto now = TrafficStats::Now();
	if (packet) TrafficStats::Local().Packet(now, TrafficStats::KeyOf(packet->data, packet->length, false), packet->length);
	if (TrafficStats::Local().Due(now, ServerTraffic::REPORT_SECONDS)) ReportTraffic();
}

void dServer::CountTraffic(const RakNet::BitStream& bitStream, bool broadcast, const SystemAddress& sysAddr) {
	// A broadcast goes to every connection but the one given
	uint32_t fanout = 1;
	if (broadcast) {
		const uint32_t connections = mPeer ? mPeer->NumberOfConnections() : 0;
		fanout = sysAddr == UNASSIGNED_SYSTEM_ADDRESS ? connections : (connections > 0 ? connections - 1 : 0);
	}
	const auto bytes = bitStream.GetNumberOfBytesUsed();
	TrafficStats::Local().Packet(TrafficStats::Now(), TrafficStats::KeyOf(bitStream.GetData(), bytes, true), bytes, fanout);
}

void dServer::AddLinkStats(RakPeerInterface* peer, uint64_t peerIndex, ServerTraffic& report, uint64_t& pingSum, std::map<uint64_t, LinkCounters>& seen) {
	if (!peer) return;
	std::vector<SystemAddress> addresses(std::max<unsigned short>(peer->GetMaximumNumberOfPeers(), 1));
	unsigned short count = static_cast<unsigned short>(addresses.size());
	if (!peer->GetConnectionList(addresses.data(), &count)) return;
	auto& link = report.report.link;
	for (unsigned short i = 0; i < count; i++) {
		auto* stats = peer->GetStatistics(addresses[i]);
		if (!stats) continue;
		LinkCounters now{ stats->packetsSent, stats->packetsReceived, stats->totalBitsSent, stats->bitsReceived, stats->messageResends };
		const uint64_t key = (peerIndex << 48) | (static_cast<uint64_t>(addresses[i].binaryAddress) << 16) | addresses[i].port;
		const auto it = mLinkCounters.find(key);
		const LinkCounters before = it != mLinkCounters.end() ? it->second : LinkCounters{};
		// A reused address is a new connection whose totals started again
		const auto delta = [](uint64_t current, uint64_t previous) { return current >= previous ? current - previous : current; };
		link.datagramsSent += delta(now.datagramsSent, before.datagramsSent);
		link.datagramsReceived += delta(now.datagramsReceived, before.datagramsReceived);
		link.bytesSent += delta(now.bitsSent, before.bitsSent) / 8;
		link.bytesReceived += delta(now.bitsReceived, before.bitsReceived) / 8;
		link.resends += delta(now.resends, before.resends);
		link.resendQueue += stats->messagesOnResendQueue;
		link.connections++;
		pingSum += std::max(0, peer->GetAveragePing(addresses[i]));
		seen[key] = now;
	}
}

void dServer::ReportTraffic() {
	ServerTraffic report;
	report.serverType = mServerType;
	report.zoneId = mZoneID;
	report.instanceId = static_cast<uint32_t>(mInstanceID);
	report.report = TrafficStats::Local().Take(TrafficStats::Now());

	uint64_t pingSum = 0;
	std::map<uint64_t, LinkCounters> seen;
	AddLinkStats(mPeer, 0, report, pingSum, seen);
	AddLinkStats(mMasterPeer, 1, report, pingSum, seen);
	mLinkCounters = std::move(seen);
	if (report.report.link.connections) report.report.link.averagePingMs = static_cast<uint32_t>(pingSum / report.report.link.connections);

	if (mTrafficSink) mTrafficSink(report);
	else if (mMasterPeer && mMasterConnectionActive) MasterPackets::SendToMaster(report, this);
}
