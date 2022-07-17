#include "AMFDeserialize.h"

#include "AMFFormat.h"

/**
 * AMF3 Reference document https://rtmp.veriskope.com/pdf/amf3-file-format-spec.pdf
 * I have only written the values that the LEGO Universe client sends to the server.  All others are left out.
 * AMF3 Deserializer written by EmosewaMC
 */
AMFValue* AMFDeserialize::Read(RakNet::BitStream* inStream) {
	if (!inStream) return nullptr;
	AMFValue* returnValue = nullptr;
	// Read in the value type from the bitStream
	int8_t valueType;
	inStream->Read(valueType);
	// Based on the typing, read a different value
	switch (valueType) {
		case AMFValueType::AMFUndefined: {
			returnValue = new AMFUndefinedValue();
			break;
		}

		case AMFValueType::AMFNull: {
			returnValue = new AMFNullValue();
			break;
		}

		case AMFValueType::AMFFalse: {
			returnValue = new AMFFalseValue();
			break;
		}

		case AMFValueType::AMFTrue: {
			returnValue = new AMFTrueValue();
			break;
		}

		case AMFValueType::AMFInteger: {
			returnValue = new AMFIntegerValue();
			static_cast<AMFIntegerValue*>(returnValue)->SetIntegerValue(ReadU29(inStream));
			break;
		}

		case AMFValueType::AMFDouble: {
			returnValue = new AMFDoubleValue();
			double value;
			inStream->Read<double>(value);
			static_cast<AMFDoubleValue*>(returnValue)->SetDoubleValue(value);
			break;
		}

		case AMFValueType::AMFString: {
			returnValue = new AMFStringValue();
			static_cast<AMFStringValue*>(returnValue)->SetStringValue(ReadString(inStream));
			break;
		}

		case AMFValueType::AMFArray: {
			returnValue = new AMFArrayValue();
			auto sizeOfDenseArray = (ReadU29(inStream) >> 1);
			if (sizeOfDenseArray >= 1) {
				char spacing;
				inStream->Read(spacing); // idk
				for (uint32_t i = 0; i < sizeOfDenseArray; i++) {
					auto res = Read(inStream);
					static_cast<AMFArrayValue*>(returnValue)->PushBackValue(res);
				}
			} else {
				while (true) {
					auto key = ReadString(inStream);
					if (key.size() == 0) break;
					static_cast<AMFArrayValue*>(returnValue)->InsertValue(key, Read(inStream));
				}
			}
			break;
		}

		// Don't think I need these for now.  Will log if I need them.
		case AMFValueType::AMFXMLDoc:
		case AMFValueType::AMFDate:
		case AMFValueType::AMFObject:
		case AMFValueType::AMFXML:
		case AMFValueType::AMFByteArray:
		case AMFValueType::AMFVectorInt:
		case AMFValueType::AMFVectorUInt:
		case AMFValueType::AMFVectorDouble:
		case AMFValueType::AMFVectorObject:
		case AMFValueType::AMFDictionary: {
			throw static_cast<AMFValueType>(valueType);
			break;
		}
		default:
			throw static_cast<AMFValueType>(valueType);
			break;
	}
	return returnValue;
}

uint32_t AMFDeserialize::ReadU29(RakNet::BitStream* inStream) {
	bool hasMoreBytes = true;
	uint32_t byteNumber{};
	uint8_t numberOfBytesRead{};
	while (hasMoreBytes && numberOfBytesRead < 4) {
		uint8_t byte{};
		inStream->Read(byte);
		if (numberOfBytesRead < 3) {
			hasMoreBytes = byte & static_cast<uint8_t>(1 << 7);
			byte = byte << 1UL;
		}
		byteNumber = byteNumber << 8UL;
		byteNumber = byteNumber | static_cast<uint32_t>(byte);
		if (numberOfBytesRead < 3) {
			byteNumber = byteNumber >> 1UL;
		}
		numberOfBytesRead++;
	}
	return byteNumber;
}

std::string AMFDeserialize::ReadString(RakNet::BitStream* inStream) {
	std::string value = "";
	auto length = ReadU29(inStream);
	if (length % 2 == 1) {
		length = length >> 1;
		for (uint32_t i = 0; i < length; i++) {
			char character;
			inStream->Read(character);
			value += character;
		}
		// Empty strings are never sent by reference
		if (!value.empty()) accessedElements.push_back(value);
	} else if (length > 0){
		// Length is a reference!
		length = length >> 1;
		value = accessedElements[length];
	}
	return value;
}
