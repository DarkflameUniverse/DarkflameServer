#define _VARIADIC_MAX 10
#include "dServer.h"
#include "dNetCommon.h"
#include "Logger.h"
#include "dConfig.h"

#include "RakNetworkFactory.h"
#include "MessageIdentifiers.h"
#include "eConnectionType.h"
#include "eServerMessageType.h"
#include "eMasterMessageType.h"

#include "BitStreamUtils.h"
#include "MasterPackets.h"
#include "ZoneInstanceManager.h"
#include "StringifiedEnum.h"

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

dServer::dServer(const std::string& ip, int port, int instanceID, int maxConnections, bool isInternal, bool useEncryption, Logger* logger, const std::string masterIP, int masterPort, ServerType serverType, dConfig* config, Game::signal_t* lastSignal, unsigned int zoneID) {
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
	mShouldShutdown = lastSignal;
	//Attempt to start our server here:
	mIsOkay = Startup();

	//Forcibly log to both the console and our file what ip, port and possibly zoneID / instanceID we're running on:
	bool prevLogSetting = mLogger->GetLogToConsole();
	mLogger->SetLogToConsole(true);

	if (mIsOkay) {
		if (zoneID == 0)
			LOG("%s Server is listening on %s:%i with encryption: %i", StringifiedEnum::ToString(serverType).data(), ip.c_str(), port, int(useEncryption));
		else
			LOG("%s Server is listening on %s:%i with encryption: %i, running zone %i / %i", StringifiedEnum::ToString(serverType).data(), ip.c_str(), port, int(useEncryption), zoneID, instanceID);
	} else { LOG("FAILED TO START SERVER ON IP/PORT: %s:%i", ip.c_str(), port); return; }

	mLogger->SetLogToConsole(prevLogSetting);

	//Connect to master if we are not master:
	if (serverType != ServerType::Master) {
		SetupForMasterConnection();
		if (!ConnectToMaster()) {
			LOG("Failed ConnectToMaster!");
		}
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
			LOG("Lost our connection to master, shutting DOWN!");
			mMasterConnectionActive = false;
			//ConnectToMaster(); //We'll just shut down now
		}

		if (packet->data[0] == ID_CONNECTION_REQUEST_ACCEPTED) {
			LOG("Established connection to master, zone (%i), instance (%i)", this->GetZoneID(), this->GetInstanceID());
			mMasterConnectionActive = true;
			mMasterSystemAddress = packet->systemAddress;
			MasterPackets::SendServerInfo(this, packet);
		}

		if (packet->data[0] == ID_USER_PACKET_ENUM) {
			if (static_cast<eConnectionType>(packet->data[1]) == eConnectionType::MASTER) {
				switch (static_cast<eMasterMessageType>(packet->data[3])) {
				case eMasterMessageType::REQUEST_ZONE_TRANSFER_RESPONSE: {
					ZoneInstanceManager::Instance()->HandleRequestZoneTransferResponse(packet);
					break;
				}
				case eMasterMessageType::SHUTDOWN:
					*mShouldShutdown = -2;
					break;

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

void dServer::Send(RakNet::BitStream& bitStream, const SystemAddress& sysAddr, bool broadcast) {
	mPeer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE_ORDERED, 0, sysAddr, broadcast);
}

void dServer::SendToMaster(RakNet::BitStream& bitStream) {
	if (!mMasterConnectionActive) ConnectToMaster();
	mMasterPeer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE_ORDERED, 0, mMasterSystemAddress, false);
}

void dServer::Disconnect(const SystemAddress& sysAddr, eServerDisconnectIdentifiers disconNotifyID) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::SERVER, eServerMessageType::DISCONNECT_NOTIFY);
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
		UpdateBandwidthLimit();
		UpdateMaximumMtuSize();
		mPeer->SetIncomingPassword("3.25 ND1", 8);
	}

	mPeer->SetMaximumIncomingConnections(mMaxConnections);
	if (mUseEncryption) mPeer->InitializeSecurity(NULL, NULL, NULL, NULL);

	return true;
}

void dServer::UpdateMaximumMtuSize() {
	auto maxMtuSize = mConfig->GetValue("maximum_mtu_size");
	mPeer->SetMTUSize(maxMtuSize.empty() ? 1228 : std::stoi(maxMtuSize));
}

void dServer::UpdateBandwidthLimit() {
	auto newBandwidth = mConfig->GetValue("maximum_outgoing_bandwidth");
	mPeer->SetPerConnectionOutgoingBandwidthLimit(!newBandwidth.empty() ? std::stoi(newBandwidth) : 0);
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

	if (mServerType != ServerType::Master && mMasterPeer) {
		mMasterPeer->Shutdown(1000);
		RakNetworkFactory::DestroyRakPeerInterface(mMasterPeer);
	}
}

void dServer::SetupForMasterConnection() {
	mMasterSocketDescriptor = SocketDescriptor(uint16_t(mPort + 1), 0);
	mMasterPeer = RakNetworkFactory::GetRakPeerInterface();
	bool ret = mMasterPeer->Startup(1, 30, &mMasterSocketDescriptor, 1);
	if (!ret) LOG("Failed MasterPeer Startup!");
}

bool dServer::ConnectToMaster() {
	//LOG("Connection to Master %s:%d", mMasterIP.c_str(), mMasterPort);
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
