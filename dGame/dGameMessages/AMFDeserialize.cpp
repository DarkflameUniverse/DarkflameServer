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
        // If size is greater than 1 the next number will be
        Game::logger->Log("AMFDeserialize", "start %i size %i\n", start, size);

        // Now that we have read the header, start reading the info.
    }

    int8_t sizeOfString;
    bool isReference = false;

    // Read identifier.  If odd, this is a literal.  If even, this is a reference to a previously read string.
    inStream->Read(sizeOfString);
    if (sizeOfString % 2 == 1) {
        sizeOfString = sizeOfString >> 1;
        Game::logger->Log("AMFDeserialize", "Size of initial is %i\n", sizeOfString);
    }
    else {
        Game::logger->Log("AMFDeserialize", "Size is reference to %i\n", sizeOfString);
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
        } else {
            isReference = false;
            key = accessedElements[sizeOfString];
            Game::logger->Log("AMFDeserialize", "Key is a reference (%s)!\n", key.c_str());
        }
        // Read in the value type from the bitStream
        int8_t valueType;
        inStream->Read(valueType);

        // Based on the typing, read a different value
        switch (valueType) {
            case AMFValueType::AMFUndefined: {
                AMFUndefinedValue* undefinedValue = new AMFUndefinedValue();
                Game::logger->Log("AMFDeserialize", "Hit undefinedValue (%s)!\n", key.c_str());
                accessedElements.push_back(key);
                accessedElements.push_back("");
                break;
            }
            case AMFValueType::AMFNull: {
                AMFNullValue* nullValue = new AMFNullValue();
                Game::logger->Log("AMFDeserialize", "Hit nullValue (%s)!\n", key.c_str());
                accessedElements.push_back(key);
                accessedElements.push_back("");
                break;
            }
            case AMFValueType::AMFFalse: {
                AMFFalseValue* falseValue = new AMFFalseValue();
                values->InsertValue(key, falseValue);
                Game::logger->Log("AMFDeserialize", "Key (%s) value false!\n", key.c_str());
                accessedElements.push_back(key);
                accessedElements.push_back("0.0");
                break;
            }
            case AMFValueType::AMFTrue: {
                AMFTrueValue* trueValue = new AMFTrueValue();
                Game::logger->Log("AMFDeserialize", "Key (%s) value true!\n", key.c_str());
                values->InsertValue(key, trueValue);
                accessedElements.push_back(key);
                accessedElements.push_back("1.0");
                break;
            }
            // UNTESTED
            case AMFValueType::AMFInteger: {
                AMFIntegerValue* integerValue = new AMFIntegerValue();
                Game::logger->Log("AMFDeserialize", "Hit integerValue!\n", key.c_str());
                bool posOrNeg = -1;
                int32_t readValue = 0;
                bool hasMore = true;
                if (hasMore) {
                    inStream->Read(hasMore);
                    if (posOrNeg == -1) inStream->Read(posOrNeg);
                    for (uint32_t i = 0; i < 7; i++) {
                        readValue = readValue << 1;
                        bool bit = 0;
                        inStream->Read(bit);
                        if (bit) readValue |= 1UL << 0;
                    }
                }
                readValue = readValue << 3;
                integerValue->SetIntegerValue(readValue);
                Game::logger->Log("AMFDeserialize", "Key (%s) value (%i)!\n", key.c_str(), readValue);
                if (posOrNeg == 1) readValue = -readValue;
                Game::logger->Log("AMFDeserialize", "Post conversion Key (%s) value (%i)!\n", key.c_str(), readValue);
                values->InsertValue(key, integerValue);
                accessedElements.push_back(key);
                accessedElements.push_back(std::to_string(readValue));
                break;
            }
            case AMFValueType::AMFDouble: {
                AMFDoubleValue* doubleValue = new AMFDoubleValue();
                double value;
                inStream->Read(value);
                Game::logger->Log("AMFDeserialize", "Key (%s) value (%f)!\n", key.c_str(), value);
                doubleValue->SetDoubleValue(value);
                values->InsertValue(key, doubleValue);
                accessedElements.push_back(key);
                accessedElements.push_back(std::to_string(value));
                break;
            }
            case AMFValueType::AMFString: {
                AMFStringValue* stringValue = new AMFStringValue();
                std::string value;
                int8_t length;
                inStream->Read(length);
                if (length % 2 == 1) length = length >> 1;
                for (uint32_t i = 0; i < length; i++) {
                    int8_t character;
                    inStream->Read(character);
                    value.push_back(character);
                }
                stringValue->SetStringValue(value);
                values->InsertValue(key, stringValue);
                Game::logger->Log("AMFDeserialize", "Key (%s) value (%s)!\n", key.c_str(), value.c_str());
                accessedElements.push_back(key);
                accessedElements.push_back(value);
                break;
            }
            case AMFValueType::AMFXMLDoc: {
                AMFXMLDocValue* xmlDocValue = new AMFXMLDocValue();
                Game::logger->Log("AMFDeserialize", "Hit xmlDocValue!\n", key.c_str());
                return values;
                break;
            }
            case AMFValueType::AMFDate: {
                AMFDateValue* dateValue = new AMFDateValue();
                Game::logger->Log("AMFDeserialize", "Hit dateValue!\n", key.c_str());
                return values;
                break;
            }
            case AMFValueType::AMFArray: {                
                Game::logger->Log("AMFDeserialize", "Hit arrayValue!\n", key.c_str());
                int8_t sizeOfSubArray;
                int8_t doFullProcess = 0;
                inStream->Read(sizeOfSubArray);
                if (sizeOfSubArray % 2 == 1) {
                    sizeOfSubArray = sizeOfSubArray >> 1;
                }
                if (sizeOfSubArray > 0) {
                    inStream->Read(doFullProcess);
                }
                auto result = Read(inStream, doFullProcess >= 1);
                values->InsertValue(key, result);
                accessedElements.push_back(key);
                accessedElements.push_back("");
                Game::logger->Log("AMFDeserialize", "Array is key (%s)\n", key.c_str());
                break;
            }
            // Don't think I need these for now.  Will log if I need them.
            case AMFValueType::AMFObject:
            case AMFValueType::AMFXML:
            case AMFValueType::AMFByteArray:
            case AMFValueType::AMFVectorInt:
            case AMFValueType::AMFVectorUInt:
            case AMFValueType::AMFVectorDouble:
            case AMFValueType::AMFVectorObject:
            case AMFValueType::AMFDictionary:
                Game::logger->Log("AMFDeserialize", "Got unusable value %i with key (%s)!\n", valueType, key);
                break;
        }
        inStream->Read(sizeOfString);
        if (sizeOfString % 2 == 1) {
            sizeOfString = sizeOfString >> 1;
            Game::logger->Log("AMFDeserialize", "Size of next is (%i)\n", sizeOfString);
        }
        else {
            isReference = true;
            Game::logger->Log("AMFDeserialize", "Size of next is Reference to (%i)\n", sizeOfString);
        }
    }
    return values;
}