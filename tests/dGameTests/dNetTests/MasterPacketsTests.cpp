#include "MasterPackets.h"
#include "master/CDClientReload.h"
#include "master/DashboardMessages.h"
#include "master/DataChanged.h"
#include "master/InstanceMigration.h"
#include "master/MessageCapture.h"
#include "master/PlayerAction.h"
#include "PacketDispatcher.h"
#include "PacketTestUtils.h"
#include "Game.h"
#include "Logger.h"
#include "dGameMessagesTests/GameMessageTestUtils.h"
#include "Legacy/MasterPacketsLegacy.h"

#include <array>
#include <functional>
#include <limits>

#include <gtest/gtest.h>

using namespace PacketTestUtils;
using GameMessageTestUtils::RoundTrip;
using GameMessageTestUtils::ExpectTruncatedFails;

namespace {
	const std::array<std::string, 4> g_Strings = { "", "localhost", "a password that is longer than fifty characters, which master cuts", "10.0.0.255" };
	const std::array<uint64_t, 3> g_RequestIds = { 0, 1, 0x0102030405060708ULL };
	const std::array<uint32_t, 3> g_Numbers = { 0, 1100, 0xFFFFFFFF };

	PacketBytes StructPacket(const LUBitStream& msg) {
		RakNet::BitStream bitStream;
		msg.WritePacket(bitStream);
		return FromBitStream(bitStream);
	}

	PacketBytes Written(const std::function<void(RakNet::BitStream&)>& write) {
		RakNet::BitStream bitStream;
		write(bitStream);
		return FromBitStream(bitStream);
	}

	// A stream positioned after the header of msg's packet
	void LoadPayload(RakNet::BitStream& bitStream, const LUBitStream& msg) {
		msg.WritePacket(bitStream);
		bitStream.IgnoreBytes(8);
	}

	// The old way the dashboard and migration structs were sent: the header, then Serialize
	template<typename T>
	void ExpectHeaderThenSerialize(const T& msg, MessageType::Master id) {
		RakNet::BitStream old;
		LUBitStream(ServiceType::MASTER, id).WriteHeader(old);
		msg.Serialize(old);
		EXPECT_PACKET_EQ(FromBitStream(old), StructPacket(msg));
		RoundTrip(msg);
	}
}

TEST(MasterPacketsTests, ZoneTransferMatchesLegacy) {
	Stamps someStamps;
	someStamps.list.emplace_back(eStamps::PASSPORT_AUTH_START, 0, 1700000000);
	someStamps.list.emplace_back(eStamps::PASSPORT_AUTH_WORLD_PACKET_RECEIVED, 1100, 1700000001);
	for (const auto& stamps : { Stamps{}, someStamps })
	for (const auto requestID : g_RequestIds) {
		for (const bool mythranShift : { false, true }) {
			for (const auto zone : g_Numbers) {
				MasterPackets::RequestZoneTransfer request;
				request.requestID = requestID;
				request.mythranShift = mythranShift;
				request.zoneID = zone;
				request.cloneID = zone ^ 0x5555;
				request.stamps = stamps;
				EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyMaster::SendZoneTransferRequest(b, requestID, mythranShift, zone, zone ^ 0x5555, stamps); }), StructPacket(request));

				RakNet::BitStream stream; LoadPayload(stream, request);
				const auto legacy = LegacyMaster::ReadZoneTransferRequest(stream);
				const auto copy = RoundTrip(request);
				EXPECT_EQ(legacy.requestID, copy.requestID);
				EXPECT_EQ(legacy.mythranShift, copy.mythranShift);
				EXPECT_EQ(legacy.zoneID, copy.zoneID);
				EXPECT_EQ(legacy.zoneClone, copy.cloneID);
				EXPECT_EQ(legacy.stamps.size(), copy.stamps.size());

				for (const auto& ip : g_Strings) {
					for (const uint32_t port : { 0u, 2001u, 70000u }) {
						MasterPackets::RequestZoneTransferResponse response;
						response.requestID = requestID;
						response.mythranShift = mythranShift;
						response.zoneID = zone;
						response.zoneInstance = zone / 2;
						response.zoneClone = zone / 3;
						response.serverPort = static_cast<uint16_t>(port);
						response.serverIP = LUString(ip, 255);
						response.stamps = stamps;
						EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyMaster::SendZoneTransferResponse(b, requestID, mythranShift, zone, zone / 2, zone / 3, ip, port, stamps); }), StructPacket(response));

						RakNet::BitStream responseStream; LoadPayload(responseStream, response);
						const auto legacyResponse = LegacyMaster::ReadZoneTransferResponse(responseStream);
						const auto responseCopy = RoundTrip(response);
						EXPECT_EQ(legacyResponse.requestID, responseCopy.requestID);
						EXPECT_EQ(legacyResponse.mythranShift, responseCopy.mythranShift > 0);
						EXPECT_EQ(legacyResponse.zoneInstance, responseCopy.zoneInstance);
						EXPECT_EQ(legacyResponse.serverPort, responseCopy.serverPort);
						EXPECT_EQ(legacyResponse.serverIP, responseCopy.serverIP.string);
						EXPECT_EQ(legacyResponse.stamps.size(), responseCopy.stamps.size());
					}
				}
			}
		}
	}
}

