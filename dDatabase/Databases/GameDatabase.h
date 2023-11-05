#ifndef __GAMEDATABASE__H__
#define __GAMEDATABASE__H__

#include "DatabaseStructs.h"
#include <optional>

namespace sql {
	class Statement;
	class PreparedStatement;
};

class GameDatabase {
public:
	virtual void Connect() = 0;
	virtual void Destroy(std::string source = "", bool log = true) = 0;
	virtual sql::Statement* CreateStmt() = 0;
	virtual sql::PreparedStatement* CreatePreppedStmt(const std::string& query) = 0;
	virtual void Commit() = 0;
	virtual bool GetAutoCommit() = 0;
	virtual void SetAutoCommit(bool value) = 0;

	virtual std::optional<DatabaseStructs::MasterInfo> GetMasterInfo() = 0;
	virtual std::optional<DatabaseStructs::ApprovedNames> GetApprovedCharacterNames() = 0;
	virtual std::optional<DatabaseStructs::FriendsList> GetFriendsList(const uint32_t charID) = 0;
	virtual bool DoesCharacterExist(const std::string& name) = 0;
	virtual std::optional<DatabaseStructs::BestFriendStatus> GetBestFriendStatus(const uint32_t playerAccountId, const uint32_t friendAccountId) = 0;
	virtual void SetBestFriendStatus(const uint32_t playerAccountId, const uint32_t friendAccountId, const uint32_t bestFriendStatus) = 0;
	virtual void AddFriend(const uint32_t playerAccountId, const uint32_t friendAccountId) = 0;
	virtual std::optional<uint32_t> GetAccountIdFromCharacterName(const std::string& name) = 0;
	virtual void RemoveFriend(const uint32_t playerAccountId, const uint32_t friendAccountId) = 0;
	virtual void UpdateActivityLog(const uint32_t accountId, const eActivityType activityType, const LWOMAPID mapId) = 0;
	virtual void DeleteUgcModelData(const LWOOBJID& modelId) = 0;
	virtual void UpdateUgcModelData(const LWOOBJID& modelId, std::istringstream& lxfml) = 0;
	virtual std::vector<DatabaseStructs::UgcModel> GetUgcModels() = 0;
	virtual void CreateMigrationHistoryTable() = 0;
	virtual bool IsMigrationRun(const std::string_view str) = 0;
	virtual void InsertMigration(const std::string_view str) = 0;
	virtual std::optional<DatabaseStructs::CharacterInfo> GetCharacterInfo(const uint32_t charId) = 0;
	virtual std::string GetCharacterXml(const uint32_t charId) = 0;
	virtual void UpdateCharacterXml(const uint32_t charId, const std::string_view lxfml) = 0;
	virtual std::optional<DatabaseStructs::UserInfo> GetUserInfo(const std::string_view username) = 0;
	virtual std::optional<uint32_t> GetLastUsedCharacterId(const uint32_t accountId) = 0;
	virtual bool IsUsernameAvailable(const std::string_view username) = 0;
	virtual void InsertNewCharacter(const uint32_t accountId, const uint32_t characterId, const std::string_view name, const std::string_view pendingName) = 0;
	virtual void InsertCharacterXml(const uint32_t accountId, const std::string_view lxfml) = 0;
	virtual std::vector<uint32_t> GetCharacterIds(uint32_t accountId) = 0;
	virtual bool IsCharacterIdInUse(const uint32_t characterId) = 0;
	virtual void DeleteCharacter(const uint32_t characterId) = 0;
	virtual void SetCharacterName(const uint32_t characterId, const std::string_view name) = 0;
	virtual void SetPendingCharacterName(const uint32_t characterId, const std::string_view name) = 0;
	virtual void UpdateLastLoggedInCharacter(const uint32_t characterId) = 0;
	virtual void SetPetNameModerationStatus(const LWOOBJID& petId, const std::string_view name, const int32_t approvalStatus) = 0;
	virtual std::optional<DatabaseStructs::PetNameInfo> GetPetNameInfo(const LWOOBJID& petId) = 0;
	virtual std::optional<DatabaseStructs::PropertyInfo> GetPropertyInfo(const uint32_t templateId, const uint32_t cloneId) = 0;
	virtual void UpdatePropertyModerationInfo(const LWOOBJID& id, const uint32_t privacyOption, const std::string_view rejectionReason, const uint32_t modApproved) = 0;
	virtual void UpdatePropertyDetails(const LWOOBJID& id, const std::string_view name, const std::string_view description) = 0;
	virtual void InsertNewProperty(
		const LWOOBJID& propertyId,
		const uint32_t characterId,
		const uint32_t templateId,
		const uint32_t cloneId,
		const std::string_view name,
		const std::string_view description,
		const uint32_t zoneId) = 0;

	virtual std::vector<DatabaseStructs::DatabaseModel> GetPropertyModels(const LWOOBJID& propertyId) = 0;
	virtual void RemoveUnreferencedUgcModels() = 0;
	virtual void InsertNewPropertyModel(const LWOOBJID& propertyId, const DatabaseStructs::DatabaseModel& model, const std::string_view name) = 0;
	virtual void UpdateModelPositionRotation(const LWOOBJID& propertyId, const NiPoint3& position, const NiQuaternion& rotation) = 0;
	virtual void RemoveModel(const LWOOBJID& modelId) = 0;
	virtual std::vector<LWOOBJID> GetPropertyModelIds(const LWOOBJID& propertyId) = 0;
	virtual std::string GetCharacterNameForCloneId(const uint32_t cloneId) = 0;
	virtual std::optional<DatabaseStructs::PropertyModerationInfo> GetPropertyModerationInfo(const LWOOBJID& propertyId) = 0;
};

#endif  //!__GAMEDATABASE__H__
