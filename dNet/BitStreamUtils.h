#ifndef __BITSTREAMUTILS__H__
#define __BITSTREAMUTILS__H__

#include "GeneralUtils.h"
#include "MessageIdentifiers.h"
#include "BitStream.h"
#include <string>

enum class eConnectionType : uint16_t;

struct LUString {
	std::string string;
	uint32_t size;

	LUString(uint32_t size) {
		this->size = size;
	};
	LUString(std::string string, uint32_t size = 33) {
		this->string = string;
		this->size = size;
	};
	std::u16string GetAsU16String() const {
		return GeneralUtils::ASCIIToUTF16(this->string);
	};
};

struct LUWString {
	std::u16string string;
	uint32_t size;

	LUWString(uint32_t size) {
		this->size = size;
	};
	LUWString(std::u16string string, uint32_t size = 33) {
		this->string = string;
		this->size = size;
	};
	LUWString(std::string string, uint32_t size = 33) {
		this->string = GeneralUtils::ASCIIToUTF16(string);
		this->size = size;
	};
	std::string GetAsString() const {
		return GeneralUtils::UTF16ToWTF8(this->string);
	};
};

namespace BitStreamUtils {
	template<typename T>
	void WriteHeader(RakNet::BitStream& bitStream, eConnectionType connectionType, T internalPacketID) {
		bitStream.Write<uint8_t>(MessageID(ID_USER_PACKET_ENUM));
		bitStream.Write<eConnectionType>(connectionType);
		bitStream.Write<uint32_t>(static_cast<uint32_t>(internalPacketID));
		bitStream.Write<uint8_t>(0);
	}

}

namespace RakNet {
#ifndef __BITSTREAM_NATIVE_END
#error No definition for big endian reading of LUString
#endif

	template <>
	inline bool RakNet::BitStream::Read<LUString>(LUString& value) {
		value.string.resize(value.size);
		bool res = ReadBits(reinterpret_cast<unsigned char*>(value.string.data()), BYTES_TO_BITS(value.string.size()), true);
		if (!res) return false;
		value.string.erase(std::find(value.string.begin(), value.string.end(), '\0'), value.string.end());
		return res;
	}

	template <>
	inline bool RakNet::BitStream::Read<LUWString>(LUWString& value) {
		value.string.resize(value.size);
		bool res = ReadBits(reinterpret_cast<unsigned char*>(value.string.data()), BYTES_TO_BITS(value.string.size()) * sizeof(std::u16string::value_type), true);
		if (!res) return false;
		value.string.erase(std::find(value.string.begin(), value.string.end(), u'\0'), value.string.end());
		return res;
	}

	template <>
	inline void RakNet::BitStream::Write<std::string>(std::string value) {
		this->WriteBits(reinterpret_cast<const unsigned char*>(value.data()), BYTES_TO_BITS(value.size()));
	}

	template <>
	inline void RakNet::BitStream::Write<std::u16string>(std::u16string value) {
		this->WriteBits(reinterpret_cast<const unsigned char*>(value.data()), BYTES_TO_BITS(value.size()) * sizeof(std::u16string::value_type));
	}

	template <>
	inline void RakNet::BitStream::Write<LUString>(LUString value) {
		value.string.resize(value.size);
		this->Write(value.string);
	}

	template <>
	inline void RakNet::BitStream::Write<LUWString>(LUWString value) {
		value.string.resize(value.size);
		this->Write(value.string);
	}
};

#endif  //!__BITSTREAMUTILS__H__
