#include "AMFDeserialize.h"

#include <stdexcept>

#include "Amf3.h"

/**
 * AMF3 Reference document https://rtmp.veriskope.com/pdf/amf3-file-format-spec.pdf
 * AMF3 Deserializer written by EmosewaMC
 */

AMFBaseValue* AMFDeserialize::Read(RakNet::BitStream* inStream) {
	if (!inStream) return nullptr;
	AMFBaseValue* returnValue = nullptr;
	// Read in the value type from the bitStream
	eAmf marker;
	inStream->Read(marker);
	// Based on the typing, create the value associated with that and return the base value class
	switch (marker) {
	case eAmf::Undefined: {
		returnValue = new AMFBaseValue();
		break;
	}

	case eAmf::Null: {
		returnValue = new AMFNullValue();
		break;
	}

	case eAmf::False: {
		returnValue = new AMFBoolValue(false);
		break;
	}

	case eAmf::True: {
		returnValue = new AMFBoolValue(true);
		break;
	}

	case eAmf::Integer: {
		returnValue = ReadAmfInteger(inStream);
		break;
	}

	case eAmf::Double: {
		returnValue = ReadAmfDouble(inStream);
		break;
	}

	case eAmf::String: {
		returnValue = ReadAmfString(inStream);
		break;
	}

	case eAmf::Array: {
		returnValue = ReadAmfArray(inStream);
		break;
	}

	// These values are unimplemented in the live client and will remain unimplemented
	// unless someone modifies the client to allow serializing of these values.
	case eAmf::XMLDoc:
	case eAmf::Date:
	case eAmf::Object:
	case eAmf::XML:
	case eAmf::ByteArray:
	case eAmf::VectorInt:
	case eAmf::VectorUInt:
	case eAmf::VectorDouble:
	case eAmf::VectorObject:
	case eAmf::Dictionary: {
		throw marker;
		break;
	}
	default:
		throw std::invalid_argument("Invalid AMF3 marker" + std::to_string(static_cast<int32_t>(marker)));
		break;
	}
	return returnValue;
}

uint32_t AMFDeserialize::ReadU29(RakNet::BitStream* inStream) {
	bool byteFlag = true;
	uint32_t actualNumber{};
	uint8_t numberOfBytesRead{};
	while (byteFlag && numberOfBytesRead < 4) {
		uint8_t byte{};
		inStream->Read(byte);
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

const std::string AMFDeserialize::ReadString(RakNet::BitStream* inStream) {
	auto length = ReadU29(inStream);
	// Check if this is a reference
	bool isReference = length % 2 == 1;
	// Right shift by 1 bit to get index if reference or size of next string if value
	length = length >> 1;
	if (isReference) {
		std::string value(length, 0);
		inStream->Read(&value[0], length);
		// Empty strings are never sent by reference
		if (!value.empty()) accessedElements.push_back(value);
		return value;
	} else {
		// Length is a reference to a previous index - use that as the read in value
		return accessedElements.at(length);
	}
}

AMFBaseValue* AMFDeserialize::ReadAmfDouble(RakNet::BitStream* inStream) {
	double value;
	inStream->Read<double>(value);
	return new AMFDoubleValue(value);
}

AMFBaseValue* AMFDeserialize::ReadAmfArray(RakNet::BitStream* inStream) {
	auto arrayValue = new AMFArrayValue();

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

AMFBaseValue* AMFDeserialize::ReadAmfString(RakNet::BitStream* inStream) {
	return new AMFStringValue(ReadString(inStream));
}

AMFBaseValue* AMFDeserialize::ReadAmfInteger(RakNet::BitStream* inStream) {
	return new AMFIntValue(ReadU29(inStream));
}