// Senders without the login stamps (older servers) still read, with no stamps; a cut off fixed part does not
TEST(MasterPacketsTests, ZoneTransferStampsAreOptional) {
	RakNet::BitStream noStamps;
	noStamps.Write<uint64_t>(5);
	noStamps.Write<uint8_t>(1);
	noStamps.Write<uint32_t>(1100);
	noStamps.Write<uint32_t>(0);
	MasterPackets::RequestZoneTransfer request;
	ASSERT_TRUE(request.Deserialize(noStamps));
	EXPECT_EQ(request.requestID, 5u);
	EXPECT_TRUE(request.stamps.empty());

	MasterPackets::RequestZoneTransfer full;
	full.requestID = 5;
	RakNet::BitStream all;
	full.Serialize(all);
	for (uint32_t bits = 0; bits < 8 * (8 + 1 + 4 + 4); bits++) {
		RakNet::BitStream prefix;
		prefix.WriteBits(all.GetData(), bits, false);
		MasterPackets::RequestZoneTransfer copy;
		EXPECT_FALSE(copy.Deserialize(prefix)) << bits;
	}
}

TEST(MasterPacketsTests, ServerInfoMatchesLegacy) {
	for (const auto type : { ServiceType::AUTH, ServiceType::CHAT, ServiceType::WORLD, ServiceType::DASHBOARD }) {
		for (const auto& ip : g_Strings) {
			if (ip.size() > 33) continue; // LUString(33)
			for (const int instance : { 0, 7, -1 }) {
				MasterPackets::ServerInfo info;
				info.port = 2001;
				info.zoneID = 1100;
				info.instanceID = static_cast<uint32_t>(instance);
				info.serverType = type;
				info.ip = LUString(ip);
				EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyMaster::SendServerInfo(b, 2001, 1100u, instance, type, ip); }), StructPacket(info));

				RakNet::BitStream stream; LoadPayload(stream, info);
				const auto legacy = LegacyMaster::ReadServerInfo(stream);
				const auto copy = RoundTrip(info);
				EXPECT_EQ(legacy.theirPort, copy.port);
				EXPECT_EQ(legacy.theirInstanceID, copy.instanceID);
				EXPECT_EQ(legacy.theirServerType, copy.serverType);
				EXPECT_EQ(legacy.theirIP, copy.ip.string);
				ExpectTruncatedFails(info);
			}
		}
		MasterPackets::ServerInfo offline;
		offline.serverType = type;
		offline.ip = LUString("offline");
		EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyMaster::WriteServerOffline(b, type); }), StructPacket(offline));
	}
}

