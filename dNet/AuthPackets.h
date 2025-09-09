#ifndef AUTHPACKETS_H
#define AUTHPACKETS_H

#define _VARIADIC_MAX 10
#include "dCommonVars.h"
#include "dNetCommon.h"
#include "magic_enum.hpp"
#include "BitStream.h"
#include "RakNetTypes.h"
#include "BitStreamUtils.h"
#include <functional>
#include <memory>

enum class eLoginResponse : uint8_t;
enum class ServiceType : uint16_t;
class dServer;

enum class eMessageID : uint32_t {
	HandshakeRequest = 0,
	HandshakeResponse,
	LoginRequest,
	LoginResponse,
	UnknownError
};

enum class eStamps : uint32_t {
	PASSPORT_AUTH_START,
	PASSPORT_AUTH_BYPASS,
	PASSPORT_AUTH_ERROR,
	PASSPORT_AUTH_DB_SELECT_START,
	PASSPORT_AUTH_DB_SELECT_FINISH,
	PASSPORT_AUTH_DB_INSERT_START,
	PASSPORT_AUTH_DB_INSERT_FINISH,
	PASSPORT_AUTH_LEGOINT_COMMUNICATION_START,
	PASSPORT_AUTH_LEGOINT_RECEIVED,
	PASSPORT_AUTH_LEGOINT_THREAD_SPAWN,
	PASSPORT_AUTH_LEGOINT_WEBSERVICE_START,
	PASSPORT_AUTH_LEGOINT_WEBSERVICE_FINISH,
	PASSPORT_AUTH_LEGOINT_LEGOCLUB_START,
	PASSPORT_AUTH_LEGOINT_LEGOCLUB_FINISH,
	PASSPORT_AUTH_LEGOINT_THREAD_FINISH,
	PASSPORT_AUTH_LEGOINT_REPLY,
	PASSPORT_AUTH_LEGOINT_ERROR,
	PASSPORT_AUTH_LEGOINT_COMMUNICATION_END,
	PASSPORT_AUTH_LEGOINT_DISCONNECT,
	PASSPORT_AUTH_WORLD_COMMUNICATION_START,
	PASSPORT_AUTH_CLIENT_OS,
	PASSPORT_AUTH_WORLD_PACKET_RECEIVED,
	PASSPORT_AUTH_IM_COMMUNICATION_START,
	PASSPORT_AUTH_IM_LOGIN_START,
	PASSPORT_AUTH_IM_LOGIN_ALREADY_LOGGED_IN,
	PASSPORT_AUTH_IM_OTHER_LOGIN_REMOVED,
	PASSPORT_AUTH_IM_LOGIN_QUEUED,
	PASSPORT_AUTH_IM_LOGIN_RESPONSE,
	PASSPORT_AUTH_IM_COMMUNICATION_END,
	PASSPORT_AUTH_WORLD_SESSION_CONFIRM_TO_AUTH,
	PASSPORT_AUTH_WORLD_COMMUNICATION_FINISH,
	PASSPORT_AUTH_WORLD_DISCONNECT,
	NO_LEGO_INTERFACE,
	DB_ERROR,
	GM_REQUIRED,
	NO_LEGO_WEBSERVICE_XML,
	LEGO_WEBSERVICE_TIMEOUT,
	LEGO_WEBSERVICE_ERROR,
	NO_WORLD_SERVER
};

struct Stamp {
	eStamps type;
	uint32_t value;
	uint64_t timestamp;

	Stamp(eStamps type, uint32_t value, uint64_t timestamp = time(nullptr)){
		this->type = type;
		this->value = value;
		this->timestamp = timestamp;
	}

	void Serialize(RakNet::BitStream& outBitStream);
};

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

// For more info on these values, go to this link
// https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/ns-sysinfoapi-system_info
enum class ProcessorType : uint32_t {
    UNKNOWN    = 0,
    INTEL_386  = 386,      // Intel 80386
    INTEL_486  = 486,      // Intel 80486
    PENTIUM    = 586,      // Intel Pentium
    MIPS       = 4000,     // MIPS R4000
    ALPHA      = 21064,    // Alpha 21064
    PPC        = 601,      // PowerPC 601
    SHX        = 103,      // SuperH SHX
    INTEL_IA64 = 2200,     // Intel Itanium
    AMD_X8664  = 8664,     // x64 (AMD or Intel EM64T)
    ARM        = 448,      // ARM (0x01C0 in hex, 448 in decimal)
    ARM64      = 43620     // ARM64 (0xAA64 in hex, 43620 in decimal)
};

