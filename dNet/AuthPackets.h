#ifndef AUTHPACKETS_H
#define AUTHPACKETS_H

#define _VARIADIC_MAX 10
#include "dCommonVars.h"
#include "dNetCommon.h"
#include "magic_enum.hpp"
#include "BitStream.h"
#include "RakNetTypes.h"
#include "BitStreamUtils.h"
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

	struct LoginRequest : public LUBitStream {
		std::string username;
		std::string password;
		LanguageCodeID locale_id;
		ClientOS clientOS;
		struct ComputerInfo {
			std::string memoryStats;
			std::string videoCard;
			struct ProcessorInfo {
				uint32_t count = 0;
				uint32_t type = 0;
				uint16_t level = 0;
				uint16_t revision = 0;
			} processorInfo;
			struct OSVersionInfo {
				uint32_t infoSize = 0;
				uint32_t majorVersion = 0;
				uint32_t minorVersion = 0;
				uint32_t buildNumber = 0;
				uint32_t platformID = 0;
			} osVersionInfo;
		} computerInfo;

		LoginRequest() : LUBitStream(ServiceType::AUTH, MessageType::Auth::LOGIN_REQUEST) {}
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;
	};

	// Non struct functions
	void LoadClaimCodes();
	void Handle(RakNet::BitStream& inStream, const SystemAddress& sysAddr);

}

#endif // AUTHPACKETS_H
