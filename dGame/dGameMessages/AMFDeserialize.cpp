#include "AMFDeserialize.h"

#include "dLogger.h"

AMFArrayValue* AMFDeserialize::Read(RakNet::BitStream* inStream, bool doFullProcess) {
    AMFArrayValue* values = new AMFArrayValue();
    // Only do this if doFullProcess is true.
    if (doFullProcess) {
        int8_t start;
        int8_t size;
        inStream->Read(start);
        inStream->Read(size);

        // Shift size 1 right if odd.  This should always result in a zero.
        if (size % 2 == 1) size = size >> 1;
        // Now that we have read the header, start reading the info.
    }

    int8_t sizeOfString;
    bool isReference = false;

    // Read identifier.  If odd, this is a literal.  If even, this is a reference to a previously read string.
    inStream->Read(sizeOfString);
    if (sizeOfString % 2 == 1) {
        sizeOfString = sizeOfString >> 1;
    }
    else {
        // This WILL be even to begin with and will never result in truncation
        sizeOfString = sizeOfString / 2;
        isReference = true;
    }

    while (sizeOfString > 0) {
        // Read the key in if this this is not a reference.  Otherwise get its reference.
        std::string key;
        if (!isReference) {
            for (uint32_t i = 0; i < sizeOfString; i++) {
                int8_t character;
                inStream->Read(character);
                key.push_back(character);
            }
            accessedElements.push_back(key);
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
                Game::logger->Log("AMFDeserialize", "Hit undefinedValue (%s)!\n", key.c_str());
                break;
            }

            case AMFValueType::AMFNull: {
                AMFNullValue* nullValue = new AMFNullValue();
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
                Game::logger->Log("AMFDeserialize", "Key (%s) value (%f)!\n", key.c_str(), value);
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
                    if (!value.empty()) accessedElements.push_back(value);
                } else {
                    // Length is a reference if even!
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
                int8_t doFullProcess = 0;
                inStream->Read(sizeOfSubArray);
                if (sizeOfSubArray % 2 == 1) {
                    sizeOfSubArray = sizeOfSubArray >> 1;
                }
                AMFArrayValue* result = nullptr;
                if (sizeOfSubArray >= 1) {
                    inStream->Read(doFullProcess);
                    for (uint32_t i = 0; i < sizeOfSubArray; i++) {
                        result = Read(inStream, true);
                    }
                } else {
                    result = Read(inStream);
                }
                values->InsertValue(key, result);
                Game::logger->Log("AMFDeserialize", "Array has key (%s)\n", key.c_str());
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
                Game::logger->Log("AMFDeserialize", "Got unusable value %i with key (%s)!  Returning NULL\n", valueType, key);
                throw valueType;
                break;
        }
        isReference = false;
        inStream->Read(sizeOfString);
        if (sizeOfString % 2 == 1) {
            sizeOfString = sizeOfString >> 1;
        }
        else {
            // This WILL be even to begin with and will never result in truncation
            sizeOfString = sizeOfString / 2;
            isReference = true;
        }
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