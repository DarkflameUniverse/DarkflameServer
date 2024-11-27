#ifndef TESTSQLDATABASE_H
#define TESTSQLDATABASE_H

#include "GameDatabase.h"

class TestSQLDatabase : public GameDatabase {
	void Connect() override;
	void Destroy(std::string source = "") override;

	sql::PreparedStatement* CreatePreppedStmt(const std::string& query) override;
	void Commit() override;
	bool GetAutoCommit() override;
	void SetAutoCommit(bool value) override;
	void ExecuteCustomQuery(const std::string_view query) override;

	// Overloaded queries
	std::optional<IServers::MasterInfo> GetMasterInfo() override;

	std::vector<std::string> GetApprovedCharacterNames() override;

	std::vector<FriendData> GetFriendsList(uint32_t charID) override;

	std::optional<IFriends::BestFriendStatus> GetBestFriendStatus(const uint32_t playerCharacterId, const uint32_t friendCharacterId) override;
	void SetBestFriendStatus(const uint32_t playerAccountId, const uint32_t friendAccountId, const uint32_t bestFriendStatus) override;
	void AddFriend(const uint32_t playerAccountId, const uint32_t friendAccountId) override;
	void RemoveFriend(const uint32_t playerAccountId, const uint32_t friendAccountId) override;
	void UpdateActivityLog(const uint32_t characterId, const eActivityType activityType, const LWOMAPID mapId) override;
	void DeleteUgcModelData(const LWOOBJID& modelId) override;
	void UpdateUgcModelData(const LWOOBJID& modelId, std::istringstream& lxfml) override;
	std::vector<IUgc::Model> GetAllUgcModels() override;
	void CreateMigrationHistoryTable() override;
	bool IsMigrationRun(const std::string_view str) override;
	void InsertMigration(const std::string_view str) override;
	std::optional<ICharInfo::Info> GetCharacterInfo(const uint32_t charId) override;
	std::optional<ICharInfo::Info> GetCharacterInfo(const std::string_view charId) override;
	std::string GetCharacterXml(const uint32_t accountId) override;
	void UpdateCharacterXml(const uint32_t characterId, const std::string_view lxfml) override;
	std::optional<IAccounts::Info> GetAccountInfo(const std::string_view username) override;
	void InsertNewCharacter(const ICharInfo::Info info) override;
	void InsertCharacterXml(const uint32_t accountId, const std::string_view lxfml) override;
	std::vector<uint32_t> GetAccountCharacterIds(uint32_t accountId) override;
	void DeleteCharacter(const uint32_t characterId) override;
	void SetCharacterName(const uint32_t characterId, const std::string_view name) override;
	void SetPendingCharacterName(const uint32_t characterId, const std::string_view name) override;
	void UpdateLastLoggedInCharacter(const uint32_t characterId) override;
	void SetPetNameModerationStatus(const LWOOBJID& petId, const IPetNames::Info& info) override;
	std::optional<IPetNames::Info> GetPetNameInfo(const LWOOBJID& petId) override;
	std::optional<IProperty::Info> GetPropertyInfo(const LWOMAPID mapId, const LWOCLONEID cloneId) override;
	void UpdatePropertyModerationInfo(const IProperty::Info& info) override;
	void UpdatePropertyDetails(const IProperty::Info& info) override;
	void InsertNewProperty(const IProperty::Info& info, const uint32_t templateId, const LWOZONEID& zoneId) override;
	std::vector<IPropertyContents::Model> GetPropertyModels(const LWOOBJID& propertyId) override;
	void RemoveUnreferencedUgcModels() override;
	void InsertNewPropertyModel(const LWOOBJID& propertyId, const IPropertyContents::Model& model, const std::string_view name) override;
	void UpdateModel(const LWOOBJID& propertyId, const NiPoint3& position, const NiQuaternion& rotation, const std::array<std::pair<int32_t, std::string>, 5>& behaviors) override;
	void RemoveModel(const LWOOBJID& modelId) override;
	void UpdatePerformanceCost(const LWOZONEID& zoneId, const float performanceCost) override;
	void InsertNewBugReport(const IBugReports::Info& info) override;
	void InsertCheatDetection(const IPlayerCheatDetections::Info& info) override;
	void InsertNewMail(const IMail::MailInfo& mail) override;
	void InsertNewUgcModel(
		std::istringstream& sd0Data,
		const uint32_t blueprintId,
		const uint32_t accountId,
		const uint32_t characterId) override;
	std::vector<IMail::MailInfo> GetMailForPlayer(const uint32_t characterId, const uint32_t numberOfMail) override;
	std::optional<IMail::MailInfo> GetMail(const uint64_t mailId) override;
	uint32_t GetUnreadMailCount(const uint32_t characterId) override;
	void MarkMailRead(const uint64_t mailId) override;
	void DeleteMail(const uint64_t mailId) override;
	void ClaimMailItem(const uint64_t mailId) override;
	void InsertSlashCommandUsage(const uint32_t characterId, const std::string_view command) override;
	void UpdateAccountUnmuteTime(const uint32_t accountId, const uint64_t timeToUnmute) override;
	void UpdateAccountBan(const uint32_t accountId, const bool banned) override;
	void UpdateAccountPassword(const uint32_t accountId, const std::string_view bcryptpassword) override;
	void InsertNewAccount(const std::string_view username, const std::string_view bcryptpassword) override;
	void SetMasterIp(const std::string_view ip, const uint32_t port) override;
	std::optional<uint32_t> GetCurrentPersistentId() override;
	void InsertDefaultPersistentId() override;
	void UpdatePersistentId(const uint32_t id) override;
	std::optional<uint32_t> GetDonationTotal(const uint32_t activityId) override;
	std::optional<bool> IsPlaykeyActive(const int32_t playkeyId) override;
	std::vector<IUgc::Model> GetUgcModels(const LWOOBJID& propertyId) override;
	void AddIgnore(const uint32_t playerId, const uint32_t ignoredPlayerId) override;
	void RemoveIgnore(const uint32_t playerId, const uint32_t ignoredPlayerId) override;
	std::vector<IIgnoreList::Info> GetIgnoreList(const uint32_t playerId) override;
	void InsertRewardCode(const uint32_t account_id, const uint32_t reward_code) override;
	std::vector<uint32_t> GetRewardCodesByAccountID(const uint32_t account_id) override;
	void AddBehavior(const IBehaviors::Info& info) override;
	std::string GetBehavior(const int32_t behaviorId) override;
	void RemoveBehavior(const int32_t behaviorId) override;
	void UpdateAccountGmLevel(const uint32_t accountId, const eGameMasterLevel gmLevel) override;
	std::optional<IProperty::PropertyEntranceResult> GetProperties(const IProperty::PropertyLookup& params) override { return {}; };
};

#endif  //!TESTSQLDATABASE_H
