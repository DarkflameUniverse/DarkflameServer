#pragma once

#include "RakNetDefines.h"
#include "GameMessages.h"

class AMFDeserialize {
    public:
        AMFArrayValue* Read(RakNet::BitStream* inStream, bool doFullProcess = false);
    private:
        /**
         * Elements read in from bitStream so far.
         */
        std::vector<std::string> accessedElements;
};