TEST(MasterPacketsTests, SessionKeysMatchLegacy) {
	for (const uint32_t key : { 0u, 12345u, 0xFFFFFFFFu }) {
		for (const auto& name : g_Strings) {
			if (name.size() > 33) continue;
			MasterPackets::SetSessionKey set;
			set.sessionKey = key;
			set.username = LUString(name);
			EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyMaster::WriteSessionKey(b, MessageType::Master::SET_SESSION_KEY, key, LUString(name)); }), StructPacket(set));
			RoundTrip(set);
			ExpectTruncatedFails(set);

			MasterPackets::NewSessionAlert alert;
			alert.sessionKey = key;
			alert.username = LUString(name);
			EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyMaster::WriteSessionKey(b, MessageType::Master::NEW_SESSION_ALERT, key, LUString(name)); }), StructPacket(alert));
			RoundTrip(alert);

			MasterPackets::SessionKeyResponse response;
			response.sessionKey = key;
			response.username = LUWString(name);
			EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyMaster::WriteSessionKeyResponse(b, key, LUWString(name)); }), StructPacket(response));
			RoundTrip(response);
			ExpectTruncatedFails(response);

			MasterPackets::RequestSessionKey request;
			request.username = LUWString(name);
			EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyMaster::WriteRequestSessionKey(b, LUWString(name)); }), StructPacket(request));
			RoundTrip(request);
			ExpectTruncatedFails(request);
		}
	}
}

TEST(MasterPacketsTests, PrivateZonesMatchLegacy) {
	for (const auto& password : g_Strings) {
		MasterPackets::CreatePrivateZone create;
		create.zoneID = 1150;
		create.cloneID = 0xAABBCCDD;
		create.password = password;
		EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyMaster::SendZoneCreatePrivate(b, 1150, 0xAABBCCDD, password); }), StructPacket(create));

		RakNet::BitStream stream; LoadPayload(stream, create);
		const auto legacy = LegacyMaster::ReadCreatePrivateZone(stream);
		RakNet::BitStream stream2; LoadPayload(stream2, create);
		MasterPackets::CreatePrivateZone read;
		ASSERT_TRUE(read.Deserialize(stream2));
		EXPECT_EQ(legacy.first.first, read.zoneID);
		EXPECT_EQ(legacy.first.second, read.cloneID);
		EXPECT_EQ(legacy.second, read.password); // both cut at 50 characters
		if (password.size() <= 50) RoundTrip(create);

		for (const bool mythranShift : { false, true }) {
			MasterPackets::RequestPrivateZone request;
			request.requestID = 0x0102030405060708ULL;
			request.mythranShift = mythranShift;
			request.password = password;
			EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyMaster::SendZoneRequestPrivate(b, 0x0102030405060708ULL, mythranShift, password); }), StructPacket(request));

			RakNet::BitStream requestStream; LoadPayload(requestStream, request);
			const auto legacyRequest = LegacyMaster::ReadRequestPrivateZone(requestStream);
			RakNet::BitStream requestStream2; LoadPayload(requestStream2, request);
			MasterPackets::RequestPrivateZone readRequest;
			ASSERT_TRUE(readRequest.Deserialize(requestStream2));
			EXPECT_EQ(legacyRequest.first.first, readRequest.requestID);
			EXPECT_EQ(legacyRequest.first.second, readRequest.mythranShift);
			EXPECT_EQ(legacyRequest.second, readRequest.password);
			if (password.size() <= 50) RoundTrip(request);
		}
	}
}

