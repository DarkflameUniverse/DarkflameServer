#pragma once

#include "BitStream.h"

#include <vector>
#include <string>

class AMFValue;
class AMFDeserialize {
    public:
        /**
         * Read an AMF3 value from a bitstream.
         * 
         * @param inStream inStream to read value from.
         * @return Returns an AMFValue with all the information from the bitStream in it.
         */
        AMFValue* Read(RakNet::BitStream* inStream);
    private:
        /**
         * @brief Private method to read a U29 integer from a bitstream
         * 
         * @param inStream bitstream to read data from
         * @return The number as an unsigned 29 bit integer
         */
        uint32_t ReadU29(RakNet::BitStream* inStream);

        /**
         * @brief Reads a string from a bitstream
         * 
         * @param inStream bitStream to read data from
         * @return The read string
         */
        std::string ReadString(RakNet::BitStream* inStream);
        std::vector<std::string> accessedElements;
};