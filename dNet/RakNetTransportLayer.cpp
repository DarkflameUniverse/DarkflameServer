#define _VARIADIC_MAX 10
#include "RakNetTransportLayer.h"
#include "dNetCommon.h"
#include "Logger.h"
#include "dConfig.h"

#include "RakNetworkFactory.h"
#include "MessageIdentifiers.h"
#include "eConnectionType.h"
#include "eServerMessageType.h"
#include "eManagerMessageType.h"

#include "BitStreamUtils.h"
#include "MasterPackets.h"
#include "ZoneInstanceManager.h"
#include "StringifiedEnum.h"
#include "RakNetPeer.h"

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

RakNetTransportLayer::RakNetTransportLayer(const std::string& ip, int port, int instanceID, int maxConnections, bool isInternal, bool useEncryption, Logger* logger, const std::string ManagerIP, int ManagerPort, ServerType serverType, dConfig* config, Game::signal_t* lastSignal, unsigned int zoneID) {
	mIP = ip;
	mPort = port;
	mZoneID = zoneID;
	mInstanceID = instanceID;
	mMaxConnections = maxConnections;
	mIsInternal = isInternal;
	mUseEncryption = useEncryption;
	mLogger = logger;
	mManagerIP = ManagerIP;
	mManagerPort = ManagerPort;
	mManagerConnectionActive = false;
	mNetIDManager = nullptr;
	mReplicaManager = nullptr;
	mServerType = serverType;
	mConfig = config;
	mShouldShutdown = lastSignal;
	mIsOkay = false;
}

RakNetTransportLayer::~RakNetTransportLayer() {
	Shutdown();
}

Packet* RakNetTransportLayer::ReceiveFromManager() {
	if (!mManagerPeer) return nullptr;
	if (!mManagerConnectionActive) ConnectToManager();

	Packet* packet = mManagerPeer->Receive();
	if (packet) {
		if (packet->length < 1) { mManagerPeer->DeallocatePacket(packet); return nullptr; }

		if (packet->data[0] == ID_DISCONNECTION_NOTIFICATION || packet->data[0] == ID_CONNECTION_LOST) {
			LOG("Lost our connection to Manager, shutting DOWN!");
			mManagerConnectionActive = false;
			//ConnectToManager(); //We'll just shut down now
		}

		if (packet->data[0] == ID_CONNECTION_REQUEST_ACCEPTED) {
			LOG("Established connection to Manager, zone (%i), instance (%i)", this->GetZoneID(), this->GetInstanceID());
			mManagerConnectionActive = true;
			mManagerSystemAddress = packet->systemAddress;
			MasterPackets::SendServerInfo(this, packet);
		}

		if (packet->data[0] == ID_USER_PACKET_ENUM) {
			if (static_cast<eConnectionType>(packet->data[1]) == eConnectionType::MASTER) {
				switch (static_cast<eManagerMessageType>(packet->data[3])) {
				case eManagerMessageType::REQUEST_ZONE_TRANSFER_RESPONSE: {
					ZoneInstanceManager::Instance()->HandleRequestZoneTransferResponse(packet);
					break;
				}
				case eManagerMessageType::SHUTDOWN:
					*mShouldShutdown = -2;
					break;

				//When we handle these packets in World instead RakNetTransportLayer, we just return the packet's pointer.
				default:

					return packet;
				}
			}
		}

		mManagerPeer->DeallocatePacket(packet);
	}

	return nullptr;
}

Packet* RakNetTransportLayer::Receive() {
	return mPeer->Receive();
}

void RakNetTransportLayer::DeallocatePacket(Packet* packet) {
	mPeer->DeallocatePacket(packet);
}

void RakNetTransportLayer::DeallocateManagerPacket(Packet* packet) {
	mManagerPeer->DeallocatePacket(packet);
}

void RakNetTransportLayer::Send(RakNet::BitStream& bitStream, const SystemAddress& sysAddr, bool broadcast) {
	mPeer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE_ORDERED, 0, sysAddr, broadcast);
}

void RakNetTransportLayer::SendToManager(RakNet::BitStream& bitStream) {
	if (!mManagerConnectionActive) ConnectToManager();
	mManagerPeer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE_ORDERED, 0, mManagerSystemAddress, false);
}