TEST(MasterPacketsTests, WorldStateMatchesLegacy) {
	for (const LWOMAPID zone : { static_cast<LWOMAPID>(0), static_cast<LWOMAPID>(1100), static_cast<LWOMAPID>(0xFFFF) }) {
		for (const LWOINSTANCEID instance : { static_cast<LWOINSTANCEID>(0), static_cast<LWOINSTANCEID>(3), static_cast<LWOINSTANCEID>(0xFFFF) }) {
			MasterPackets::WorldReady ready;
			ready.zoneID = zone;
			ready.instanceID = instance;
			EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyMaster::SendWorldReady(b, zone, instance); }), StructPacket(ready));
			RoundTrip(ready);
			ExpectTruncatedFails(ready);

			MasterPackets::PlayerAdded added;
			added.zoneID = zone;
			added.instanceID = instance;
			EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyMaster::WritePlayerCount(b, MessageType::Master::PLAYER_ADDED, zone, instance); }), StructPacket(added));
			RoundTrip(added);
			ExpectTruncatedFails(added);

			MasterPackets::PlayerRemoved removed;
			removed.zoneID = zone;
			removed.instanceID = instance;
			EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyMaster::WritePlayerCount(b, MessageType::Master::PLAYER_REMOVED, zone, instance); }), StructPacket(removed));
			RoundTrip(removed);

			MasterPackets::WorldShutDown shutDown;
			shutDown.zoneID = zone;
			shutDown.instanceID = instance;
			EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyMaster::WriteWorldShutDown(b, zone, instance); }), StructPacket(shutDown));
			RoundTrip(shutDown);

			for (const bool isPrivate : { false, true }) {
				MasterPackets::WorldReadyInfo info;
				info.zoneID = zone;
				info.instanceID = instance;
				info.cloneID = 0xDEADBEEF;
				info.ip = LUString("127.0.0.1");
				info.port = 3000;
				info.isPrivate = isPrivate ? 1 : 0;
				EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyMaster::WriteWorldReadyInfo(b, zone, instance, 0xDEADBEEF, "127.0.0.1", 3000, isPrivate); }), StructPacket(info));
				RakNet::BitStream stream; LoadPayload(stream, info);
				const auto legacy = LegacyMaster::ReadWorldReadyInfo(stream);
				const auto copy = RoundTrip(info);
				EXPECT_EQ(legacy.cloneID, copy.cloneID);
				EXPECT_EQ(legacy.ip, copy.ip.string);
				EXPECT_EQ(legacy.isPrivate, copy.isPrivate != 0);
				ExpectTruncatedFails(info);
			}
		}
	}

	for (const int zone : { 0, 1100, -1 }) {
		MasterPackets::PrepZone prep;
		prep.zoneID = zone;
		EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyMaster::TellMasterToPrepZone(b, zone); }), StructPacket(prep));
		RoundTrip(prep);
		ExpectTruncatedFails(prep);
	}
	{
		// A property's clone rides after the zone; a prep without one reads as clone 0
		MasterPackets::PrepZone prep;
		prep.zoneID = 1251;
		prep.cloneID = 2290;
		RakNet::BitStream stream;
		prep.Serialize(stream);
		MasterPackets::PrepZone read;
		ASSERT_TRUE(read.Deserialize(stream));
		EXPECT_EQ(read.zoneID, 1251);
		EXPECT_EQ(read.cloneID, 2290u);
		RakNet::BitStream plain;
		plain.Write<int32_t>(1100);
		ASSERT_TRUE(read.Deserialize(plain));
		EXPECT_EQ(read.cloneID, 0u);
	}

	for (const auto requestID : g_RequestIds) {
		MasterPackets::AffirmTransferRequest request;
		request.requestID = requestID;
		EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyMaster::WriteAffirmTransfer(b, MessageType::Master::AFFIRM_TRANSFER_REQUEST, requestID); }), StructPacket(request));
		RoundTrip(request);
		ExpectTruncatedFails(request);
		MasterPackets::AffirmTransferResponse response;
		response.requestID = requestID;
		EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyMaster::WriteAffirmTransfer(b, MessageType::Master::AFFIRM_TRANSFER_RESPONSE, requestID); }), StructPacket(response));
		RoundTrip(response);
	}

	for (const auto zone : g_Numbers) {
		MasterPackets::InstanceShutdown shutdown;
		shutdown.zoneID = zone;
		shutdown.instanceID = zone / 7;
		EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) { LegacyMaster::WriteInstanceShutdown(b, zone, zone / 7); }), StructPacket(shutdown));
		RoundTrip(shutdown);
		ExpectTruncatedFails(shutdown);
	}
}

