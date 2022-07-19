#include "dCommonVars.h"
#include "WorldPackets.h"
#include "BitStream.h"
#include "dMessageIdentifiers.h"
#include "PacketUtils.h"
#include "GeneralUtils.h"
#include "User.h"
#include "Character.h"
#include "dLogger.h"
#include <iostream>
#include "Game.h"
#include "LDFFormat.h"
#include "dServer.h"
#include "dZoneManager.h"
#include "CharacterComponent.h"
#include "ZCompression.h"

void WorldPackets::SendLoadStaticZone(const SystemAddress& sysAddr, float x, float y, float z, uint32_t checksum) {
    RakNet::BitStream bitStream;
    PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_LOAD_STATIC_ZONE);
    
	auto zone = dZoneManager::Instance()->GetZone()->GetZoneID();
	bitStream.Write(static_cast<uint16_t>(zone.GetMapID()));
	bitStream.Write(static_cast<uint16_t>(zone.GetInstanceID()));
	//bitStream.Write(static_cast<uint32_t>(zone.GetCloneID()));
	bitStream.Write(0);

    bitStream.Write(checksum);
    bitStream.Write(static_cast<uint16_t>(0));     // ??

    bitStream.Write(x);
    bitStream.Write(y);
    bitStream.Write(z);

    bitStream.Write(static_cast<uint32_t>(0));     // Change this to eventually use 4 on activity worlds
    
    SEND_PACKET
}

void WorldPackets::SendCharacterList ( const SystemAddress& sysAddr, User* user ) {
	if (!user) return;

	RakNet::BitStream bitStream;
	PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_CHARACTER_LIST_RESPONSE);
	
	std::vector<Character*> characters = user->GetCharacters();
	bitStream.Write(static_cast<uint8_t>(characters.size()));
    bitStream.Write(static_cast<uint8_t>(0)); //character index in front, just picking 0
	
	for (uint32_t i = 0; i < characters.size(); ++i) {
		bitStream.Write(characters[i]->GetObjectID());
        bitStream.Write(static_cast<uint32_t>(0));
		
		PacketUtils::WriteWString(bitStream, characters[i]->GetName(), 33);
        PacketUtils::WriteWString(bitStream, characters[i]->GetUnapprovedName(), 33);
		
		bitStream.Write(static_cast<uint8_t>(characters[i]->GetNameRejected()));
        bitStream.Write(static_cast<uint8_t>(false));
		
		PacketUtils::WriteString(bitStream, "", 10);
		
		bitStream.Write(characters[i]->GetShirtColor());
        bitStream.Write(characters[i]->GetShirtStyle());
        bitStream.Write(characters[i]->GetPantsColor());
        bitStream.Write(characters[i]->GetHairStyle());
        bitStream.Write(characters[i]->GetHairColor());
        bitStream.Write(characters[i]->GetLeftHand());
        bitStream.Write(characters[i]->GetRightHand());
        bitStream.Write(characters[i]->GetEyebrows());
        bitStream.Write(characters[i]->GetEyes());
        bitStream.Write(characters[i]->GetMouth());
        bitStream.Write(static_cast<uint32_t>(0));

        bitStream.Write(static_cast<uint16_t>(characters[i]->GetZoneID()));
        bitStream.Write(static_cast<uint16_t>(characters[i]->GetZoneInstance()));
        bitStream.Write(characters[i]->GetZoneClone());

        bitStream.Write(characters[i]->GetLastLogin());

        const auto& equippedItems = characters[i]->GetEquippedItems();
        bitStream.Write(static_cast<uint16_t>(equippedItems.size()));
        
        for (uint32_t j = 0; j < equippedItems.size(); ++j) {
            bitStream.Write(equippedItems[j]);
        }
	}
	
	SEND_PACKET
}

void WorldPackets::SendCharacterCreationResponse ( const SystemAddress& sysAddr, eCreationResponse response ) {
	RakNet::BitStream bitStream;
	PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_CHARACTER_CREATE_RESPONSE);
	bitStream.Write(response);
	SEND_PACKET
}

void WorldPackets::SendCharacterRenameResponse ( const SystemAddress& sysAddr, eRenameResponse response ) {
	RakNet::BitStream bitStream;
	PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_CHARACTER_RENAME_RESPONSE);
	bitStream.Write(response);
	SEND_PACKET
}

void WorldPackets::SendCharacterDeleteResponse(const SystemAddress& sysAddr, bool response) {
    RakNet::BitStream bitStream;
    PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_DELETE_CHARACTER_RESPONSE);
    bitStream.Write(static_cast<uint8_t>(response));
    SEND_PACKET
}

void WorldPackets::SendTransferToWorld ( const SystemAddress& sysAddr, const std::string& serverIP, uint32_t serverPort, bool mythranShift ) {
	RakNet::BitStream bitStream;
	PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_TRANSFER_TO_WORLD);
	
	PacketUtils::WriteString(bitStream, serverIP, 33);
	bitStream.Write(static_cast<uint16_t>(serverPort));
	bitStream.Write(static_cast<uint8_t>(mythranShift));
	
	SEND_PACKET
}

