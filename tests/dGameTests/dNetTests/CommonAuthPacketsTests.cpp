#include "AuthPackets.h"
#include "ClientPackets.h"
#include "CommonPackets.h"
#include "GameDependencies.h"
#include "PacketTestUtils.h"
#include "Legacy/CommonAuthPacketsLegacy.h"

#include "eLoginResponse.h"
#include "eServerDisconnectIdentifiers.h"
#include "magic_enum.hpp"

#include <cstdlib>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace PacketTestUtils;

namespace {
	SystemAddress TestAddress(uint16_t port = 1234) {
		SystemAddress address;
		address.binaryAddress = 0x0100007F;
		address.port = port;
		return address;
	}

	// Runs a handler on a copy of the packet (header included), the way the servers receive it.
	Packet MakePacket(RakNet::BitStream& bitStream, const SystemAddress& sysAddr) {
		Packet packet{};
		packet.systemAddress = sysAddr;
		packet.data = bitStream.GetData();
		packet.length = bitStream.GetNumberOfBytesUsed();
		packet.bitSize = bitStream.GetNumberOfBitsUsed();
		return packet;
	}

	// Reads the header like the servers do and hands the rest to a dispatcher.
	void Dispatch(RakNet::BitStream& packetBytes, const SystemAddress& sysAddr, const std::function<void(RakNet::BitStream&, const SystemAddress&, uint32_t)>& handler) {
		RakNet::BitStream inStream(packetBytes.GetData(), packetBytes.GetNumberOfBytesUsed(), false);
		LUBitStream header;
		ASSERT_TRUE(header.ReadHeader(inStream));
		handler(inStream, sysAddr, header.internalPacketID);
	}

	void ExpectSamePackets(const std::vector<CapturedPacket>& expected, const std::vector<CapturedPacket>& actual) {
		ASSERT_FALSE(expected.empty());
		ASSERT_EQ(expected.size(), actual.size());
		for (size_t i = 0; i < expected.size(); i++) {
			EXPECT_PACKET_EQ(FromCapture(expected[i]), FromCapture(actual[i]));
			EXPECT_EQ(expected[i].sysAddr, actual[i].sysAddr);
			EXPECT_EQ(expected[i].broadcast, actual[i].broadcast);
		}
	}

	// Runs both, retrying if the second changed while they ran (stamps carry time(nullptr)).
	void ExpectSameOutput(const std::function<void()>& legacy, const std::function<void()>& converted) {
		for (int attempt = 0; attempt < 3; attempt++) {
			const auto before = std::time(nullptr);
			Game::randomEngine.seed(1234);
			const auto expected = Capture(legacy);
			Game::randomEngine.seed(1234);
			const auto actual = Capture(converted);
			if (before != std::time(nullptr)) continue;
			ExpectSamePackets(expected, actual);
			return;
		}
		FAIL() << "the clock kept changing seconds while comparing";
	}

	template<typename T>
	T RoundTrip(const T& packet) {
		RakNet::BitStream first;
		packet.WritePacket(first);
		T copy;
		EXPECT_TRUE(copy.ReadHeader(first));
		EXPECT_TRUE(copy.Deserialize(first));
		EXPECT_EQ(first.GetNumberOfUnreadBits(), 0);
		RakNet::BitStream second;
		copy.WritePacket(second);
		EXPECT_PACKET_EQ(FromBitStream(first), FromBitStream(second));
		return copy;
	}

	template<typename T>
	void ExpectTruncatedFails(const T& packet) {
		RakNet::BitStream full;
		packet.WritePacket(full);
		const auto bytes = full.GetNumberOfBytesUsed();
		for (uint32_t cut = 8; cut < bytes; cut++) {
			RakNet::BitStream truncated(full.GetData(), cut, true);
			T copy;
			ASSERT_TRUE(copy.ReadHeader(truncated));
			EXPECT_FALSE(copy.Deserialize(truncated)) << "cut at " << cut << " of " << bytes;
		}
	}

	const std::vector<std::string> g_Strings = { "", "a", "Hello World", "user_name-01", std::string(40, 'x'), std::string(300, 'y') };
}

class CommonAuthPacketsTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

TEST_F(CommonAuthPacketsTests, VersionConfirmHandshakeMatchesLegacy) {
	for (const uint32_t netVersion : { 0u, 171022u, 0xFFFFFFFFu }) {
		for (const auto serviceType : { ServiceType::CLIENT, ServiceType::AUTH, static_cast<ServiceType>(0xFFFF) }) {
			for (const uint16_t port : { uint16_t{ 0 }, uint16_t{ 1234 }, uint16_t{ 0xFFFF } }) {
				CommonPackets::ClientVersionConfirm request;
				request.netVersion = netVersion;
				request.unknown = 0x12345678;
				request.serviceType = serviceType;
				request.processID = 4321;
				request.port = port;
				request.unknown2 = LUString("127.0.0.1");
				RakNet::BitStream bytes;
				request.WritePacket(bytes);
				const auto sysAddr = TestAddress();

				ExpectSameOutput(
					[&] { auto packet = MakePacket(bytes, sysAddr); LegacyAuthPackets::HandleHandshake(Game::server, &packet); },
					[&] { Dispatch(bytes, sysAddr, CommonPackets::Handle); });

				const auto copy = RoundTrip(request);
				EXPECT_EQ(copy.netVersion, netVersion);
				EXPECT_EQ(copy.serviceType, serviceType);
				EXPECT_EQ(copy.port, port);
				EXPECT_EQ(copy.unknown2.string, "127.0.0.1");
			}
		}
	}
}

