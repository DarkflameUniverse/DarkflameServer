#ifndef __MYSQLDATABASE__H__
#define __MYSQLDATABASE__H__

#include <conncpp.hpp>
#include <memory>

#include "GameDatabase.h"

class MySQLDatabase : public GameDatabase {
public:
	void Connect() override;
	void Destroy(std::string source = "", bool log = true) override;

	sql::Statement* CreateStmt() override;
	sql::PreparedStatement* CreatePreppedStmt(const std::string& query) override;
	void Commit() override;
	bool GetAutoCommit() override;
	void SetAutoCommit(bool value) override;

	// Overloaded queries
	std::optional<DatabaseStructs::MasterInfo> GetMasterInfo() override;

	std::optional<DatabaseStructs::ApprovedNames> GetApprovedCharacterNames() override;

	std::optional<DatabaseStructs::FriendsList> GetFriendsList(uint32_t charID) override;
	// No optional needed here, since if we did that, we'd return a bool of a bool in essenece.
	// Just return true if and only if the character name exists.
	bool DoesCharacterExist(const std::string& name) override;
	std::optional<DatabaseStructs::BestFriendStatus> GetBestFriendStatus(const uint32_t playerAccountId, const uint32_t friendAccountId) override;
	void SetBestFriendStatus(const uint32_t playerAccountId, const uint32_t friendAccountId, const uint32_t bestFriendStatus) override;
	void AddFriend(const uint32_t playerAccountId, const uint32_t friendAccountId) override;
	std::optional<uint32_t> GetAccountIdFromCharacterName(const std::string& name) override;
	void RemoveFriend(const uint32_t playerAccountId, const uint32_t friendAccountId) override;
	void UpdateActivityLog(const uint32_t accountId, const eActivityType activityType, const LWOMAPID mapId) override;
	void DeleteUgcModelData(const LWOOBJID& modelId) override;
	void UpdateUgcModelData(const LWOOBJID& modelId, std::istringstream& lxfml) override;
	std::vector<DatabaseStructs::UgcModel> GetUgcModels() override;
	void CreateMigrationHistoryTable() override;
	bool IsMigrationRun(const std::string_view str) override;
	void InsertMigration(const std::string_view str) override;
	std::optional<DatabaseStructs::CharacterInfo> GetCharacterInfo(const uint32_t accountId) override;
	std::string GetCharacterXml(const uint32_t accountId) override;
	void UpdateCharacterXml(const uint32_t accountId, const std::string_view lxfml) override;
	std::optional<DatabaseStructs::UserInfo> GetUserInfo(const std::string_view username) override;
	std::optional<uint32_t> GetLastUsedCharacterId(uint32_t accountId) override;
	bool IsUsernameAvailable(const std::string_view username) override;
	void InsertNewCharacter(const uint32_t accountId, const uint32_t characterId, const std::string_view name, const std::string_view pendingName) override;
	void InsertCharacterXml(const uint32_t accountId, const std::string_view lxfml) override;
	std::vector<uint32_t> GetCharacterIds(uint32_t accountId) override;
	bool IsCharacterIdInUse(const uint32_t characterId) override;
	void DeleteCharacter(const uint32_t characterId) override;
	void SetCharacterName(const uint32_t characterId, const std::string_view name) override;
	void SetPendingCharacterName(const uint32_t characterId, const std::string_view name) override;
	void UpdateLastLoggedInCharacter(const uint32_t characterId) override;
	void SetPetNameModerationStatus(const LWOOBJID& petId, const std::string_view name, const int32_t approvalStatus) override;
	std::optional<DatabaseStructs::PetNameInfo> GetPetNameInfo(const LWOOBJID& petId) override;
	std::optional<DatabaseStructs::PropertyInfo> GetPropertyInfo(const uint32_t templateId, const uint32_t cloneId) override;
	void UpdatePropertyModerationInfo(const LWOOBJID& id, const uint32_t privacyOption, const std::string_view rejectionReason, const uint32_t modApproved) override;
	void UpdatePropertyDetails(const LWOOBJID& id, const std::string_view name, const std::string_view description) override;
	void InsertNewProperty(
		const LWOOBJID& propertyId,
		const uint32_t characterId,
		const uint32_t templateId,
		const uint32_t cloneId,
		const std::string_view name,
		const std::string_view description,
		const uint32_t zoneId) override;
	std::vector<DatabaseStructs::DatabaseModel> GetPropertyModels(const LWOOBJID& propertyId) override;
	void RemoveUnreferencedUgcModels() override;
	void InsertNewPropertyModel(const LWOOBJID& propertyId, const DatabaseStructs::DatabaseModel& model, const std::string_view name) override;
	void UpdateModelPositionRotation(const LWOOBJID& propertyId, const NiPoint3& position, const NiQuaternion& rotation) override;
	void RemoveModel(const LWOOBJID& modelId) override;
	std::vector<LWOOBJID> GetPropertyModelIds(const LWOOBJID& propertyId) override;
	std::string GetCharacterNameForCloneId(const uint32_t cloneId) override;
	std::optional<DatabaseStructs::PropertyModerationInfo> GetPropertyModerationInfo(const LWOOBJID& propertyId) override;
private:
	std::unique_ptr<sql::PreparedStatement> CreatePreppedStmtUnique(const std::string& query);

	std::unique_ptr<sql::ResultSet> ExecuteQueryUnique(const std::string& query);
	std::unique_ptr<sql::ResultSet> ExecuteQueryUnique(const std::unique_ptr<sql::PreparedStatement>& query);

};

#endif  //!__MYSQLDATABASE__H__
