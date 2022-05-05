#pragma once

#include "RakNetDefines.h"
#include "GameMessages.h"

class AMFDeserialize {
    public:
        AMFArrayValue* Read(RakNet::BitStream* inStream, bool doFullProcess = false);
    private:
        std::vector<std::string> accessedElements;
};