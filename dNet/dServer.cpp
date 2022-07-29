#define _VARIADIC_MAX 10
#include "dServer.h"
#include "dNetCommon.h"
#include "dLogger.h"

#include "RakNetworkFactory.h"
#include "MessageIdentifiers.h"

#include "PacketUtils.h"
#include "dMessageIdentifiers.h"
#include "MasterPackets.h"
#include "ZoneInstanceManager.h"

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

dServer::dServer(const std::string& ip, int port, int instanceID, int maxConnections, bool isInternal, bool useEncryption, dLogger* logger, const std::string masterIP, int masterPort, ServerType serverType, unsigned int zoneID) {
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

	//Attempt to start our server here:
	mIsOkay = Startup();

	//Forcibly log to both the console and our file what ip, port and possibly zoneID / instanceID we're running on:
	bool prevLogSetting = mLogger->GetIsLoggingToConsole();
	mLogger->SetLogToConsole(true);

	if (mIsOkay) {
		if (zoneID == 0)
			mLogger->Log("dServer", "Server is listening on %s:%i with encryption: %i", ip.c_str(), port, int(useEncryption));
		else
			mLogger->Log("dServer", "Server is listening on %s:%i with encryption: %i, running zone %i / %i", ip.c_str(), port, int(useEncryption), zoneID, instanceID);
	} else { mLogger->Log("dServer", "FAILED TO START SERVER ON IP/PORT: %s:%i", ip.c_str(), port); return; }

	mLogger->SetLogToConsole(prevLogSetting);

	//Connect to master if we are not master:
	if (serverType != ServerType::Master) {
		SetupForMasterConnection();
		ConnectToMaster();
	}

	//Set up Replica if we're a world server:
	if (serverType == ServerType::World) {
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
	if (packet) {
		if (packet->length < 1) { mMasterPeer->DeallocatePacket(packet); return nullptr; }

		if (packet->data[0] == ID_DISCONNECTION_NOTIFICATION || packet->data[0] == ID_CONNECTION_LOST) {
			mLogger->Log("dServer", "Lost our connection to master, shutting DOWN!");
			mMasterConnectionActive = false;
			//ConnectToMaster(); //We'll just shut down now
		}

		if (packet->data[0] == ID_CONNECTION_REQUEST_ACCEPTED) {
			mLogger->Log("dServer", "Established connection to master, zone (%i), instance (%i)", this->GetZoneID(), this->GetInstanceID());
			mMasterConnectionActive = true;
			mMasterSystemAddress = packet->systemAddress;
			MasterPackets::SendServerInfo(this, packet);
		}

		if (packet->data[0] == ID_USER_PACKET_ENUM) {
			if (packet->data[1] == MASTER) {
				switch (packet->data[3]) {
				case MSG_MASTER_REQUEST_ZONE_TRANSFER_RESPONSE: {
					uint64_t requestID = PacketUtils::ReadPacketU64(8, packet);
					ZoneInstanceManager::Instance()->HandleRequestZoneTransferResponse(requestID, packet);
					break;
				}

															  //When we handle these packets in World instead dServer, we just return the packet's pointer.
				default:

					return packet;
				}
			}
		}

		mMasterPeer->DeallocatePacket(packet);
	}

	return nullptr;
}

Packet* dServer::Receive() {
	return mPeer->Receive();
}

void dServer::DeallocatePacket(Packet* packet) {
	mPeer->DeallocatePacket(packet);
}

void dServer::DeallocateMasterPacket(Packet* packet) {
	mMasterPeer->DeallocatePacket(packet);
}

void dServer::Send(RakNet::BitStream* bitStream, const SystemAddress& sysAddr, bool broadcast) {
	mPeer->Send(bitStream, SYSTEM_PRIORITY, RELIABLE_ORDERED, 0, sysAddr, broadcast);
}

void dServer::SendToMaster(RakNet::BitStream* bitStream) {
	if (!mMasterConnectionActive) ConnectToMaster();
	mMasterPeer->Send(bitStream, SYSTEM_PRIORITY, RELIABLE_ORDERED, 0, mMasterSystemAddress, false);
}

void dServer::Disconnect(const SystemAddress& sysAddr, uint32_t disconNotifyID) {
	RakNet::BitStream bitStream;
	PacketUtils::WriteHeader(bitStream, SERVER, MSG_SERVER_DISCONNECT_NOTIFY);
	bitStream.Write(disconNotifyID);
	mPeer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE_ORDERED, 0, sysAddr, false);

	mPeer->CloseConnection(sysAddr, true);
}

bool dServer::IsConnected(const SystemAddress& sysAddr) {
	return mPeer->IsConnected(sysAddr);
}

bool dServer::Startup() {
	mSocketDescriptor = SocketDescriptor(uint16_t(mPort), 0);
	mPeer = RakNetworkFactory::GetRakPeerInterface();

	if (!mPeer) return false;
	if (!mPeer->Startup(mMaxConnections, 10, &mSocketDescriptor, 1)) return false;

	if (mIsInternal) {
		mPeer->SetIncomingPassword("3.25 DARKFLAME1", 15);
	} else {
		//mPeer->SetPerConnectionOutgoingBandwidthLimit(800000); //100Kb/s
		mPeer->SetIncomingPassword("3.25 ND1", 8);
	}

	mPeer->SetMaximumIncomingConnections(mMaxConnections);
	if (mUseEncryption) mPeer->InitializeSecurity(NULL, NULL, NULL, NULL);

	return true;
}

void dServer::Shutdown() {
	mPeer->Shutdown(1000);

	if (mNetIDManager) {
		delete mNetIDManager;
		mNetIDManager = nullptr;
	}

	if (mReplicaManager) {
		delete mReplicaManager;
		mReplicaManager = nullptr;
	}

	//RakNetworkFactory::DestroyRakPeerInterface(mPeer); //Not needed, we already called Shutdown ourselves.
	if (mServerType != ServerType::Master) {
		mMasterPeer->Shutdown(1000);
		//RakNetworkFactory::DestroyRakPeerInterface(mMasterPeer);
	}
}

void dServer::SetupForMasterConnection() {
	mMasterSocketDescriptor = SocketDescriptor(uint16_t(mPort + 1), 0);
	mMasterPeer = RakNetworkFactory::GetRakPeerInterface();
	mMasterPeer->Startup(1, 30, &mMasterSocketDescriptor, 1);
}

bool dServer::ConnectToMaster() {
	return mMasterPeer->Connect(mMasterIP.c_str(), mMasterPort, "3.25 DARKFLAME1", 15);
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
