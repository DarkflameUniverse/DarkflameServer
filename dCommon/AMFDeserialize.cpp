#include "AMFDeserialize.h"

#include <stdexcept>

#include "Amf3.h"

/**
 * AMF3 Reference document https://rtmp.veriskope.com/pdf/amf3-file-format-spec.pdf
 * AMF3 Deserializer written by EmosewaMC
 */

std::unique_ptr<AMFBaseValue> AMFDeserialize::Read(RakNet::BitStream& inStream) {
	// Read in the value type from the bitStream
	eAmf marker;
	inStream.Read(marker);
	// Based on the typing, create the value associated with that and return the base value class
	switch (marker) {
	case eAmf::Undefined:
		return std::make_unique<AMFBaseValue>();
	case eAmf::Null:
		return std::make_unique<AMFNullValue>();
	case eAmf::False:
		return std::make_unique<AMFBoolValue>(false);
	case eAmf::True:
		return std::make_unique<AMFBoolValue>(true);
	case eAmf::Integer:
		return ReadAmfInteger(inStream);
	case eAmf::Double:
		return ReadAmfDouble(inStream);
	case eAmf::String:
		return ReadAmfString(inStream);
	case eAmf::Array:
		return ReadAmfArray(inStream);

	// These values are unimplemented in the live client and will remain unimplemented
	// unless someone modifies the client to allow serializing of these values.
	case eAmf::XMLDoc:
		[[fallthrough]];
	case eAmf::Date:
		[[fallthrough]];
	case eAmf::Object:
		[[fallthrough]];
	case eAmf::XML:
		[[fallthrough]];
	case eAmf::ByteArray:
		[[fallthrough]];
	case eAmf::VectorInt:
		[[fallthrough]];
	case eAmf::VectorUInt:
		[[fallthrough]];
	case eAmf::VectorDouble:
		[[fallthrough]];
	case eAmf::VectorObject:
		[[fallthrough]];
	case eAmf::Dictionary:
		throw marker;
	default:
		throw std::invalid_argument("Invalid AMF3 marker" + std::to_string(static_cast<int32_t>(marker)));
	}
}

uint32_t AMFDeserialize::ReadU29(RakNet::BitStream& inStream) {
	bool byteFlag = true;
	uint32_t actualNumber{};
	uint8_t numberOfBytesRead{};
	while (byteFlag && numberOfBytesRead < 4) {
		uint8_t byte{};
		inStream.Read(byte);
		// Parse the byte
		if (numberOfBytesRead < 3) {
			byteFlag = byte & static_cast<uint8_t>(1 << 7);
			byte = byte << 1UL;
		}
		// Combine the read byte with our current read in number
		actualNumber <<= 8UL;
		actualNumber |= static_cast<uint32_t>(byte);
		// If we are not done reading in bytes, shift right 1 bit
		if (numberOfBytesRead < 3) actualNumber = actualNumber >> 1UL;
		numberOfBytesRead++;
	}
	return actualNumber;
}

const std::string AMFDeserialize::ReadString(RakNet::BitStream& inStream) {
	auto length = ReadU29(inStream);
	// Check if this is a reference
	bool isReference = length % 2 == 1;
	// Right shift by 1 bit to get index if reference or size of next string if value
	length = length >> 1;
	if (isReference) {
		std::string value(length, 0);
		inStream.Read(&value[0], length);
		// Empty strings are never sent by reference
		if (!value.empty()) accessedElements.push_back(value);
		return value;
	} else {
		// Length is a reference to a previous index - use that as the read in value
		return accessedElements.at(length);
	}
}

std::unique_ptr<AMFDoubleValue> AMFDeserialize::ReadAmfDouble(RakNet::BitStream& inStream) {
	double value;
	inStream.Read<double>(value);
	return std::make_unique<AMFDoubleValue>(value);
}

std::unique_ptr<AMFArrayValue> AMFDeserialize::ReadAmfArray(RakNet::BitStream& inStream) {
	auto arrayValue = std::make_unique<AMFArrayValue>();

	// Read size of dense array
	const auto sizeOfDenseArray = (ReadU29(inStream) >> 1);
	// Then read associative portion
	while (true) {
		const auto key = ReadString(inStream);
		// No more associative values when we encounter an empty string key
		if (key.size() == 0) break;
		arrayValue->Insert(key, Read(inStream));
	}
	// Finally read dense portion
	for (uint32_t i = 0; i < sizeOfDenseArray; i++) {
		arrayValue->Insert(i, Read(inStream));
	}
	return arrayValue;
}

std::unique_ptr<AMFStringValue> AMFDeserialize::ReadAmfString(RakNet::BitStream& inStream) {
	return std::make_unique<AMFStringValue>(ReadString(inStream));
}

std::unique_ptr<AMFIntValue> AMFDeserialize::ReadAmfInteger(RakNet::BitStream& inStream) {
	return std::make_unique<AMFIntValue>(ReadU29(inStream)); // NOTE: NARROWING CONVERSION FROM UINT TO INT. IS THIS INTENDED?
}