enum class ProcessorLevel : uint16_t {
    INTEL_386 = 3,
    INTEL_486 = 4,
    PENTIUM = 5,
    INTEL_P6 = 6, // Pentium Pro/II/III/4 or later
    ITANIUM = 0xA,
    X64 = 0xE,
    ARM = 0x14,
    ARM64 = 0x15,
    UNKNOWN = 0xFFFF
};

// ProcessorRevision is typically a 16-bit value, but we can define some common values for reference
// For x86: high byte = model, low byte = stepping
// For other architectures, refer to documentation
enum class ProcessorRevision : uint16_t {
    // Intel 80386
    INTEL_386_MODEL_0_STEPPING_0 = 0x0000,
    INTEL_386_MODEL_0_STEPPING_1 = 0x0001,
    // Intel 80486
    INTEL_486_MODEL_0_STEPPING_0 = 0x0000,
    INTEL_486_MODEL_0_STEPPING_1 = 0x0001,
    // Intel Pentium
    PENTIUM_MODEL_1_STEPPING_1 = 0x0101,
    PENTIUM_MODEL_2_STEPPING_3 = 0x0203,
    // Intel Pentium Pro/II/III/4
    P6_MODEL_3_STEPPING_3 = 0x0303,
    P6_MODEL_5_STEPPING_7 = 0x0507,
    // Intel Itanium
    ITANIUM_MODEL_1_STEPPING_0 = 0x0100,
    // AMD x64
    AMD_X64_MODEL_15_STEPPING_65 = 0x0F41,
    AMD_X64_MODEL_23_STEPPING_1 = 0x1701,
    // Intel x64
    INTEL_X64_MODEL_6_STEPPING_10 = 0x060A,
    INTEL_X64_MODEL_15_STEPPING_11 = 0x0F0B,
    // ARM/ARM64 (values are typically implementation-specific)
    ARM_MODEL_0_STEPPING_0 = 0x0000,
    ARM64_MODEL_0_STEPPING_0 = 0x0000,
    // Unknown
    UNKNOWN = 0xFFFF
};

// Windows version enum based on OSVERSIONINFOEXA major/minor values
// https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-osversioninfoexa
enum class WindowsVersion : uint32_t {
    UNKNOWN         = 0,
    WIN_2000        = 0x0500, // Major: 5, Minor: 0
    WIN_XP          = 0x0501, // Major: 5, Minor: 1
    WIN_SERVER_2003 = 0x0502, // Major: 5, Minor: 2
    WIN_VISTA       = 0x0600, // Major: 6, Minor: 0
    WIN_7           = 0x0601, // Major: 6, Minor: 1
    WIN_8           = 0x0602, // Major: 6, Minor: 2
    WIN_8_1         = 0x0603, // Major: 6, Minor: 3
    WIN_10          = 0x0A00, // Major: 10, Minor: 0
    WIN_11          = 0x0A00  // Major: 10, Minor: 0 (distinguish by build number)
};

// Windows Platform ID enum based on OSVERSIONINFOEXA documentation
// https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-osversioninfoexa
enum class WindowsPlatformID : uint32_t {
    UNKNOWN                = 0,
    WIN32s                 = 0, // Windows 3.x
    WIN32_WINDOWS          = 1, // Windows 95/98/ME
    WIN32_NT               = 2, // Windows NT and later
    WIN32_CE               = 3  // Windows CE
};

// Windows Build Number enum (common values)
enum class WindowsBuildNumber : uint32_t {
    UNKNOWN    = 0,
    WIN_2000   = 2195,
    WIN_XP     = 2600,
    WIN_SERVER_2003 = 3790,
    WIN_VISTA  = 6000,
    WIN_7      = 7600,
    WIN_7_SP1  = 7601,
    WIN_8      = 9200,
    WIN_8_1    = 9600,
    WIN_10_1507 = 10240,
    WIN_10_1511 = 10586,
    WIN_10_1607 = 14393,
    WIN_10_1703 = 15063,
    WIN_10_1709 = 16299,
    WIN_10_1803 = 17134,
    WIN_10_1809 = 17763,
    WIN_10_1903 = 18362,
    WIN_10_1909 = 18363,
    WIN_10_2004 = 19041,
    WIN_10_20H2 = 19042,
    WIN_10_21H1 = 19043,
    WIN_10_21H2 = 19044,
    WIN_10_22H2 = 19045,
    WIN_11_21H2 = 22000,
    WIN_11_22H2 = 22621,
    WIN_11_23H2 = 22631
};