void WorldPackets::SendServerState ( const SystemAddress& sysAddr ) {
	RakNet::BitStream bitStream;
	PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_SERVER_STATES);
	bitStream.Write(static_cast<uint8_t>(1)); //If the server is receiving this request, it probably is ready anyway.
	SEND_PACKET
}

void WorldPackets::SendCreateCharacter(const SystemAddress& sysAddr, Entity* entity, const std::string& xmlData, const std::u16string& username, int32_t gm) {
    RakNet::BitStream bitStream;
    PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_CREATE_CHARACTER);
    
    RakNet::BitStream data;
    data.Write<uint32_t>(7); //LDF key count

    auto character = entity->GetComponent<CharacterComponent>();
    if (!character) {
        Game::logger->Log("WorldPackets", "Entity is not a character?? what??");
        return;
    }

    LDFData<LWOOBJID>* objid = new LDFData<LWOOBJID>(u"objid", entity->GetObjectID());
    LDFData<LOT>* lot = new LDFData<LOT>(u"template", 1);
    LDFData<std::string> * xmlConfigData = new LDFData<std::string>(u"xmlData", xmlData);
    LDFData<std::u16string>* name = new LDFData<std::u16string>(u"name", username);
    LDFData<int32_t>* gmlevel = new LDFData<int32_t>(u"gmlevel", gm);
    LDFData<int32_t>* chatmode = new LDFData<int32_t>(u"chatmode", gm);
    LDFData<int64_t>* reputation = new LDFData<int64_t>(u"reputation", character->GetReputation());

    objid->WriteToPacket(&data);
    lot->WriteToPacket(&data);
    name->WriteToPacket(&data);
    gmlevel->WriteToPacket(&data);
    chatmode->WriteToPacket(&data);
    xmlConfigData->WriteToPacket(&data);
    reputation->WriteToPacket(&data);
    
    delete objid;
    delete lot;
    delete xmlConfigData;
    delete gmlevel;
    delete chatmode;
    delete name;
    delete reputation;

#ifdef _WIN32
    bitStream.Write<uint32_t>(data.GetNumberOfBytesUsed() + 1);
    bitStream.Write<uint8_t>(0);
    bitStream.Write((char*)data.GetData(), data.GetNumberOfBytesUsed());
#else 
    //Compress the data before sending:
    const int reservedSize = 5 * 1024 * 1024;
    uint8_t compressedData[reservedSize];
    size_t size = ZCompression::Compress(data.GetData(), data.GetNumberOfBytesUsed(), compressedData, reservedSize);

    bitStream.Write<uint32_t>(size + 9); //size of data + header bytes (8)
    bitStream.Write<uint8_t>(1);         //compressed boolean, true
    bitStream.Write<uint32_t>(data.GetNumberOfBytesUsed());
    bitStream.Write<uint32_t>(size);

    for (size_t i = 0; i < size; i++)
        bitStream.Write(compressedData[i]);
#endif

    PacketUtils::SavePacket("chardata.bin", (const char *)bitStream.GetData(), static_cast<uint32_t>(bitStream.GetNumberOfBytesUsed()));
    SEND_PACKET
    Game::logger->Log("WorldPackets", "Sent CreateCharacter for ID: %llu\n", entity->GetObjectID());
}

void WorldPackets::SendChatModerationResponse(const SystemAddress& sysAddr, bool requestAccepted, uint32_t requestID, const std::string& receiver, std::vector<std::pair<uint8_t, uint8_t>> unacceptedItems) {
    CBITSTREAM
    PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_CHAT_MODERATION_STRING);

    bitStream.Write<uint8_t>(unacceptedItems.empty()); // Is sentence ok?
    bitStream.Write<uint16_t>(0x16); // Source ID, unknown

    bitStream.Write(static_cast<uint8_t>(requestID)); // request ID
    bitStream.Write(static_cast<char>(0)); // chat mode

    PacketUtils::WritePacketWString(receiver, 42, &bitStream); // receiver name

    for (auto it : unacceptedItems) {
        bitStream.Write<uint8_t>(it.first); // start index
        bitStream.Write<uint8_t>(it.second); // length
    }

    for (int i = unacceptedItems.size(); 64 > i; i++) {
        bitStream.Write<uint16_t>(0);
    }

    SEND_PACKET
}

void WorldPackets::SendGMLevelChange(const SystemAddress& sysAddr, bool success, uint8_t highestLevel, uint8_t prevLevel, uint8_t newLevel) {
    CBITSTREAM
    PacketUtils::WriteHeader(bitStream, CLIENT, MSG_CLIENT_MAKE_GM_RESPONSE);
    
    bitStream.Write<uint8_t>(success);
    bitStream.Write<uint16_t>(highestLevel);
    bitStream.Write<uint16_t>(prevLevel);
    bitStream.Write<uint16_t>(newLevel);
    
    SEND_PACKET
}
