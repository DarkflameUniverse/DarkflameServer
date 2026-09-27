#ifndef __BITSTREAMUTILS__H__
#define __BITSTREAMUTILS__H__

#include "GeneralUtils.h"
#include "BitStream.h"
#include "MessageIdentifiers.h"
#include "ServiceType.h"
#include <string>
#include <algorithm>
#include <type_traits>

#define VALIDATE_READ(x) do { if (!x) return false; } while (0)

struct LUString {
	std::string string;
	uint32_t size;

	LUString(uint32_t size = 33) {
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

	LUWString(uint32_t size = 33) {
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

struct LUBitStream {
	ServiceType connectionType = ServiceType::UNKNOWN;
	uint32_t internalPacketID = 0xFFFFFFFF;

	LUBitStream() = default;
	virtual ~LUBitStream() = default;

	template <typename T> 
	LUBitStream(ServiceType connectionType, T internalPacketID) {
		this->connectionType = connectionType;
		this->internalPacketID = static_cast<uint32_t>(internalPacketID);
	}

	void WriteHeader(RakNet::BitStream& bitStream) const;
	bool ReadHeader(RakNet::BitStream& bitStream);
	void Send(const SystemAddress& sysAddr) const;
	void Broadcast() const { Send(UNASSIGNED_SYSTEM_ADDRESS); };

	// Writes the complete packet (WriteHeader() then Serialize()) into bitStream.
	// This is exactly what Send puts on the wire; tests use it to compare bytes without a server.
	void WritePacket(RakNet::BitStream& bitStream) const;

	virtual void Serialize(RakNet::BitStream& bitStream) const {}
	virtual bool Deserialize(RakNet::BitStream& bitStream) { return true; }
	virtual void Handle() {};
};


namespace BitStreamUtils {
	/**
	 * Writes an optional ("default flag") field: one bit saying whether value differs from defaultValue, then
	 * the value itself only if it does. This is how the client encodes game message parameters that have a default.
	 */
	template<typename T>
	void WriteOptional(RakNet::BitStream& bitStream, const T& value, const T& defaultValue) {
		const bool isNotDefault = value != defaultValue;
		bitStream.Write(isNotDefault);
		if (isNotDefault) bitStream.Write(value);
	}

	/**
	 * Reads a field written by WriteOptional. If the flag bit is not set, value is set to defaultValue.
	 */
	template<typename T>
	bool ReadOptional(RakNet::BitStream& bitStream, T& value, const T& defaultValue) {
		bool isNotDefault = false;
		if (!bitStream.Read(isNotDefault)) return false;
		if (!isNotDefault) {
			value = defaultValue;
			return true;
		}
		return bitStream.Read(value);
	}

	/**
	 * Writes a length prefixed string: a LenT holding the number of characters, followed by the raw
	 * characters (1 byte each for std::string, 2 bytes each for std::u16string) with no null terminator.
	 * This is the layout the client uses for std::string / std::wstring game message fields.
	 */
	template<typename LenT = uint32_t, typename StringT>
	void WriteLengthPrefixed(RakNet::BitStream& bitStream, const StringT& value) {
		bitStream.Write<LenT>(static_cast<LenT>(value.size()));
		bitStream.WriteBits(reinterpret_cast<const unsigned char*>(value.data()), BYTES_TO_BITS(value.size() * sizeof(typename StringT::value_type)));
	}

	/**
	 * Reads a string written by WriteLengthPrefixed. Fails (returns false) if the stream runs out of data or
	 * if the length is negative or larger than maxLength characters.
	 */
	/**
	 * Writes name-value (LDF) text the way the client reads it: a u32 character count, the characters, and a null
	 * terminator (not counted) when the text isn't empty.
	 */
	inline void WriteNameValueText(RakNet::BitStream& bitStream, const std::u16string& text) {
		WriteLengthPrefixed<uint32_t>(bitStream, text);
		if (!text.empty()) bitStream.Write<uint16_t>(0);
	}

	/**
	 * Reads text written by WriteNameValueText (the null terminator is consumed and not kept).
	 */
	inline bool ReadNameValueText(RakNet::BitStream& bitStream, std::u16string& text, const uint32_t maxLength = 0x500000 /* MAX_MESSAGE_LENGTH */);

	template<typename LenT = uint32_t, typename StringT>
	bool ReadLengthPrefixed(RakNet::BitStream& bitStream, StringT& value, const uint32_t maxLength = 0x500000 /* MAX_MESSAGE_LENGTH */) {
		LenT length{};
		if (!bitStream.Read(length)) return false;
		if constexpr (std::is_signed_v<LenT>) {
			if (length < 0) return false;
		}
		if (static_cast<uint64_t>(length) > maxLength) return false;
		value.resize(length);
		if (length == 0) return true;
		return bitStream.ReadBits(reinterpret_cast<unsigned char*>(value.data()), BYTES_TO_BITS(value.size() * sizeof(typename StringT::value_type)), true);
	}

	inline bool ReadNameValueText(RakNet::BitStream& bitStream, std::u16string& text, const uint32_t maxLength) {
		if (!ReadLengthPrefixed<uint32_t>(bitStream, text, maxLength)) return false;
		uint16_t terminator{};
		return text.empty() || bitStream.Read(terminator);
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