TEST(MasterPacketsTests, EmptyMessagesMatchLegacy) {
	EXPECT_PACKET_EQ(Written([](RakNet::BitStream& b) { LegacyMaster::WriteEmpty(b, MessageType::Master::SHUTDOWN); }), StructPacket(MasterPackets::Shutdown()));
	EXPECT_PACKET_EQ(Written([](RakNet::BitStream& b) { LegacyMaster::WriteEmpty(b, MessageType::Master::SHUTDOWN_RESPONSE); }), StructPacket(MasterPackets::ShutdownResponse()));
	EXPECT_PACKET_EQ(Written([](RakNet::BitStream& b) { LegacyMaster::WriteEmpty(b, MessageType::Master::SHUTDOWN_UNIVERSE); }), StructPacket(MasterPackets::ShutdownUniverse()));
	EXPECT_PACKET_EQ(Written([](RakNet::BitStream& b) { LegacyMaster::WriteEmpty(b, MessageType::Master::REQUEST_SERVER_LIST); }), StructPacket(MasterPackets::RequestServerList()));
	EXPECT_PACKET_EQ(Written([](RakNet::BitStream& b) { LegacyMaster::WriteEmpty(b, MessageType::Master::CONFIG_RELOAD); }), StructPacket(MasterPackets::ConfigReload()));
	EXPECT_PACKET_EQ(Written([](RakNet::BitStream& b) { LegacyMaster::WriteEmpty(b, MessageType::Master::DASHBOARD_SHUTDOWN); }), StructPacket(MasterPackets::DashboardShutdown()));
}

TEST(MasterPacketsTests, Golden) {
	MasterPackets::PlayerAdded added;
	added.zoneID = 1100;
	added.instanceID = 2;
	// 53 | MASTER (6) | PLAYER_ADDED (7) | pad | u16 1100 | u16 2
	EXPECT_PACKET_EQ(FromHex("53 06 00 07 00 00 00 00 4c 04 02 00"), StructPacket(added));

	MasterPackets::AffirmTransferRequest affirm;
	affirm.requestID = 0x0102030405060708ULL;
	// AFFIRM_TRANSFER_REQUEST (17) | u64
	EXPECT_PACKET_EQ(FromHex("53 06 00 11 00 00 00 00 08 07 06 05 04 03 02 01"), StructPacket(affirm));
}

TEST(MasterPacketsTests, ServerListMatchesLegacy) {
	for (size_t count = 0; count <= 3; count++) {
		std::vector<LegacyMaster::ServerListInstance> instances;
		MasterPackets::ServerListResponse response;
		response.authOnline = 1;
		response.chatOnline = count % 2;
		for (size_t i = 0; i < count; i++) {
			LegacyMaster::ServerListInstance instance{ static_cast<LWOMAPID>(1000 + i), static_cast<LWOINSTANCEID>(i), static_cast<LWOCLONEID>(i * 7), static_cast<uint32_t>(i * 3), "10.0.0." + std::to_string(i), static_cast<uint32_t>(3000 + i), i == 2 };
			instances.push_back(instance);
			auto& entry = response.instances.emplace_back();
			entry.mapID = instance.mapID;
			entry.instanceID = instance.instanceID;
			entry.cloneID = instance.cloneID;
			entry.players = instance.players;
			entry.ip = LUString(instance.ip);
			entry.port = instance.port;
			entry.isPrivate = instance.isPrivate ? 1 : 0;
			entry.state = static_cast<MasterPackets::ServerListResponse::eState>(i % 3);
		}
		response.ugcEnabled = 1;
		response.ugcOnline = count % 2;
		response.ugcPid = 4242 + count;
		// The legacy list, then the UGC server's state
		EXPECT_PACKET_EQ(Written([&](RakNet::BitStream& b) {
			LegacyMaster::WriteServerList(b, true, count % 2, instances);
			b.Write<uint8_t>(1);
			b.Write<uint8_t>(count % 2);
			b.Write<uint32_t>(4242 + count);
			for (size_t i = 0; i < count; i++) b.Write<uint8_t>(static_cast<uint8_t>(i % 3)); // each world's state
		}), StructPacket(response));

		RakNet::BitStream stream; LoadPayload(stream, response);
		const auto legacy = LegacyMaster::ReadServerList(stream);
		const auto copy = RoundTrip(response);
		ASSERT_EQ(legacy.instances.size(), copy.instances.size());
		for (size_t i = 0; i < count; i++) {
			EXPECT_EQ(legacy.instances[i].mapID, copy.instances[i].mapID);
			EXPECT_EQ(legacy.instances[i].players, copy.instances[i].players);
			EXPECT_EQ(legacy.instances[i].ip, copy.instances[i].ip.string);
			EXPECT_EQ(legacy.instances[i].isPrivate, copy.instances[i].isPrivate != 0);
			EXPECT_EQ(static_cast<size_t>(copy.instances[i].state), i % 3);
		}
		EXPECT_EQ(copy.ugcEnabled, 1);
		EXPECT_EQ(copy.ugcOnline, count % 2);
		EXPECT_EQ(copy.ugcPid, 4242 + count);
		ExpectTruncatedFails(response);
	}
}

