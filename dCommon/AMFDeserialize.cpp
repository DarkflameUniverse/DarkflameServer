#include "AMFDeserialize.h"

#include "AMFFormat.h"

/**
 * AMF3 Reference document https://rtmp.veriskope.com/pdf/amf3-file-format-spec.pdf
 * I have only written the values that the LEGO Universe client sends to the server.  All others are left out.
 * AMF3 Deserializer written by EmosewaMC
 */
AMFArrayValue* AMFDeserialize::Read(RakNet::BitStream* inStream, bool readStartOfArray, std::string parentKey) {
    if (!inStream) return nullptr;
    AMFArrayValue* values = new AMFArrayValue();
    if (readStartOfArray) {
        int8_t start;
        int8_t size;
        inStream->Read(start);
        inStream->Read(size);
    }

    int8_t sizeOfString;
    bool isReference = false;

    // Read identifier.  If odd, this is a literal.  If even, this is a reference to a previously read string.
    inStream->Read(sizeOfString);

    if (sizeOfString % 2 == 0) isReference = true;

    sizeOfString = sizeOfString >> 1;

    while (sizeOfString > 0) {
        // Read the key in if this this is not a reference.  Otherwise get its reference.
        std::string key;
        if (!isReference) {
            for (uint32_t i = 0; i < sizeOfString; i++) {
                int8_t character;
                inStream->Read(character);
                key.push_back(character);
            }
            // Empty strings are never sent by reference.  This should never be empty here though...
            if (!key.empty()) accessedElements.push_back(key);
        } else {
            key = accessedElements[sizeOfString];
        }
        // Read in the value type from the bitStream
        int8_t valueType;
        inStream->Read(valueType);
        // Based on the typing, read a different value
        switch (valueType) {
            case AMFValueType::AMFUndefined: {
                AMFUndefinedValue* undefinedValue = new AMFUndefinedValue();
                values->InsertValue(key, undefinedValue);
                break;
            }

            case AMFValueType::AMFNull: {
                AMFNullValue* nullValue = new AMFNullValue();
                values->InsertValue(key, nullValue);
                break;
            }

            case AMFValueType::AMFFalse: {
                AMFFalseValue* falseValue = new AMFFalseValue();
                values->InsertValue(key, falseValue);
                break;
            }

            case AMFValueType::AMFTrue: {
                AMFTrueValue* trueValue = new AMFTrueValue();
                values->InsertValue(key, trueValue);
                break;
            }

            case AMFValueType::AMFDouble: {
                AMFDoubleValue* doubleValue = new AMFDoubleValue();
                double value;
                inStream->Read(value);
                doubleValue->SetDoubleValue(value);
                values->InsertValue(key, doubleValue);
                break;
            }

            case AMFValueType::AMFString: {
                AMFStringValue* stringValue = new AMFStringValue();
                std::string value;
                int8_t length;
                inStream->Read(length);
                if (length % 2 == 1)  {
                    length = length >> 1;
                    for (uint32_t i = 0; i < length; i++) {
                        int8_t character;
                        inStream->Read(character);
                        value.push_back(character);
                    }
                    // Empty strings are never sent by reference
                    if (!value.empty()) accessedElements.push_back(value);
                } else {
                    // Length is a reference!
                    length = length / 2;
                    value = accessedElements[length];
                }
                stringValue->SetStringValue(value);
                values->InsertValue(key, stringValue);
                break;
            }

            case AMFValueType::AMFArray: {                
                int8_t sizeOfSubArray;
                int8_t index = 0;
                inStream->Read(sizeOfSubArray);

                if (sizeOfSubArray % 2 == 1) sizeOfSubArray = sizeOfSubArray >> 1;

                AMFArrayValue* result = new AMFArrayValue();
                if (sizeOfSubArray >= 1) {
                    // Need to read this byte!
                    inStream->Read(index);
                    for (uint32_t i = 0; i < sizeOfSubArray; i++) {
                        result->PushBackValue(Read(inStream, true, key));
                    }
                } else {
                    result = Read(inStream, false, key);
                }
                values->InsertValue(key, result);
                break;
            }

            // Don't think I need these for now.  Will log if I need them.
            case AMFValueType::AMFInteger:
            case AMFValueType::AMFXMLDoc:
            case AMFValueType::AMFDate:
            case AMFValueType::AMFObject:
            case AMFValueType::AMFXML:
            case AMFValueType::AMFByteArray:
            case AMFValueType::AMFVectorInt:
            case AMFValueType::AMFVectorUInt:
            case AMFValueType::AMFVectorDouble:
            case AMFValueType::AMFVectorObject:
            case AMFValueType::AMFDictionary:
                throw static_cast<AMFValueType>(valueType);
                break;
        }
        // Read size of next string
        inStream->Read(sizeOfString);

        if (sizeOfString % 2 == 0) isReference = true;
        else isReference = false;

        sizeOfString = sizeOfString >> 1;
    }
    return values;
}