TEST_F(CommonAuthPacketsTests, ServerVersionConfirmMatchesLegacy) {
	for (const auto serviceType : { ServiceType::AUTH, ServiceType::WORLD, ServiceType::CHAT }) {
		const auto sysAddr = TestAddress();
		ExpectSameOutput(
			[&] { LegacyAuthPackets::SendHandshake(Game::server, sysAddr, "ignored", 1, serviceType); },
			[&] {
				CommonPackets::ServerVersionConfirm response;
				response.serviceType = static_cast<uint32_t>(serviceType);
				response.Send(sysAddr);
			});
	}
}

TEST_F(CommonAuthPacketsTests, VersionConfirmGoldenBytes) {
	CommonPackets::ServerVersionConfirm response;
	response.serviceType = static_cast<uint32_t>(ServiceType::WORLD);
	RakNet::BitStream bytes;
	response.WritePacket(bytes);
	// 0x53 | COMMON u16 | VERSION_CONFIRM u32 | pad | 171022 | 861228100 | WORLD u32 | 219818307120 u64
	EXPECT_PACKET_EQ(FromHex("53 00 00 00 00 00 00 00 0e 9c 02 00 44 4c 55 33 04 00 00 00 30 2e 31 2e 33 00 00 00"), FromBitStream(bytes));
	RoundTrip(response);

	CommonPackets::ClientVersionConfirm request;
	request.netVersion = 171022;
	request.serviceType = ServiceType::CLIENT;
	request.processID = 0x11223344;
	request.port = 0x5566;
	RakNet::BitStream requestBytes;
	request.WritePacket(requestBytes);
	std::string zeros;
	for (int i = 0; i < 33; i++) zeros += " 00";
	// ... | 171022 | unknown 0 | CLIENT u16 | pad u16 | process id | port | 33 byte string
	EXPECT_PACKET_EQ(FromHex("53 00 00 00 00 00 00 00 0e 9c 02 00 00 00 00 00 05 00 00 00 44 33 22 11 66 55" + zeros), FromBitStream(requestBytes));
	ExpectTruncatedFails(request);
}

TEST_F(CommonAuthPacketsTests, DisconnectNotifyMatchesLegacy) {
	for (const auto id : magic_enum::enum_values<eServerDisconnectIdentifiers>()) {
		RakNet::BitStream expected;
		LegacyAuthPackets::WriteDisconnectNotify(expected, id);
		CommonPackets::DisconnectNotify notify;
		notify.disconnectID = id;
		RakNet::BitStream actual;
		notify.WritePacket(actual);
		EXPECT_PACKET_EQ(FromBitStream(expected), FromBitStream(actual));
		EXPECT_EQ(RoundTrip(notify).disconnectID, id);
	}

	CommonPackets::DisconnectNotify kick;
	kick.disconnectID = eServerDisconnectIdentifiers::KICK;
	RakNet::BitStream bytes;
	kick.WritePacket(bytes);
	EXPECT_PACKET_EQ(FromHex("53 00 00 01 00 00 00 00 0b 00 00 00"), FromBitStream(bytes));
	ExpectTruncatedFails(kick);
}

TEST_F(CommonAuthPacketsTests, GeneralNotifyGoldenAndRoundTrip) {
	CommonPackets::GeneralNotify notify;
	notify.notifyType = 0;
	notify.showMessageBox = true;
	RakNet::BitStream bytes;
	notify.WritePacket(bytes);
	EXPECT_PACKET_EQ(FromHex("53 00 00 02 00 00 00 00 00 00 00 00 01"), FromBitStream(bytes));
	const auto copy = RoundTrip(notify);
	EXPECT_EQ(copy.notifyType, 0);
	EXPECT_TRUE(copy.showMessageBox);
	ExpectTruncatedFails(notify);
}

