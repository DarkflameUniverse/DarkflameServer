#ifndef TESTSQLDATABASE_H
#define TESTSQLDATABASE_H

#include "GameDatabase.h"

class TestSQLDatabase : public GameDatabase {
	void Connect() override;
	void Destroy(std::string source = "") override;

	void Commit() override;
	bool GetAutoCommit() override;
	void SetAutoCommit(bool value) override;
	void ExecuteCustomQuery(const std::string_view query) override;

	// Overloaded queries
	std::optional<IServers::MasterInfo> GetMasterInfo() override;

	std::vector<std::string> GetApprovedCharacterNames() override;

	std::vector<FriendData> GetFriendsList(LWOOBJID charID) override;

	std::optional<IFriends::BestFriendStatus> GetBestFriendStatus(const LWOOBJID playerCharacterId, const LWOOBJID friendCharacterId) override;
	void SetBestFriendStatus(const LWOOBJID playerAccountId, const LWOOBJID friendAccountId, const uint32_t bestFriendStatus) override;
	void AddFriend(const LWOOBJID playerAccountId, const LWOOBJID friendAccountId) override;
	void RemoveFriend(const LWOOBJID playerAccountId, const LWOOBJID friendAccountId) override;
	void UpdateActivityLog(const LWOOBJID characterId, const eActivityType activityType, const LWOMAPID mapId) override;
	std::vector<IActivityLog::Entry> GetRecentActivity(const uint32_t limit) override;
	uint32_t GetActivityLogCount() override;
	std::vector<IActivityLog::Entry> GetActivityLogPaginated(uint32_t offset, uint32_t limit, const std::string& orderColumn, const std::string& orderDir) override;
	void DeleteUgcModelData(const LWOOBJID& modelId) override;
	void UpdateUgcModelData(const LWOOBJID& modelId, std::stringstream& lxfml) override;
	std::vector<IUgc::Model> GetAllUgcModels() override;
	void CreateMigrationHistoryTable() override;
	bool IsMigrationRun(const std::string_view str) override;
	void InsertMigration(const std::string_view str) override;
	std::optional<ICharInfo::Info> GetCharacterInfo(const LWOOBJID charId) override;
	std::optional<ICharInfo::Info> GetCharacterInfo(const std::string_view charId) override;
	std::string GetCharacterXml(const LWOOBJID accountId) override;
	void UpdateCharacterXml(const LWOOBJID characterId, const std::string_view lxfml) override;
	std::optional<IAccounts::Info> GetAccountInfo(const std::string_view username) override;
	std::vector<IAccounts::ListInfo> GetAllAccounts() override;
	void UpdateAccountLock(const uint32_t accountId, const bool locked) override;
	std::vector<IAccounts::SessionInfo> GetAccountSessions(const uint32_t accountId, uint32_t limit = 50) override { return {}; }
	void InsertNewCharacter(const ICharInfo::Info info) override;
	void InsertCharacterXml(const LWOOBJID accountId, const std::string_view lxfml) override;
	std::vector<LWOOBJID> GetAccountCharacterIds(LWOOBJID accountId) override;
	void DeleteCharacter(const LWOOBJID characterId) override;
	void SetCharacterName(const LWOOBJID characterId, const std::string_view name) override;
	void SetPendingCharacterName(const LWOOBJID characterId, const std::string_view name) override;
	void UpdateLastLoggedInCharacter(const LWOOBJID characterId) override;
	void SetPetNameModerationStatus(const LWOOBJID& petId, const IPetNames::Info& info) override;
	std::optional<IPetNames::Info> GetPetNameInfo(const LWOOBJID& petId) override;
	std::optional<IProperty::Info> GetPropertyInfo(const LWOMAPID mapId, const LWOCLONEID cloneId) override;
	void UpdatePropertyModerationInfo(const IProperty::Info& info) override;
	void UpdatePropertyDetails(const IProperty::Info& info) override;
	void UpdateLastSave(const IProperty::Info& info) override;
	void InsertNewProperty(const IProperty::Info& info, const uint32_t templateId, const LWOZONEID& zoneId) override;
	std::vector<IPropertyContents::Model> GetPropertyModels(const LWOOBJID& propertyId) override;
	void RemoveUnreferencedUgcModels() override;
	void InsertNewPropertyModel(const LWOOBJID& propertyId, const IPropertyContents::Model& model, const std::string_view name) override;
	void UpdateModel(const LWOOBJID& modelID, const NiPoint3& position, const NiQuaternion& rotation, const std::array<std::pair<LWOOBJID, std::string>, 5>& behaviors) override;
	void RemoveModel(const LWOOBJID& modelId) override;
	void UpdatePerformanceCost(const LWOZONEID& zoneId, const float performanceCost) override;
	void InsertNewBugReport(const IBugReports::Info& info) override;
	void InsertCheatDetection(const IPlayerCheatDetections::Info& info) override;
	void InsertNewMail(const MailInfo& mail) override;

