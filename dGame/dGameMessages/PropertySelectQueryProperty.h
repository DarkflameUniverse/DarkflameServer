#pragma once

#ifndef PROPERTYSELECTQUERY_H
#define PROPERTYSELECTQUERY_H

#include "Entity.h"

class PropertySelectQueryProperty final
{
public:
	void Serialize(RakNet::BitStream& stream) const;

	void Deserialize(RakNet::BitStream& stream) const;

	LWOCLONEID CloneId = LWOCLONEID_INVALID;        // The cloneID of the property
	std::string OwnerName = "";                     // The property owners name
	std::string Name = "";                          // The property name
	std::string Description = "";                   // The property description
	float Reputation = 0;                           // The reputation of the property
	bool IsBestFriend = false;                      // Whether or not the property belongs to a best friend
	bool IsFriend = false;                          // Whether or not the property belongs to a friend
	bool IsModeratorApproved = false;               // Whether or not a moderator has approved this property
	bool IsAlt = false;                             // Whether or not the property is owned by an alt of the account owner
	bool IsOwned = false;                           // Whether or not the property is owned
	uint32_t AccessType = 0;                        // The privacy option of the property
	uint32_t DateLastPublished = 0;                 // The last day the property was published
	float PerformanceCost = 0;                      // The performance cost of the property
	uint32_t PerformanceIndex = 0;                  // The performance index of the property?  Always 0?
};

#endif