template <>
struct magic_enum::customize::enum_range<LanguageCodeID> {
	static constexpr int min = 1031;
	static constexpr int max = 2057;
};

enum class Language : uint32_t {
	en_US,
	pl_US,
	de_DE,
	en_GB,
};

namespace AuthPackets {
	struct AuthLUBitStream : public LUBitStream {
		eMessageID messageID = eMessageID::UnknownError;
		SystemAddress sysAddr = UNASSIGNED_SYSTEM_ADDRESS;
		dServer* server = nullptr;

		AuthLUBitStream() = default;
		AuthLUBitStream(eMessageID _messageID) : messageID{_messageID} {};

		virtual void Serialize(RakNet::BitStream& bitStream) const override;
		virtual bool Deserialize(RakNet::BitStream& bitStream) override;
		virtual void Handle() override {};
	};

	struct HandshakeRequest : public AuthLUBitStream {
		uint32_t clientVersion = 0;
		ServiceType serviceType;
		uint32_t processID = 0;
		uint16_t port = 0;

		HandshakeRequest() : AuthLUBitStream(eMessageID::HandshakeRequest) {}
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;
	};

	struct HandshakeResponse : public AuthLUBitStream {
		std::string nextServerIP;
		uint16_t nextServerPort = 0;
		ServiceType serverType;

		HandshakeResponse() : AuthLUBitStream(eMessageID::HandshakeResponse) {}
		HandshakeResponse(const std::string& ip, uint16_t port, ServiceType type) 
			: AuthLUBitStream(eMessageID::HandshakeResponse), nextServerIP(ip), nextServerPort(port), serverType(type) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct LoginRequest : public AuthLUBitStream {
		std::string username;
		std::string password;
		LanguageCodeID locale_id;
		ClientOS clientOS;
		struct ComputerInfo {
			std::string memoryStats;
			std::string videoCard;
			struct ProcessorInfo {
				uint32_t count = 0;
				ProcessorType type = ProcessorType::UNKNOWN;
				ProcessorLevel level = ProcessorLevel::UNKNOWN;
				ProcessorRevision revision = ProcessorRevision::UNKNOWN;
			} processorInfo;
			struct OSVersionInfo {
				uint32_t infoSize = 0;
				WindowsVersion version = WindowsVersion::UNKNOWN;
				WindowsBuildNumber buildNumber = WindowsBuildNumber::UNKNOWN;
				WindowsPlatformID platformID = WindowsPlatformID::UNKNOWN;
			} osVersionInfo;
		} computerInfo;

		LoginRequest() : AuthLUBitStream(eMessageID::LoginRequest) {}
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;
	};

	struct LoginResponse : public AuthLUBitStream {
		eLoginResponse responseCode;
		std::string errorMsg;
		std::string wServerIP;
		uint16_t wServerPort = 0;
		std::string username;
		std::vector<Stamp> stamps;

		LoginResponse() : AuthLUBitStream(eMessageID::LoginResponse) {}
		LoginResponse(eLoginResponse code, const std::string& error, const std::string& ip, uint16_t port, const std::string& user, std::vector<Stamp>& s)
			: AuthLUBitStream(eMessageID::LoginResponse), responseCode(code), errorMsg(error), wServerIP(ip), wServerPort(port), username(user), stamps(s) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	// Legacy function signatures maintained for backward compatibility
	void HandleHandshake(dServer* server, Packet* packet);
	void SendHandshake(dServer* server, const SystemAddress& sysAddr, const std::string& nextServerIP, uint16_t nextServerPort, const ServiceType serverType);

	void HandleLoginRequest(dServer* server, Packet* packet);
	void SendLoginResponse(dServer* server, const SystemAddress& sysAddr, eLoginResponse responseCode, const std::string& errorMsg, const std::string& wServerIP, uint16_t wServerPort, std::string username, std::vector<Stamp>& stamps);

	void HandleAuth(RakNet::BitStream& inStream, const SystemAddress& sysAddr, dServer* server);
	void LoadClaimCodes();
}

#endif // AUTHPACKETS_H
