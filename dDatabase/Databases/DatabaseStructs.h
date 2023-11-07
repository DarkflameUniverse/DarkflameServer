#ifndef __DATABASESTRUCTS__H__
#define __DATABASESTRUCTS__H__

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "dCommonVars.h"
#include "eGameMasterLevel.h"
#include "ePermissionMap.h"
#include "NiQuaternion.h"

enum class eActivityType : uint32_t {
	PlayerLoggedIn,
	PlayerLoggedOut,
};

// Contains various structures used as return values from database queries.
namespace DatabaseStructs {
	struct MasterInfo {
		std::string ip;
		uint32_t port;
	};

	struct ApprovedNames {
		std::vector<std::string> names;
	};

	struct FriendsList {
		std::vector<FriendData> friends;
	};

	struct BestFriendStatus {
		uint32_t playerAccountId;
		uint32_t friendAccountId;
		uint32_t bestFriendStatus;
	};

	struct UgcModel {
		LWOOBJID id;
		std::stringstream lxfmlData;
	};

	struct CharacterInfo {
		std::string name;
		std::string pendingName;
		bool needsRename;
		LWOCLONEID cloneId;
		ePermissionMap permissionMap;
	};

	struct UserInfo {
		uint32_t accountId;
		eGameMasterLevel maxGMLevel;
	};

	struct PetNameInfo {
		std::string petName;
		int32_t approvalStatus;
	};

	struct PropertyInfo {
		LWOOBJID id;
		LWOOBJID ownerId;
		LWOCLONEID cloneId;
		std::string name;
		std::string description;
		int32_t privacyOption;
		std::string rejectionReason;
		uint32_t modApproved;
		uint32_t lastUpdatedTime;
		uint32_t claimedTime;
		uint32_t reputation;
	};

	struct DatabaseModel {
		LWOOBJID id;
		LOT lot;
		NiPoint3 position;
		NiQuaternion rotation;
		LWOOBJID ugcId;
	};

	struct PropertyModerationInfo {
		std::string rejectionReason;
		uint32_t modApproved;
	};

	struct MailInsert {
		std::string senderUsername;
		std::string recipient;
		std::string subject;
		std::string body;
		uint32_t senderId{};
		uint32_t receiverId{};
		int32_t attachmentCount{};
		uint32_t itemID{};
		LOT itemLOT{};
		LWOOBJID subkey{};
	};
};

#endif  //!__DATABASESTRUCTS__H__
