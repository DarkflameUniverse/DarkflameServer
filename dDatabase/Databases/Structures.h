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

struct MailInfo {
	uint64_t ID;
	uint32_t SenderID;
	std::string SenderName; 
	uint32_t ReceiverID;
	std::string ReceiverName;
	uint64_t TimeSent;
	std::string Subject;
	std::string Body;
	uint32_t AttachmentID;
	uint32_t AttachmentLOT;
	uint64_t AttachmentSubkey;
	uint32_t AttachmentCount;
	bool WasRead;
};