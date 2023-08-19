#ifndef __BITSTREAMUTILS__H__
#define __BITSTREAMUTILS__H__

#include "GeneralUtils.h"
#include <MessageIdentifiers.h>
#include <BitStream.h>
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
	template <>
	inline bool RakNet::BitStream::Read<LUString>(LUString& value) {
		bool noMoreLetters = false;
		char character;
		for (uint32_t j = 0; j < value.size; j++) {
			if (!Read(character)) return false;
			if (character == '\0') noMoreLetters = true;
			if (!noMoreLetters) value.string.push_back(character);
		}
		return true;
	}

	template <>
	inline bool RakNet::BitStream::Read<LUWString>(LUWString& value) {
		bool noMoreLetters = false;
		char16_t character;
		for (uint32_t j = 0; j < value.size; j++) {
			if (!Read(character)) return false;
			if (character == '\0') noMoreLetters = true;
			if (!noMoreLetters) value.string.push_back(character);
		}
		return true;
	}

	template <>
	inline void RakNet::BitStream::Write<LUString>(LUString value) {
		uint32_t size = value.string.size();
		uint32_t emptySize = value.size - size;

		if (size > value.size) size = value.size;

		for (uint32_t i = 0; i < size; i++) {
			this->Write(static_cast<char>(value.string[i]));
		}

		for (uint32_t i = 0; i < emptySize; i++) {
			this->Write(static_cast<char>(0));
		}
	}

	template <>
	inline void RakNet::BitStream::Write<LUWString>(LUWString value) {
		uint32_t size = static_cast<uint32_t>(value.string.length());
		uint32_t remSize = static_cast<uint32_t>(value.size - size);

		if (size > value.size) size = value.size;

		for (uint32_t i = 0; i < size; ++i) {
			this->Write(static_cast<uint16_t>(value.string[i]));
		}

		for (uint32_t j = 0; j < remSize; ++j) {
			this->Write(static_cast<uint16_t>(0));
		}
	}

	template <>
	inline void RakNet::BitStream::Write<std::string>(std::string value) {
		this->WriteBits(reinterpret_cast<const unsigned char*>(value.data()), BYTES_TO_BITS(value.size()));
	}

	template <>
	inline void RakNet::BitStream::Write<std::u16string>(std::u16string value) {
		this->WriteBits(reinterpret_cast<const unsigned char*>(value.data()), BYTES_TO_BITS(value.size()) * 2);
	}
};

#endif  //!__BITSTREAMUTILS__H__