// The dashboard and migration structs kept their payloads; they now carry their own header
TEST(MasterPacketsTests, DashboardAndMigrationStructsKeepTheirBytes) {
	PlayerActionRequest action;
	action.requestId = 7;
	action.action = ePlayerAction::CHAT_MESSAGE;
	action.text = "hello";
	action.name = "[Discord] Bob";
	ExpectHeaderThenSerialize(action, MessageType::Master::PLAYER_ACTION);

	PlayerActionResult result;
	result.requestId = 7;
	result.affected = 3;
	result.timedOut = true;
	ExpectHeaderThenSerialize(result, MessageType::Master::PLAYER_ACTION_RESULT);

	DataChanged changed;
	changed.entries.push_back({ "characters", 42 });
	ExpectHeaderThenSerialize(changed, MessageType::Master::DATA_CHANGED);

	UgcModelsMade made;
	made.blueprintIds = { 1152921510436831123, 42 };
	ExpectHeaderThenSerialize(made, MessageType::Master::UGC_MODELS_MADE);

	PlayerPositions positions;
	positions.zoneId = 1100;
	positions.players.push_back({ 42, 1.0f, 2.0f, 3.0f });
	ExpectHeaderThenSerialize(positions, MessageType::Master::PLAYER_POSITIONS);

	Announcement announcement;
	announcement.title = "Title";
	announcement.message = "Message";
	announcement.zones = { 1100, 1200 };
	ExpectHeaderThenSerialize(announcement, MessageType::Master::ANNOUNCE);

	MessageCaptureControl control;
	control.captureId = 3;
	control.seconds = 60;
	control.only = { 1, 2 };
	ExpectHeaderThenSerialize(control, MessageType::Master::MESSAGE_CAPTURE_CONTROL);

	MessageCaptureData data;
	data.captureId = 3;
	data.entries.resize(1);
	data.entries[0].payload = "abc";
	ExpectHeaderThenSerialize(data, MessageType::Master::MESSAGE_CAPTURE_DATA);

	InstanceMigrationRequest migrate;
	migrate.requestId = 9;
	migrate.requestedBy = "GM";
	ExpectHeaderThenSerialize(migrate, MessageType::Master::INSTANCE_MIGRATE);

	MigratePlayersOrder order;
	order.targetIp = "127.0.0.1";
	ExpectHeaderThenSerialize(order, MessageType::Master::MIGRATE_PLAYERS);

	MigrationStatus status;
	status.message = "Moving players";
	ExpectHeaderThenSerialize(status, MessageType::Master::MIGRATE_STATUS);

	CarriedPlayerState state;
	state.characterId = 42;
	ExpectHeaderThenSerialize(state, MessageType::Master::MIGRATE_PLAYER_STATE);
}

