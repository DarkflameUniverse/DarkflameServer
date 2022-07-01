#include "AMFDeserialize.h"

#include "dLogger.h"
/**
 * AMF3 Reference document https://rtmp.veriskope.com/pdf/amf3-file-format-spec.pdf
 * I have only written the values that the LEGO Universe client sends to the server.  All others are left out.
 * AMF3 Deserializer written by EmosewaMC
 */
AMFArrayValue* AMFDeserialize::Read(RakNet::BitStream* inStream, bool readStartOfArray, std::string parentKey) {
    AMFArrayValue* values = new AMFArrayValue();
    if (readStartOfArray) {
        int8_t start;
        int8_t size;
        inStream->Read(start);
        inStream->Read(size);

        size = size >> 1;
        assert(size == 0);
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
                Game::logger->Log("AMFDeserialize", "Hit undefinedValue (%s)!\n", key.c_str());
                break;
            }

            case AMFValueType::AMFNull: {
                AMFNullValue* nullValue = new AMFNullValue();
                values->InsertValue(key, nullValue);
                Game::logger->Log("AMFDeserialize", "Hit nullValue (%s)!\n", key.c_str());
                break;
            }

            case AMFValueType::AMFFalse: {
                AMFFalseValue* falseValue = new AMFFalseValue();
                values->InsertValue(key, falseValue);
                Game::logger->Log("AMFDeserialize", "Key (%s) value false!\n", key.c_str());
                break;
            }

            case AMFValueType::AMFTrue: {
                AMFTrueValue* trueValue = new AMFTrueValue();
                Game::logger->Log("AMFDeserialize", "Key (%s) value true!\n", key.c_str());
                values->InsertValue(key, trueValue);
                break;
            }

            case AMFValueType::AMFDouble: {
                AMFDoubleValue* doubleValue = new AMFDoubleValue();
                double value;
                inStream->Read(value);
                Game::logger->Log("AMFDeserialize", "Key (%s) value (%lf)!\n", key.c_str(), value);
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
                Game::logger->Log("AMFDeserialize", "Key (%s) value (%s)!\n", key.c_str(), value.c_str());
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
                    Game::logger->Log("AMFDeserialize", "Entering sub array of multiple items with current parent key (%s)\n", key.c_str());
                    for (uint32_t i = 0; i < sizeOfSubArray; i++) {
                        result->PushBackValue(Read(inStream, true, key));
                    }
                } else {
                    Game::logger->Log("AMFDeserialize", "Entering sub array with current parent key (%s)\n", key.c_str());
                    result = Read(inStream, false, key);
                }
                values->InsertValue(key, result);
                Game::logger->Log("AMFDeserialize", "End of Array with key (%s)\n", key.c_str());
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
                Game::logger->Log("AMFDeserialize", "Got unusable value %i with key (%s)!  Returning NULL\n", valueType, key.c_str());
                throw valueType;
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

/**
 * AMFIntegerValue* integerValue = new AMFIntegerValue();
 *     Game::logger->Log("AMFDeserialize", "Hit integerValue!\n", key.c_str());
 *     bool posOrNeg = -1;
 *     int32_t readValue = 0;
 *     bool hasMore = true;
 *     if (hasMore) {
 *         inStream->Read(hasMore);
 *         if (posOrNeg == -1) inStream->Read(posOrNeg);
 *         for (uint32_t i = 0; i < 7; i++) {
 *             readValue = readValue << 1;
 *             bool bit = 0;
 *             inStream->Read(bit);
 *             if (bit) readValue |= 1UL << 0;
 *         }
 *     }
 *     readValue = readValue << 3;
 *     integerValue->SetIntegerValue(readValue);
 *     Game::logger->Log("AMFDeserialize", "Key (%s) value (%i)!\n", key.c_str(), readValue);
 *     if (posOrNeg == 1) readValue = -readValue;
 *     Game::logger->Log("AMFDeserialize", "Post conversion Key (%s) value (%i)!\n", key.c_str(), readValue);
 *     values->InsertValue(key, integerValue);
 * break;
 * 
 */