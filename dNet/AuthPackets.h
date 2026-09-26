#ifndef AUTHPACKETS_H
#define AUTHPACKETS_H

#define _VARIADIC_MAX 10
#include "dCommonVars.h"
#include "dNetCommon.h"
#include "magic_enum.hpp"
#include "BitStreamUtils.h"
#include "ClientPackets.h"
#include "MessageType/Auth.h"

enum class eLoginResponse : uint8_t;
enum class ServiceType : uint16_t;
class dServer;

enum class ClientOS : uint8_t {
	UNKNOWN,
	WINDOWS,
	MACOS
};

enum class LanguageCodeID : uint16_t {
	de_DE = 0x0407,
	en_US = 0x0409,
	en_GB = 0x0809
};

template <>
struct magic_enum::customize::enum_range<LanguageCodeID> {
	static constexpr int min = 1031;
	static constexpr int max = 2057;
};


namespace AuthPackets {
	// Client -> auth server. The username and password, plus a description of the client's machine.
	struct LoginRequest : public LUBitStream {
		// Set by the dispatcher before Deserialize and Handle.
		SystemAddress sysAddr = UNASSIGNED_SYSTEM_ADDRESS;

		LUWString username{ 33 };
		LUWString password{ 41 };
		LanguageCodeID localeID{};
		ClientOS clientOS{};
		LUWString memoryStats{ 256 };
		LUWString videoCard{ 128 };
		// Processor
		uint32_t numberOfProcessors{};
		uint32_t processorType{};
		uint16_t processorLevel{};
		uint16_t processorRevision{};
		// OS version
		uint32_t osVersionInfoSize{};
		uint32_t majorVersion{};
		uint32_t minorVersion{};
		uint32_t buildNumber{};
		uint32_t platformID{};

		LoginRequest() : LUBitStream(ServiceType::AUTH, MessageType::Auth::LOGIN_REQUEST) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;
	};

	// Handles a ServiceType::AUTH packet whose header has already been read from inStream.
	void Handle(RakNet::BitStream& inStream, const SystemAddress& sysAddr, uint32_t packetID);

	// Answers a login with a ClientPackets::LoginResponse filled from the server's settings (event gating, client
	// version) and a new session key; on success also registers that session key with the master server.
	void SendLoginResponse(dServer* server, const SystemAddress& sysAddr, eLoginResponse responseCode, const std::string& errorMsg, const std::string& wServerIP, uint16_t wServerPort, std::string username, std::vector<Stamp>& stamps);
	void LoadClaimCodes();
}

#endif // AUTHPACKETS_H