TEST(MasterPacketsTests, DispatcherRoutesAndDropsBadPackets) {
	// Dropped packets are logged
	auto* const previousLogger = Game::logger;
	Game::logger = new Logger("./testing.log", true, true);
	PacketDispatcher<MessageType::Master> handlers;
	int calls = 0;
	MasterPackets::PlayerAdded seen;
	handlers.On<MasterPackets::PlayerAdded>(MessageType::Master::PLAYER_ADDED, [&](const MasterPackets::PlayerAdded& msg, const SystemAddress&) { calls++; seen = msg; });

	MasterPackets::PlayerAdded added;
	added.zoneID = 1100;
	added.instanceID = 4;
	RakNet::BitStream good;
	added.WritePacket(good);
	Packet packet{};
	packet.data = good.GetData();
	packet.length = good.GetNumberOfBytesUsed();
	EXPECT_TRUE(handlers.Dispatch(&packet, ServiceType::MASTER));
	EXPECT_EQ(calls, 1);
	EXPECT_EQ(seen.zoneID, 1100);
	EXPECT_EQ(seen.instanceID, 4);

	// Wrong service: not ours
	EXPECT_FALSE(handlers.Dispatch(&packet, ServiceType::CHAT));

	// Truncated: handled (dropped), the handler isn't called
	packet.length -= 1;
	EXPECT_TRUE(handlers.Dispatch(&packet, ServiceType::MASTER));
	EXPECT_EQ(calls, 1);

	// No handler for this ID
	MasterPackets::PlayerRemoved removed;
	RakNet::BitStream other;
	removed.WritePacket(other);
	packet.data = other.GetData();
	packet.length = other.GetNumberOfBytesUsed();
	EXPECT_FALSE(handlers.Dispatch(&packet, ServiceType::MASTER));

	delete Game::logger;
	Game::logger = previousLogger;
}

// UGC server -> master -> worlds: a u16 count, then each blueprint id
TEST(MasterPacketsTests, UgcModelsMadeBytes) {
	UgcModelsMade made;
	made.blueprintIds = { 0x0102030405060708, 9 };
	EXPECT_PACKET_EQ(FromHex("53 06 00 25 00 00 00 00 02 00 08 07 06 05 04 03 02 01 09 00 00 00 00 00 00 00"), StructPacket(made));

	RakNet::BitStream tooMany;
	tooMany.Write<uint16_t>(UgcModelsMade::MAX_MODELS + 1);
	UgcModelsMade read;
	EXPECT_FALSE(read.Deserialize(tooMany));

	RakNet::BitStream truncated;
	truncated.Write<uint16_t>(2);
	truncated.Write<LWOOBJID>(1);
	EXPECT_FALSE(read.Deserialize(truncated));
}

TEST(MasterPacketsTests, CDClientReload) {
	CDClientReload reload;
	reload.fdb = "cdclient-0123456789abcdef.fdb";
	reload.sqlite = "CDServer-0123456789abcdef.sqlite";
	ExpectHeaderThenSerialize(reload, MessageType::Master::CDCLIENT_RELOAD);
	EXPECT_FALSE(reload.IsRequest());

	CDClientReload request;
	request.requesterId = 42;
	ExpectHeaderThenSerialize(request, MessageType::Master::CDCLIENT_RELOAD);
	EXPECT_TRUE(request.IsRequest());

	// A name that leaves resServer, or only one of the two names, is refused
	for (const auto& [fdb, sqlite] : std::vector<std::pair<std::string, std::string>>{ { "../cdclient.fdb", "CDServer.sqlite" }, { "cdclient-1.fdb", "" } }) {
		CDClientReload bad;
		bad.fdb = fdb;
		bad.sqlite = sqlite;
		RakNet::BitStream stream;
		bad.Serialize(stream);
		CDClientReload read;
		EXPECT_FALSE(read.Deserialize(stream)) << fdb;
	}
}
