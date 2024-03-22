#ifndef WORLDPACKETS_H
#define WORLDPACKETS_H

#include "dCommonVars.h"
#include <vector>
#include <string>

class User;
struct SystemAddress;
enum class eGameMasterLevel : uint8_t;
enum class eCharacterCreationResponse : uint8_t;
enum class eRenameResponse : uint8_t;

namespace RakNet {
	class BitStream;
};

#pragma pack(push, 1)
struct HTTPMonitorInfo {
	uint16_t port = 80;
	bool openWeb = false;
	bool supportsSum = false;
	bool supportsDetail = false;
	bool supportsWho = false;
	bool supportsObjects = false;
	void Serialize(RakNet::BitStream &bitstream) const;
};
#pragma pack(pop)

namespace WorldPackets {
	void SendLoadStaticZone(const SystemAddress& sysAddr, float x, float y, float z, uint32_t checksum, LWOZONEID zone);
	void SendCharacterCreationResponse(const SystemAddress& sysAddr, eCharacterCreationResponse response);
	void SendCharacterRenameResponse(const SystemAddress& sysAddr, eRenameResponse response);
	void SendCharacterDeleteResponse(const SystemAddress& sysAddr, bool response);
	void SendTransferToWorld(const SystemAddress& sysAddr, const std::string& serverIP, uint32_t serverPort, bool mythranShift);
	void SendServerState(const SystemAddress& sysAddr);
	void SendCreateCharacter(const SystemAddress& sysAddr, int64_t reputation, LWOOBJID player, const std::string& xmlData, const std::u16string& username, eGameMasterLevel gm);
	void SendChatModerationResponse(const SystemAddress& sysAddr, bool requestAccepted, uint32_t requestID, const std::string& receiver, std::vector<std::pair<uint8_t, uint8_t>> unacceptedItems);
	void SendGMLevelChange(const SystemAddress& sysAddr, bool success, eGameMasterLevel highestLevel, eGameMasterLevel prevLevel, eGameMasterLevel newLevel);
	void SendDebugOuput(const SystemAddress& sysAddr, const std::string data);
	void SendHTTPMonitorInfo(const SystemAddress& sysAddr, const HTTPMonitorInfo info);
}

#endif // WORLDPACKETS_H