	// PlayKeys (dashboard)
	std::vector<IPlayKeys::Info> GetAllPlayKeys() override;
	std::optional<IPlayKeys::Info> GetPlayKeyById(const uint32_t playkeyId) override;
	void CreatePlayKey(const std::string_view key, uint32_t accountId, const std::string_view notes) override;
	void UpdatePlayKey(const uint32_t playkeyId, uint32_t accountId, bool active, const std::string_view notes) override;
	void DeletePlayKey(const uint32_t playkeyId) override;
	uint32_t GetPlayKeyCount() override;
	std::optional<IPlayKeys::Info> GetPlayKeyByString(const std::string_view key_string) override;
	bool ConsumePlayKeyUsage(const uint32_t playkeyId) override;

	// Bug reports (dashboard)
	std::vector<IBugReports::DetailedInfo> GetAllBugReports() override;
	std::vector<IBugReports::DetailedInfo> GetUnresolvedBugReports() override;
	std::vector<IBugReports::DetailedInfo> GetResolvedBugReports() override;
	std::optional<IBugReports::DetailedInfo> GetBugReportById(const uint64_t reportId) override;
	void ResolveBugReport(const uint64_t reportId, const uint32_t resolverAccountId, const std::string_view resolutionNotes) override;
	uint32_t GetBugReportCount() override;
	uint32_t GetUnresolvedBugReportCount() override;

	// Property moderation (dashboard)
	std::vector<IProperty::Info> GetAllProperties() override;
	std::vector<IProperty::Info> GetPropertiesByApprovalStatus(uint32_t status) override;
	uint32_t GetPropertyCount() override;
	uint32_t GetUnapprovedPropertyCount() override;

	// Pet name moderation (dashboard)
	std::vector<IPetNames::DetailedInfo> GetAllPetNames() override;
	std::vector<IPetNames::DetailedInfo> GetPetNamesByStatus(int32_t status) override;
	void SetPetApprovalStatus(const LWOOBJID& petId, int32_t status) override;
	uint32_t GetPendingPetNamesCount() override;

	// Character edits (dashboard)
	void UpdateCharacterPermissions(const LWOOBJID characterId, const ePermissionMap permissions) override;
	void SetCharacterNeedsRename(const LWOOBJID characterId, const bool needsRename) override;
	std::optional<ICharInfo::Stats> GetCharacterStats(const LWOOBJID characterId) override { return {}; }
	std::vector<ICharInfo::InventoryItem> GetCharacterInventory(const LWOOBJID characterId) override { return {}; }
	std::vector<ICharInfo::Activity> GetCharacterActivity(const LWOOBJID characterId, uint32_t limit = 50) override { return {}; }
	void RescueCharacter(const LWOOBJID characterId, uint32_t zoneId) override {}

	// Account dashboard helpers
	std::optional<IAccounts::DetailedInfo> GetAccountById(const uint32_t accountId) override;
	void UpdateAccountEmail(const uint32_t accountId, const std::string_view email) override;
	void DeleteAccount(const uint32_t accountId) override;

