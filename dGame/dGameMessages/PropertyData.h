#pragma once

#include "Entity.h"


struct PropertyPersonalData final {
	bool IsBestFriend;
	bool IsFriend;
	bool IsModeratorApproved;
	bool IsAlt;
	bool IsOwned;
};

struct PropertyMetaData final {
	uint32_t AccessType;
	uint32_t DateLastPublished;
	float PerformanceCost;
	uint32_t PerformanceIndex;
};

struct PropertyPrimaryData final {
	// The below two objects are not serialized with this struct however are used to gather the relative/Personal data of the property.
	LWOOBJID OwnerID;
	bool IsModeratorApproved;

	std::string OwnerName;
	std::string Name;
	std::string Description;
	float Reputation;
};

struct PropertyData {
	LWOCLONEID CloneID;

	PropertyPrimaryData PrimaryData;
	PropertyPersonalData PersonalData;
	PropertyMetaData MetaData;
};

