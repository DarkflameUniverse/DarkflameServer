#pragma once

#include "Entity.h"

class PropertySelectQueryProperty final
{
public:
	void Serialize(RakNet::BitStream& stream) const;

	void Deserialize(RakNet::BitStream& stream) const;

    LWOCLONEID CloneId = LWOCLONEID_INVALID;
    std::string OwnerName = "";
    std::string Name = "";
    std::string Description = "";
    uint32_t Reputation = 0;
    bool IsBestFriend = false;
    bool IsFriend = false;
    bool IsModeratorApproved = false;
    bool IsAlt = false;
    bool IsOwned = false;
    uint32_t AccessType = 0;
    uint32_t DatePublished = 0;
    uint64_t PerformanceCost = 0;
};
