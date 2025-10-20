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

	bool IsNameInUse(const std::string_view name) override { return false; };
	std::optional<IPropertyContents::Model> GetModel(const LWOOBJID modelID) override { return {}; }
	std::optional<IProperty::Info> GetPropertyInfo(const LWOOBJID id) override { return {}; }
	std::optional<IUgc::Model> GetUgcModel(const LWOOBJID ugcId) override { return {}; }
};

#endif  //!TESTSQLDATABASE_H
