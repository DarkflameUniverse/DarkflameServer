/*
 * Darkflame Universe
 * Copyright 2018
 */

#include "ClientPackets.h"
#include "dCommonVars.h"
#include "LDFFormat.h"
#include "ZCompression.h"

namespace ClientPackets {
	void LoadStaticZone::Serialize(RakNet::BitStream &bitStream) const {
		bitStream.Write<uint16_t>(zoneID.GetMapID());
		bitStream.Write<uint16_t>(zoneID.GetInstanceID());
		bitStream.Write<uint32_t>(zoneID.GetCloneID());
		bitStream.Write<uint32_t>(checksum);
		bitStream.Write<uint8_t>(editorEnabled);
		bitStream.Write<uint8_t>(editorLevel);
		bitStream.Write(position.x);
		bitStream.Write(position.y);
		bitStream.Write(position.z);
		bitStream.Write<uint32_t>(instanceType);
	}
	void CharacterCreationResponse::Serialize(RakNet::BitStream &bitStream) const {
		bitStream.Write(response);
	}
	
	void CharacterRenameResponse::Serialize(RakNet::BitStream &bitStream) const {
		bitStream.Write(response);
	}

	void CharacterDeleteResponse::Serialize(RakNet::BitStream &bitStream) const {
		bitStream.Write<uint8_t>(success);
	}

	void TransferToWorld::Serialize(RakNet::BitStream &bitStream) const {
		bitStream.Write(LUString(serverIP));
		bitStream.Write<uint16_t>(serverPort);
		bitStream.Write<uint8_t>(mythranShift);
	}

	void ServerState::Serialize(RakNet::BitStream &bitStream) const {
		bitStream.Write<uint8_t>(serverReady);
	}
	void CreateCharacter::Serialize(RakNet::BitStream &bitStream) const {

		RakNet::BitStream data;

		data.Write<uint32_t>(7); //LDF key count
		LDFData<LWOOBJID>(u"objid", objid).WriteToPacket(data);
		LDFData<LOT>(u"template", templateID).WriteToPacket(data);;
		LDFData<std::u16string>(u"name", name).WriteToPacket(data);;
		LDFData<int32_t>(u"gmlevel", static_cast<int32_t>(gmLevel)).WriteToPacket(data);;
		LDFData<int32_t>(u"chatmode", static_cast<int32_t>(chatMode)).WriteToPacket(data);;
		LDFData<std::string>(u"xmlData", xmlData).WriteToPacket(data);;
		LDFData<int64_t>(u"reputation", reputation).WriteToPacket(data);;
		//Compress the data before sending:
		const uint32_t reservedSize = ZCompression::GetMaxCompressedLength(data.GetNumberOfBytesUsed());
		uint8_t* compressedData = new uint8_t[reservedSize];

		if (!compressedData) {
			throw std::runtime_error("Failed to allocate memory for compressed data");
		}

		size_t size = ZCompression::Compress(data.GetData(), data.GetNumberOfBytesUsed(), compressedData, reservedSize);

		assert(size <= reservedSize);

		bitStream.Write<uint32_t>(size + 9); //size of data + header bytes (8)
		bitStream.Write<uint8_t>(1);         //compressed boolean, true
		bitStream.Write<uint32_t>(data.GetNumberOfBytesUsed());
		bitStream.Write<uint32_t>(size);

		/**
		 * In practice, this warning serves no purpose for us.  We allocate the max memory needed on the heap
		 * and then compress the data.  In the off chance that the compression actually increases the size,
		 * an assertion is done to prevent bad data from being saved or sent.
		 */
	#pragma warning(disable:6385) // C6385 Reading invalid data from 'compressedData'.
		bitStream.WriteAlignedBytes(compressedData, size);
	#pragma warning(default:6385)
		delete[] compressedData;
	};

	void ChatModerationString::Serialize(RakNet::BitStream &bitStream) const {
		bitStream.Write<uint8_t>(rejectedWords.empty()); // Accepted
		bitStream.Write<uint16_t>(0); // padding

		bitStream.Write(chatChannel);
		bitStream.Write(chatMode);

		bitStream.Write(LUWString(receiver, 42));

		for (auto it : rejectedWords) {
			bitStream.Write<uint8_t>(it.first); // start index
			bitStream.Write<uint8_t>(it.second); // length
		}

		// Pad out the rest of the packet
		// The client expects 64 items, so we need to write 64 - rejectedWords.size() empty items
		for (int i = rejectedWords.size(); 64 > i; i++) {
			bitStream.Write<uint16_t>(0);
		}

	}

	void GMLevelChange::Serialize(RakNet::BitStream &bitStream) const {
		bitStream.Write<uint8_t>(success);
		bitStream.Write(static_cast<uint16_t>(highestLevel));
		bitStream.Write(static_cast<uint16_t>(prevLevel));
		bitStream.Write(static_cast<uint16_t>(newLevel));
	}

	void DebugOutput::Serialize(RakNet::BitStream &bitStream) const {
		bitStream.Write<uint32_t>(data.size());
		bitStream.Write(data);
	}

	void HTTPMonitorInfoResponse::Serialize(RakNet::BitStream &bitStream) const {
		bitStream.Write<uint16_t>(port);
		bitStream.Write<uint8_t>(openWeb);
		bitStream.Write<uint8_t>(supportsSum);
		bitStream.Write<uint8_t>(supportsDetail);
		bitStream.Write<uint8_t>(supportsWho);
		bitStream.Write<uint8_t>(supportsObjects);
	}
}