TEST_F(CommonAuthPacketsTests, LoginResponseMatchesLegacy) {
	// Event gating comes from the config; environment variables override it
	setenv("EVENT_1", "Event One", 1);
	setenv("EVENT_8", std::string(40, 'e').c_str(), 1);
	setenv("VERSION_MINOR", "99", 1);

	const std::vector<eLoginResponse> codes = { eLoginResponse::GENERAL_FAILED, eLoginResponse::BANNED, eLoginResponse::PERMISSIONS_NOT_HIGH_ENOUGH, eLoginResponse::INVALID_USER, eLoginResponse::WRONG_PASS, eLoginResponse::ACCOUNT_LOCKED };
	for (const auto code : codes) {
		for (const auto& text : g_Strings) {
			for (const size_t stampCount : { size_t{ 0 }, size_t{ 1 }, size_t{ 5 } }) {
				std::vector<Stamp> legacyStamps;
				for (size_t i = 0; i < stampCount; i++) legacyStamps.emplace_back(static_cast<eStamps>(i), static_cast<uint32_t>(i * 3), 1000 + i);
				auto stamps = legacyStamps;
				const auto sysAddr = TestAddress();
				ExpectSameOutput(
					[&] { auto copy = legacyStamps; LegacyAuthPackets::SendLoginResponse(Game::server, sysAddr, code, text, text, 2001, "user", copy); },
					[&] { auto copy = stamps; AuthPackets::SendLoginResponse(Game::server, sysAddr, code, text, text, 2001, "user", copy); });
			}
		}
	}

	unsetenv("EVENT_1");
	unsetenv("EVENT_8");
	unsetenv("VERSION_MINOR");
}

TEST_F(CommonAuthPacketsTests, LoginResponseRoundTrip) {
	ClientPackets::LoginResponse response;
	response.responseCode = eLoginResponse::SUCCESS;
	response.events[0] = LUString("Talk_Like_A_Pirate");
	response.versionMajor = 1;
	response.versionCurrent = 10;
	response.versionMinor = 64;
	response.userKey = LUWString("0123456789abcdef0123456789abcdef");
	response.worldServerIP = LUString("192.168.1.2");
	response.worldServerPort = 2000;
	response.errorMessage = "Something went wrong";
	response.stamps = { Stamp(eStamps::PASSPORT_AUTH_START, 0, 5), Stamp(eStamps::NO_WORLD_SERVER, 1, 6) };
	const auto copy = RoundTrip(response);
	EXPECT_EQ(copy.responseCode, eLoginResponse::SUCCESS);
	EXPECT_EQ(copy.events[0].string, "Talk_Like_A_Pirate");
	EXPECT_EQ(copy.versionMinor, 64);
	EXPECT_EQ(copy.worldServerIP.string, "192.168.1.2");
	EXPECT_EQ(copy.worldServerPort, 2000);
	EXPECT_EQ(copy.cdnTicket.string, ClientPackets::LoginResponse::DEFAULT_CDN_TICKET);
	EXPECT_EQ(copy.localization.string, "US");
	EXPECT_EQ(copy.errorMessage, "Something went wrong");
	ASSERT_EQ(copy.stamps.size(), 2);
	EXPECT_EQ(copy.stamps[1].type, eStamps::NO_WORLD_SERVER);
	EXPECT_EQ(copy.stamps[1].timestamp, 6);
	ExpectTruncatedFails(response);

	response.errorMessage.clear();
	response.stamps.clear();
	EXPECT_TRUE(RoundTrip(response).errorMessage.empty());
}

TEST_F(CommonAuthPacketsTests, LoginRequestMatchesLegacy) {
	for (const auto& username : g_Strings) {
		AuthPackets::LoginRequest request;
		request.username = LUWString(username);
		request.password = LUWString(std::string("hunter2"), 41);
		request.localeID = LanguageCodeID::en_US;
		request.clientOS = ClientOS::WINDOWS;
		request.memoryStats = LUWString(std::string("Memory"), 256);
		request.videoCard = LUWString(std::string("Video card"), 128);
		request.numberOfProcessors = 8;
		request.processorType = 586;
		request.processorLevel = 6;
		request.processorRevision = 0x3a09;
		request.osVersionInfoSize = 148;
		request.majorVersion = 6;
		request.minorVersion = 1;
		request.buildNumber = 7601;
		request.platformID = 2;
		RakNet::BitStream bytes;
		request.WritePacket(bytes);
		const auto sysAddr = TestAddress();

		// The test database knows no accounts, so both answer INVALID_USER
		ExpectSameOutput(
			[&] { auto packet = MakePacket(bytes, sysAddr); LegacyAuthPackets::HandleLoginRequest(Game::server, &packet); },
			[&] { Dispatch(bytes, sysAddr, AuthPackets::Handle); });

		const auto copy = RoundTrip(request);
		EXPECT_EQ(copy.username.GetAsString(), username.substr(0, 33));
		EXPECT_EQ(copy.password.GetAsString(), "hunter2");
		EXPECT_EQ(copy.localeID, LanguageCodeID::en_US);
		EXPECT_EQ(copy.buildNumber, 7601);
		EXPECT_EQ(copy.platformID, 2);
		ExpectTruncatedFails(request);
	}
}