	void InsertNewUgcModel(
		std::stringstream& sd0Data,
		const uint64_t blueprintId,
		const uint32_t accountId,
		const LWOOBJID characterId) override;
	std::vector<MailInfo> GetMailForPlayer(const LWOOBJID characterId, const uint32_t numberOfMail) override;
	std::optional<MailInfo> GetMail(const uint64_t mailId) override;
	uint32_t GetUnreadMailCount(const LWOOBJID characterId) override;
	void MarkMailRead(const uint64_t mailId) override;
	void DeleteMail(const uint64_t mailId) override;
	void ClaimMailItem(const uint64_t mailId) override;
	void InsertSlashCommandUsage(const LWOOBJID characterId, const std::string_view command) override;
	std::vector<ICommandLog::Entry> GetCommandLogs(uint32_t limit = 100) override { return {}; }
	void UpdateAccountUnmuteTime(const uint32_t accountId, const uint64_t timeToUnmute) override;
	void UpdateAccountBan(const uint32_t accountId, const bool banned) override;
	void UpdateAccountPassword(const uint32_t accountId, const std::string_view bcryptpassword) override;
	void InsertNewAccount(const std::string_view username, const std::string_view bcryptpassword) override;
	void SetMasterInfo(const IServers::MasterInfo& info) override;
	std::optional<uint64_t> GetCurrentPersistentId() override;
	IObjectIdTracker::Range GetPersistentIdRange() override;
	void InsertDefaultPersistentId() override;
	std::optional<uint32_t> GetDonationTotal(const uint32_t activityId) override;
	std::optional<bool> IsPlaykeyActive(const int32_t playkeyId) override;
	std::vector<IUgc::Model> GetUgcModels(const LWOOBJID& propertyId) override;
	void AddIgnore(const LWOOBJID playerId, const LWOOBJID ignoredPlayerId) override;
	void RemoveIgnore(const LWOOBJID playerId, const LWOOBJID ignoredPlayerId) override;
	std::vector<IIgnoreList::Info> GetIgnoreList(const LWOOBJID playerId) override;
	void InsertRewardCode(const uint32_t account_id, const uint32_t reward_code) override;
	std::vector<uint32_t> GetRewardCodesByAccountID(const uint32_t account_id) override;
	void AddBehavior(const IBehaviors::Info& info) override;
	std::string GetBehavior(const LWOOBJID behaviorId) override;
	void RemoveBehavior(const LWOOBJID behaviorId) override;
	void UpdateAccountGmLevel(const uint32_t accountId, const eGameMasterLevel gmLevel) override;
	void UpdateAccountPlayKey(const uint32_t accountId, const uint32_t playKeyId) override;
	std::optional<IProperty::PropertyEntranceResult> GetProperties(const IProperty::PropertyLookup& params) override { return {}; };
	std::vector<ILeaderboard::Entry> GetDescendingLeaderboard(const uint32_t activityId) override { return {}; };
	std::vector<ILeaderboard::Entry> GetAscendingLeaderboard(const uint32_t activityId) override { return {}; };
	std::vector<ILeaderboard::Entry> GetNsLeaderboard(const uint32_t activityId) override { return {}; };
	std::vector<ILeaderboard::Entry> GetAgsLeaderboard(const uint32_t activityId) override { return {}; };
	void SaveScore(const LWOOBJID playerId, const uint32_t gameId, const Score& score) override {};
	void UpdateScore(const LWOOBJID playerId, const uint32_t gameId, const Score& score) override {};
	std::optional<ILeaderboard::Score> GetPlayerScore(const LWOOBJID playerId, const uint32_t gameId) override { return {}; };
	void IncrementNumWins(const LWOOBJID playerId, const uint32_t gameId) override {};
	void IncrementTimesPlayed(const LWOOBJID playerId, const uint32_t gameId) override {};
	void InsertUgcBuild(const std::string& modules, const LWOOBJID bigId, const std::optional<LWOOBJID> characterId) override {};
	void DeleteUgcBuild(const LWOOBJID bigId) override {};
	uint32_t GetAccountCount() override { return 0; };
	uint32_t GetBannedAccountCount() override { return 0; };
	uint32_t GetLockedAccountCount() override { return 0; };

	bool IsNameInUse(const std::string_view name) override { return false; };
	uint32_t GetCharacterCount() override { return 0; };
	std::vector<ICharInfo::Info> GetAllCharactersPaginated(uint32_t offset, uint32_t limit, const std::string& orderColumn, const std::string& orderDir) override { return {}; };
	std::vector<ICharInfo::Info> GetCharactersWithPendingNames() override { return {}; };
	std::optional<IPropertyContents::Model> GetModel(const LWOOBJID modelID) override { return {}; }
	std::optional<IProperty::Info> GetPropertyInfo(const LWOOBJID id) override { return {}; }
	std::optional<IUgc::Model> GetUgcModel(const LWOOBJID ugcId) override { return {}; }

	// Dashboard Audit Log
	void InsertAuditLog(const std::string_view ip_address, const std::string_view endpoint,
		const std::string_view method, const std::string_view user_agent, int32_t response_code) override {}
	std::vector<IDashboardAuditLog::AuditLogEntry> GetRecentAuditLogs(uint32_t limit) override { return {}; }
	std::vector<IDashboardAuditLog::AuditLogEntry> GetAuditLogsByIP(const std::string_view ip_address, uint32_t limit) override { return {}; }
	void CleanupOldAuditLogs(uint32_t days_to_keep) override {}
	void InsertAdminActionLog(uint32_t adminAccountId, const std::string_view action,
	                          const std::string_view targetType, uint64_t targetId,
	                          const std::string_view details) override {}
	std::vector<IDashboardAuditLog::AdminActionLog> GetAuditLogs(uint32_t limit = 100) override { return {}; }

	// Dashboard Config
	std::optional<std::string> GetDashboardConfig(const std::string_view config_key) override { return {}; }
	void SetDashboardConfig(const std::string_view config_key, const std::string_view config_value) override {}
};

#endif  //!TESTSQLDATABASE_H