void RakNetTransportLayer::Disconnect(const SystemAddress& sysAddr, eServerDisconnectIdentifiers disconNotifyID) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::SERVER, eServerMessageType::DISCONNECT_NOTIFY);
	bitStream.Write(disconNotifyID);
	mPeer->Send(&bitStream, SYSTEM_PRIORITY, RELIABLE_ORDERED, 0, sysAddr, false);

	mPeer->CloseConnection(sysAddr, true);
}

bool RakNetTransportLayer::IsConnected(const SystemAddress& sysAddr) {
	return mPeer->IsConnected(sysAddr);
}

bool RakNetTransportLayer::Startup() {
	mIsOkay = StartPeer();

	//Forcibly log to both the console and our file what ip, port and possibly zoneID / instanceID we're running on:
	bool prevLogSetting = mLogger->GetLogToConsole();
	mLogger->SetLogToConsole(true);

	if (mIsOkay) {
		if (mZoneID == 0)
			LOG("%s Server is listening on %s:%i with encryption: %i", StringifiedEnum::ToString(mServerType).data(), mIP.c_str(), mPort, int(mUseEncryption));
		else
			LOG("%s Server is listening on %s:%i with encryption: %i, running zone %i / %i", StringifiedEnum::ToString(mServerType).data(), mIP.c_str(), mPort, int(mUseEncryption), mZoneID, mInstanceID);
	} else { LOG("FAILED TO START SERVER ON IP/PORT: %s:%i", mIP.c_str(), mPort); return false; }

	mLogger->SetLogToConsole(prevLogSetting);

	//Connect to Manager if we are not Manager:
	if (mServerType != ServerType::Manager) {
		SetupForManagerConnection();
		if (!ConnectToManager()) {
			LOG("Failed ConnectToManager!");
		}
	}

	//Set up Replica if we're a world server:
	if (mServerType == ServerType::World) {
		/*
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
		*/
	}

	return mIsOkay;
}

void RakNetTransportLayer::UpdateMaximumMtuSize() {
	auto maxMtuSize = mConfig->GetValue("maximum_mtu_size");
	mPeer->SetMTUSize(maxMtuSize.empty() ? 1228 : std::stoi(maxMtuSize));
}

void RakNetTransportLayer::UpdateBandwidthLimit() {
	auto newBandwidth = mConfig->GetValue("maximum_outgoing_bandwidth");
	mPeer->SetPerConnectionOutgoingBandwidthLimit(!newBandwidth.empty() ? std::stoi(newBandwidth) : 0);
}

void RakNetTransportLayer::Shutdown() {
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

	if (mServerType != ServerType::Manager && mManagerPeer) {
		mManagerPeer->Shutdown(1000);
		RakNetworkFactory::DestroyRakPeerInterface(mManagerPeer);
	}
}

bool RakNetTransportLayer::StartPeer() {
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

void RakNetTransportLayer::SetupForManagerConnection() {
	mManagerSocketDescriptor = SocketDescriptor(uint16_t(mPort + 1), 0);
	mManagerPeer = RakNetworkFactory::GetRakPeerInterface();
	bool ret = mManagerPeer->Startup(1, 30, &mManagerSocketDescriptor, 1);
	if (!ret) LOG("Failed ManagerPeer Startup!");
}

bool RakNetTransportLayer::ConnectToManager() {
	//LOG("Connection to Manager %s:%d", mManagerIP.c_str(), mManagerPort);
	return mManagerPeer->Connect(mManagerIP.c_str(), mManagerPort, "3.25 DARKFLAME1", 15);
}

int RakNetTransportLayer::GetPing(const SystemAddress& sysAddr) const {
	return mPeer->GetAveragePing(sysAddr);
}

int RakNetTransportLayer::GetLatestPing(const SystemAddress& sysAddr) const {
	return mPeer->GetLastPing(sysAddr);
}

TransportPeerInterface* RakNetTransportLayer::CreateOutgoingTransport(uint32_t peerPort, const std::string& ip, uint32_t port, const std::string& password) const {
	auto sock = SocketDescriptor(static_cast<uint16_t>(peerPort), 0);
	auto* peer = RakNetworkFactory::GetRakPeerInterface();
	peer->Startup(1, 30, &sock, 1);
	peer->Connect(ip.c_str(), port, "3.25 ND1", 8);

	return new RakNetPeer(peer, ip, port, password);
}