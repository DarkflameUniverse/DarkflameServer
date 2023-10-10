#pragma once

#include <string>

#include "ePermissionMap.h"

struct CharacterInfo {
	uint32_t AccountID;
	uint32_t ID;
	std::string Name;
	std::string PendingName;
	bool NameRejected;
	uint32_t PropertyCloneID;
	ePermissionMap PermissionMap;
};

struct AccountInfo {
	uint32_t ID;
	std::string Name;
	std::string Password;
	uint32_t MaxGMLevel;
	bool Locked;
	bool Banned;
	uint32_t PlayKeyID;
	uint64_t CreatedAt;
	uint64_t MuteExpire;
};

struct PetName {
	uint64_t ID;
	std::string Name;
	bool Approved;
};
