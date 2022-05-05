#pragma once

#include "BitStream.h"

#include <vector>
#include <string>

class AMFArrayValue;
class AMFDeserialize {
    public:
        /**
         * Read an AMF3 value from a bitstream.
         * 
         * @param inStream inStream to read value from.
         * @param doFullProcess currently this is here for arrays but should be removed and have its work be done in the AMFArray switch case.
         * @return Returns an AMFArrayValue with all the information from the bitStream in it.
         */
        AMFArrayValue* Read(RakNet::BitStream* inStream, bool readStartOfArray = false, std::string parentKey = "");
    private:
        std::vector<std::string> accessedElements;
};