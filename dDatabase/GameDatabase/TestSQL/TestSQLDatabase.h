#ifndef TESTSQLDATABASE_H
#define TESTSQLDATABASE_H

#include "GameDatabase.h"

class TestSQLDatabase : public GameDatabase {
	void Connect() override;
	void Destroy(std::string source = "") override;

	void Commit() override;
	bool GetAutoCommit() override;
	void SetAutoCommit(bool value) override;
	void Rollback() override {}
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
	std::optional<ICharXml::CharacterXml> ClaimCharacterXml(const LWOOBJID charId) override { return std::nullopt; };
	bool SaveCharacterXml(const LWOOBJID charId, const std::string_view lxfml, const uint64_t generation) override { return true; };
	uint64_t GetCharacterSaveGeneration(const LWOOBJID charId) override { return 0; };
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
	void InsertNewAccount(const std::string_view username, const std::string_view bcryptpassword, const eGameMasterLevel gmLevel) override;
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
	IProperty::PropertyEntranceResult GetProperties(const IProperty::PropertyLookup& params) override { return {}; };
	std::vector<ILeaderboard::Entry> GetDescendingLeaderboard(const uint32_t activityId) override { return {}; };
	std::vector<ILeaderboard::Entry> GetAscendingLeaderboard(const uint32_t activityId) override { return {}; };
	std::vector<ILeaderboard::Entry> GetNsLeaderboard(const uint32_t activityId) override { return {}; };
	std::vector<ILeaderboard::Entry> GetAgsLeaderboard(const uint32_t activityId) override { return {}; };
	void SaveScore(const LWOOBJID playerId, const uint32_t gameId, const Score& score) override {};
	void UpdateScore(const LWOOBJID playerId, const uint32_t gameId, const Score& score) override {};
	std::optional<ILeaderboard::Score> GetPlayerScore(const LWOOBJID playerId, const uint32_t gameId) override { return {}; };
	void IncrementNumWins(const LWOOBJID playerId, const uint32_t gameId) override {};
	void IncrementTimesPlayed(const LWOOBJID playerId, const uint32_t gameId) override {};
	std::map<uint32_t, uint32_t> GetLeaderboardSizes() override { return {}; }
	void DeleteLeaderboardScore(const LWOOBJID playerId, const uint32_t gameId) override {}
	void ResetLeaderboard(const uint32_t gameId) override {}
	void InsertUgcBuild(const std::string& modules, const LWOOBJID bigId, const std::optional<LWOOBJID> characterId) override {};
	void DeleteUgcBuild(const LWOOBJID bigId) override {};
	uint32_t GetAccountCount() override { return 0; };
	uint32_t GetCharacterCount() override { return 0; };
	void RecordFailedAttempt(const uint32_t accountId) override {};
	void ClearFailedAttempts(const uint32_t accountId) override {};
	void SetLockout(const uint32_t accountId, const int64_t lockoutUntil) override {};
	bool IsLockedOut(const uint32_t accountId) override { return false; };
	void SetAccountLocked(const uint32_t accountId, const bool locked) override {};
	uint8_t GetFailedAttempts(const uint32_t accountId) override { return 0; };
	nlohmann::json GetAccountsTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true) override { return nlohmann::json::object(); };
	nlohmann::json GetAccountById(uint32_t accountId) override { return nlohmann::json::object(); };
	uint32_t CountActiveAccountsAtGmLevel(const uint8_t gmLevel, const uint32_t excludeAccountId) override { return 0; };
	std::string GetCharactersTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true) override { return "{}"; };
	std::string GetPlayKeysTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true) override { return "{}"; };
	uint32_t GetPlayKeyCount() override { return 0; };
	void CreatePlayKey(const std::string_view keyString, const uint32_t uses, const std::string_view notes = "") override {};
	void SetPlayKeyActive(const int32_t playkeyId, const bool active) override {};
	std::string GetPropertiesTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true, bool pendingOnly = false) override { return "{}"; };
	uint32_t GetPropertyCount() override { return 0; };
	IProperty::ShowcaseResult GetShowcaseProperties(const IProperty::ShowcaseQuery& query) override { return {}; };
	std::string GetBugReportsTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true, int8_t resolvedFilter = -1) override { return "{}"; };
	nlohmann::json GetBugReport(const uint32_t id) override { return nlohmann::json{{"error", "not found"}}; };
	void ResolveBugReport(const uint32_t id, const uint32_t resolverAccountId, const std::string_view resolution) override {};
	nlohmann::json GetPlayKey(const int32_t playkeyId) override { return nlohmann::json{{"error", "not found"}}; };
	void UpdatePlayKey(const int32_t playkeyId, const uint32_t uses, const std::string_view notes, const bool active) override {};
	void DeletePlayKey(const int32_t playkeyId) override {};
	std::vector<std::pair<LWOOBJID, std::string>> GetCharacterIdsAndNames() override { return {}; };
	nlohmann::json GetPendingNamesTable(const uint32_t start, const uint32_t length) override { return nlohmann::json::object(); };
	void SetCharacterPermissionMap(const LWOOBJID characterId, const uint64_t permissionMap) override {};
	IDashboardStats::Snapshot GetDashboardSnapshot() override { return {}; };
	void RecordEconomy(const std::vector<CurrencyFlow>& currency, const std::vector<UScoreFlow>& uscore,
		const std::vector<ItemFlow>& items, const std::vector<ItemTransfer>& transfers, const std::vector<MapEvent>& mapEvents,
		const std::vector<PlayerStat>& stats) override {};
	nlohmann::json GetCurrencyFlows(uint32_t fromDay, uint32_t toDay, bool excludeStaff) override { return nlohmann::json::array(); };
	nlohmann::json GetUScoreFlows(uint32_t fromDay, uint32_t toDay, bool excludeStaff) override { return nlohmann::json::array(); };
	nlohmann::json GetItemFlows(uint32_t fromDay, uint32_t toDay, LOT lot, bool excludeStaff) override { return nlohmann::json::array(); };
	nlohmann::json GetTopEarners(uint32_t fromDay, uint32_t toDay, uint32_t limit, bool excludeStaff) override { return nlohmann::json::array(); };
	nlohmann::json GetTopItems(uint32_t fromDay, uint32_t toDay, uint32_t limit, bool excludeStaff) override { return nlohmann::json::array(); };
	nlohmann::json GetTransfers(uint32_t start, uint32_t length, LWOOBJID characterId, LOT lot) override { return nlohmann::json::object(); };
	nlohmann::json GetTransfersForItems(const std::vector<LWOOBJID>& itemIds) override { return nlohmann::json::array(); };
	void ForEachMailAttachment(const std::function<void(const MailAttachment&)>& visit) override {};
	std::vector<Webhook> GetWebhooks() override { return {}; }
	std::optional<Webhook> GetWebhook(uint32_t id) override { return {}; }
	void InsertWebhook(const Webhook& webhook) override {}
	void UpdateWebhook(const Webhook& webhook) override {}
	void DeleteWebhook(uint32_t id) override {}
	void RecordWebhookResult(uint32_t id, int64_t time, int32_t status, const std::string& error) override {}
	std::optional<std::string> GetDashboardState(const std::string& name) override { return {}; }
	void SetDashboardState(const std::string& name, const std::string& value) override {}
	void DeleteDashboardState(const std::string& name) override {}
	std::string GetDashboardPreferences(uint32_t accountId) override { return "{}"; }
	void SetDashboardPreferences(uint32_t accountId, const std::string& prefs) override {}
	bool InsertEconomyFlag(const EconomyFlag& flag) override { return false; }
	nlohmann::json GetEconomyFlagsTable(uint32_t start, uint32_t length, int32_t status) override { return nlohmann::json::array(); }
	void ReviewEconomyFlag(uint64_t id, eFlagStatus status, uint32_t reviewerAccountId, const std::string& note) override {}
	uint32_t GetOpenEconomyFlagCount() override { return 0; }
	std::vector<std::pair<LWOOBJID, int64_t>> GetDailyIncome(uint32_t day) override { return {}; }
	std::map<LOT, int64_t> GetItemCreationTotals(uint32_t fromDay, uint32_t toDay) override { return {}; }
	uint32_t CompactEconomy(uint32_t cutoffDay, uint32_t mapCutoffDay) override { return 0; }
	uint32_t PruneTransfers(int64_t beforeTime) override { return 0; }
	uint32_t PruneLog(eLog log, int64_t beforeTime) override { return 0; }
	void ReportConfigFromFile(const Setting& setting) override {}
	std::vector<Setting> GetServerConfig(const std::vector<std::string>& files) override { return {}; }
	void SetWebConfigValue(const std::string& file, const std::string& name, const std::optional<std::string>& value, bool webWins, const std::string& by) override {}
	void DeleteServerConfig(const std::string& file, const std::string& name) override {}
	uint64_t InsertSettingChange(const SettingChange& change) override { return 0; }
	std::vector<SettingChange> GetSettingChanges(const std::string& file, const std::string& name, uint32_t start, uint32_t length, uint64_t& total) override { total = 0; return {}; }
	std::optional<SettingChange> GetSettingChange(uint64_t id) override { return std::nullopt; }
	// IModeration
	void SetStrikeStep(uint64_t strikeId, const std::string& step, uint32_t count) override {}
	std::vector<AppliedStrikeStep> GetAppliedStrikeSteps(uint32_t accountId, int64_t since) override { return {}; }
	uint64_t InsertPlayerReport(const PlayerReport& report) override { return 0; }
	std::vector<PlayerReport> GetPlayerReports(const PlayerReportQuery& query) override { return {}; }
	uint32_t CountPlayerReports(const PlayerReportQuery& query) override { return 0; }
	std::optional<PlayerReport> GetPlayerReport(uint64_t id) override { return {}; }
	void SetPlayerReportStatus(uint64_t id, uint8_t status, const std::string& handledBy, const std::string& resolution, int64_t time) override {}
	std::vector<ChatFilterWord> GetChatFilterWords() override { return {}; }
	void SetChatFilterWord(const ChatFilterWord& word) override {}
	bool DeleteChatFilterWord(const std::string& word) override { return false; }
	void RecordLoginAddress(uint32_t accountId, const std::string& address, int64_t time) override {}
	std::vector<LinkedAccount> GetLinkedAccounts(uint32_t accountId) override { return {}; }
	uint32_t CountLoginAddresses(uint32_t accountId) override { return 0; }
	std::vector<SlashCommand> GetSlashCommands() override { return {}; }
	void SetSlashCommand(const SlashCommand& command) override {}
	void DeleteSlashCommand(const std::string& name) override {}
	std::vector<TaskSettings> GetScheduledTasks() override { return {}; }
	void SetScheduledTask(const std::string& name, const std::optional<std::string>& schedule, bool enabled, const std::string& by) override {}
	void SetTaskLastScheduled(const std::string& name, int64_t time) override {}
	void InsertTaskRun(const TaskRun& run) override {}
	nlohmann::json GetTaskRunsTable(const std::string& task, uint32_t start, uint32_t length) override { return nlohmann::json::object(); }
	std::optional<TaskRun> GetTaskRun(uint64_t id) override { return std::nullopt; }
	std::vector<TaskRun> GetLatestTaskRuns() override { return {}; }
	uint32_t PruneTaskRuns(int64_t beforeTime) override { return 0; }
	void InsertCharacterSnapshot(const CharacterSnapshot& snapshot) override {}
	std::vector<CharacterSnapshot> GetCharacterSnapshots(LWOOBJID characterId) override { return {}; }
	std::optional<CharacterSnapshot> GetCharacterSnapshot(uint64_t id) override { return std::nullopt; }
	std::map<LWOOBJID, std::string> GetLatestSnapshotHashes() override { return {}; }
	uint32_t PruneCharacterSnapshots(int64_t beforeTime, uint32_t keep) override { return 0; }
	void InsertAccountNote(const AccountNote& note) override {}
	std::vector<AccountNote> GetAccountNotes(uint32_t accountId) override { return {}; }
	std::optional<AccountNote> GetAccountNote(uint64_t id) override { return {}; }
	void DeleteAccountNote(uint64_t id) override {}
	uint64_t InsertStrike(const Strike& strike) override { return 0; }
	std::vector<Strike> GetStrikes(uint32_t accountId) override { return {}; }
	std::optional<Strike> GetStrike(uint64_t id) override { return {}; }
	void RevokeStrike(uint64_t id, const std::string& revokedBy, const std::string& reason, int64_t time) override {}
	uint32_t CountActiveStrikes(uint32_t accountId, int64_t since) override { return 0; }
	uint32_t GetAccountWarningCount(uint32_t accountId) override { return 0; }
	void SetAccountBan(uint32_t accountId, bool banned, int64_t expires, const std::string& reason) override {}
	std::vector<uint32_t> LiftExpiredBans(int64_t now) override { return {}; }
	void InsertHealthSample(const HealthSample& sample) override {}
	std::vector<HealthSample> GetHealthSamples(int64_t from, int64_t to, int64_t bucketSeconds) override { return {}; }
	uint32_t PruneHealthSamples(int64_t beforeTime) override { return 0; }
	void InsertInstanceSamples(const std::vector<InstanceSample>& samples) override {}
	std::vector<InstanceSample> GetInstanceSamples(int64_t from, int64_t to, int64_t bucketSeconds, uint32_t zoneId) override { return {}; }
	void InsertPositionSamples(const std::vector<PositionSample>& samples) override {}
	std::vector<PositionSample> GetPositionSamples(uint32_t zoneId, uint32_t instanceId, int64_t from, int64_t to, int64_t bucketSeconds, uint32_t limit) override { return {}; }
	std::vector<PositionInstance> GetPositionInstances(uint32_t zoneId, int64_t from, int64_t to) override { return {}; }
	uint32_t PrunePositionSamples(int64_t beforeTime) override { return 0; }
	uint64_t InsertAiSuggestion(const AiSuggestion& suggestion) override { return 0; }
	std::vector<AiSuggestion> GetAiSuggestions(const std::string& kind, int64_t itemId, uint32_t limit) override { return {}; }
	std::optional<AiSuggestion> FindAiSuggestion(const std::string& kind, int64_t itemId, const std::string& fingerprint) override { return std::nullopt; }
	AiUsage GetAiUsageSince(int64_t since) override { return {}; }
	std::optional<EconomyFlagRow> GetEconomyFlagRow(uint64_t id) override { return std::nullopt; }
	nlohmann::json GetMapCellsPerDay(uint32_t zoneId, std::optional<uint32_t> cloneId, uint8_t kind, uint32_t fromDay, uint32_t toDay, uint32_t limit) override { return nlohmann::json::array(); }
	std::vector<ScheduledAnnouncement> GetScheduledAnnouncements() override { return {}; }
	uint64_t InsertScheduledAnnouncement(const ScheduledAnnouncement& announcement) override { return 0; }
	void UpdateScheduledAnnouncement(const ScheduledAnnouncement& announcement) override {}
	void MarkAnnouncementSent(uint64_t id, int64_t time) override {}
	void DeleteScheduledAnnouncement(uint64_t id) override {}
	std::vector<ScheduledEvent> GetScheduledEvents() override { return {}; }
	uint64_t InsertScheduledEvent(const ScheduledEvent& event) override { return 0; }
	void UpdateScheduledEvent(const ScheduledEvent& event) override {}
	void DeleteScheduledEvent(uint64_t id) override {}
	std::vector<ZoneLimit> GetZoneLimits() override { return {}; }
	void SetZoneLimit(const ZoneLimit& limit) override {}
	void DeleteZoneLimit(uint32_t zoneId) override {}
	// IPropertyReputation
	VisitorHistory GetPropertyVisitorHistory(LWOOBJID propertyId, uint32_t accountId, uint32_t day, uint32_t days) override { return {}; }
	int64_t GetPropertyReputationOnDay(LWOOBJID propertyId, uint32_t day) override { return 0; }
	void AddPropertyReputation(LWOOBJID propertyId, uint32_t accountId, uint32_t day, int64_t points, int64_t seconds) override {}
	nlohmann::json GetPropertyReputationDays(LWOOBJID propertyId, uint32_t fromDay) override { return nlohmann::json::array(); }

	// IPropertyRent
	std::vector<RentRate> GetPropertyRentRates() override { return {}; }
	void SetPropertyRentRate(const RentRate& rate) override {}
	bool DeletePropertyRentRate(uint32_t mapId) override { return false; }
	std::vector<OwnedProperty> GetPropertiesOfOwner(LWOOBJID ownerId) override { return {}; }
	int64_t GetPropertyRentDue(LWOOBJID propertyId) override { return 0; }
	void SetPropertyRent(LWOOBJID propertyId, int64_t amount, int64_t due) override {}
	void SetPropertyPrivacy(LWOOBJID propertyId, int32_t privacyOption) override {}

	// IContraband
	std::vector<ContrabandItem> GetContrabandItems() override { return {}; }
	void SetContrabandItem(const ContrabandItem& item) override {}
	bool DeleteContrabandItem(LOT lot) override { return false; }

	// IFeaturedProperties
	std::vector<FeaturedSlot> GetFeaturedPropertySlots() override { return {}; }
	void SetFeaturedPropertySlot(const FeaturedSlot& slot) override {}
	FeaturedSettings GetFeaturedPropertiesSettings() override { return {}; }
	void SetFeaturedPropertiesSettings(const FeaturedSettings& settings) override {}


	// IMessageCaptures
	uint64_t InsertMessageCaptureSession(const MessageCaptureSession& session) override { return 0; }
	void UpdateMessageCaptureSession(const MessageCaptureSession& session) override {}
	void InsertMessageCaptureEntries(const std::vector<MessageCaptureRecord>& entries) override {}
	std::optional<MessageCaptureSession> GetMessageCaptureSession(uint64_t id) override { return std::nullopt; }
	std::vector<MessageCaptureSession> GetMessageCaptureSessions(const SessionQuery& query) override { return {}; }
	uint64_t CountMessageCaptureSessions(const SessionQuery& query) override { return 0; }
	std::vector<MessageCaptureRecord> GetMessageCaptureEntries(uint64_t sessionId, uint32_t afterSeq, uint32_t limit) override { return {}; }
	void DeleteMessageCaptureSession(uint64_t id) override {}

	// ILiveOps
	std::vector<LiveEvent> GetLiveEvents(bool activeOnly, uint32_t limit) override { return {}; }
	std::optional<LiveEvent> GetLiveEvent(uint64_t id) override { return std::nullopt; }
	uint64_t InsertLiveEvent(const LiveEvent& event) override { return 0; }
	bool EndLiveEvent(uint64_t id, eLiveEventState state, int64_t endedAt, const std::string& endedBy, const std::string& reason) override { return false; }
	std::vector<LiveEventInstance> GetLiveEventInstances(const std::vector<uint64_t>& eventIds) override { return {}; }
	void SetLiveEventInstance(const LiveEventInstance& instance) override {}
	void AddLiveEventScores(uint64_t eventId, const std::vector<std::pair<LWOOBJID, int64_t>>& scores, int64_t time) override {}
	std::vector<LiveOpsScore> GetLiveEventScores(uint64_t eventId, uint32_t limit) override { return {}; }
	std::vector<Challenge> GetChallenges(bool openOnly) override { return {}; }
	std::optional<Challenge> GetChallenge(uint64_t id) override { return std::nullopt; }
	uint64_t InsertChallenge(const Challenge& challenge) override { return 0; }
	void UpdateChallenge(const Challenge& challenge) override {}
	void DeleteChallenge(uint64_t id) override {}
	void AddChallengeContributions(const std::vector<Contribution>& contributions, int64_t time) override {}
	std::map<uint64_t, ChallengeTotal> GetChallengeTotals(const std::vector<uint64_t>& challengeIds) override { return {}; }
	std::vector<LiveOpsScore> GetChallengeContributions(uint64_t challengeId, uint32_t limit) override { return {}; }
	std::map<uint64_t, int64_t> GetCharacterContributions(LWOOBJID characterId) override { return {}; }
	std::vector<Reward> GetChallengeRewards(uint64_t challengeId) override { return {}; }
	void InsertChallengeReward(const Reward& reward) override {}
	std::vector<Reward> GetUnclaimedChallengeCoins(LWOOBJID characterId) override { return {}; }
	bool ClaimChallengeCoins(uint64_t challengeId, LWOOBJID characterId, int64_t time) override { return false; }
	nlohmann::json GetPropertiesOwnedBy(LWOOBJID characterId) override { return nlohmann::json::array(); }
	nlohmann::json GetBugReportsBy(LWOOBJID characterId, uint32_t limit) override { return nlohmann::json::array(); }
	nlohmann::json GetEconomyFlagsFor(LWOOBJID characterId, uint32_t limit) override { return nlohmann::json::array(); }
	nlohmann::json GetFriendsOf(LWOOBJID characterId) override { return nlohmann::json::array(); }
	nlohmann::json GetCheatDetectionsFor(uint32_t accountId, uint32_t limit) override { return nlohmann::json::array(); }
	nlohmann::json GetAuditAbout(uint32_t accountId, uint32_t limit) override { return nlohmann::json::array(); }
	nlohmann::json GetPropertyRecord(LWOOBJID propertyId) override { return nlohmann::json::object(); }
	nlohmann::json GetPropertyModelRecords(LWOOBJID propertyId) override { return nlohmann::json::array(); }
	uint64_t InsertChatMessage(const ChatMessage& message) override { return 0; }
	std::vector<ChatMessage> GetChatMessages(const ChatQuery& query) override { return {}; }
	uint64_t CountChatMessages(const ChatQuery& query) override { return 0; }
	void InsertModerationDecision(const std::string& kind, int64_t subjectId, const std::string& subject, bool approved, const std::string& reason, int64_t time) override {}
	nlohmann::json GetModerationDecisions(const std::string& kind, int64_t subjectId, uint32_t limit) override { return nlohmann::json::array(); }
	Totp GetTotp(uint32_t accountId) override { return {}; }
	void SetTotp(uint32_t accountId, const std::string& encryptedSecret, int64_t enabledAt) override {}
	bool UseTotpStep(uint32_t accountId, int64_t step) override { return false; }
	void ReplaceRecoveryCodes(uint32_t accountId, const std::vector<std::string>& codeHashes) override {}
	bool UseRecoveryCode(uint32_t accountId, const std::string& codeHash) override { return false; }
	uint32_t GetRecoveryCodesLeft(uint32_t accountId) override { return 0; }
	nlohmann::json GetBugReportsAfter(uint32_t afterId, uint32_t limit) override { return nlohmann::json::array(); }
	uint32_t GetMaxBugReportId() override { return 0; }
	nlohmann::json GetTransfersForCharacters(const std::vector<LWOOBJID>& characterIds, uint32_t start, uint32_t length) override { return nlohmann::json::array(); }
	nlohmann::json GetMapZones(eMapEvent kind, uint32_t fromDay, uint32_t toDay) override { return nlohmann::json::array(); };
	nlohmann::json GetMapZonesAllKinds(uint32_t fromDay, uint32_t toDay) override { return nlohmann::json::array(); };
	nlohmann::json GetMapLots(uint32_t zone, std::optional<uint32_t> clone, eMapEvent kind, uint32_t fromDay, uint32_t toDay, uint32_t limit) override { return nlohmann::json::array(); };
	nlohmann::json GetMapCells(uint32_t zone, std::optional<uint32_t> clone, eMapEvent kind, uint32_t fromDay, uint32_t toDay, LOT lot) override { return nlohmann::json::array(); };
	nlohmann::json GetMapEventsPerDay(uint32_t fromDay, uint32_t toDay, const PlaceFilter& place) override { return nlohmann::json::array(); };
	nlohmann::json GetPlayerStatsPerDay(uint32_t fromDay, uint32_t toDay, bool excludeStaff, const PlaceFilter& place) override { return nlohmann::json::array(); };
	nlohmann::json GetPlayerStatsPerZone(uint32_t fromDay, uint32_t toDay, bool excludeStaff) override { return nlohmann::json::array(); };
	nlohmann::json GetMapEventsByLot(eMapEvent kind, uint32_t fromDay, uint32_t toDay, const PlaceFilter& place, uint32_t limit) override { return nlohmann::json::array(); }
	nlohmann::json GetCloneOwners(const std::vector<uint32_t>& clones) override { return { {"owners", nlohmann::json::array()}, {"properties", nlohmann::json::array()} }; }
	nlohmann::json GetCloneVisitors(uint32_t zoneId, uint32_t cloneId, int64_t from, int64_t to, uint32_t limit) override { return nlohmann::json::array(); }
	uint32_t ApprovePreviouslyApprovedPetNames() override { return 0; };
	std::vector<std::pair<LWOOBJID, std::string>> GetAllPetNames() override { return {}; };
	void ForEachCharacterXml(const std::function<void(LWOOBJID, const std::string&)>& visit) override {};
	void ForEachCharacterXmlContaining(const std::string& needle, const std::function<void(LWOOBJID, const std::string&)>& visit) override {};
	uint32_t FixPropertyCloneIds() override { return 0; };
	std::optional<IAccountEmails::EmailInfo> GetAccountEmail(const uint32_t accountId) override { return std::nullopt; };
	void SetAccountEmail(const uint32_t accountId, const std::string_view email, const bool confirmed) override {};
	std::optional<uint32_t> GetAccountIdByConfirmedEmail(const std::string_view email) override { return std::nullopt; };
	void InsertAccountToken(const std::string_view tokenHash, const uint32_t accountId, const std::string_view purpose, const std::string_view data, const int64_t expiresAt) override {};
	std::optional<IAccountEmails::AccountToken> ConsumeAccountToken(const std::string_view tokenHash, const std::string_view purpose) override { return std::nullopt; };
	void DeleteAccountTokens(const uint32_t accountId, const std::string_view purpose) override {};
	void DeleteExpiredAccountTokens() override {};
	int64_t GetSessionsValidAfter(const uint32_t accountId) override { return 0; };
	void SetSessionsValidAfter(const uint32_t accountId, const int64_t time) override {};
	std::optional<LWOOBJID> GetModelPropertyId(const LWOOBJID modelID) override { return std::nullopt; };
	std::optional<int32_t> GetRedeemablePlayKeyId(const std::string_view keyString) override { return std::nullopt; };
	void SetAccountPlayKey(const uint32_t accountId, const int32_t playkeyId) override {};
	void DeleteBugReport(const uint32_t id) override {};
	uint32_t GetBugReportCount() override { return 0; };

	std::string GetActivityLogTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true) override { return "{}"; };
	uint32_t GetActivityLogCount() override { return 0; };
	std::string GetCommandLogTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true) override { return "{}"; };
	std::string GetPetNamesTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true, bool pendingOnly = false) override { return "{}"; };
	void ApprovePetName(const int64_t id) override {};
	void RejectPetName(const int64_t id) override {};
	std::vector<LWOOBJID> GetPetsWithUnknownOwner() override { return {}; }
	void SetPetOwner(const LWOOBJID petId, const LWOOBJID ownerId) override {}
	nlohmann::json GetAccountCharacters(uint32_t accountId) override { return nlohmann::json::array(); };
	void ApproveProperty(const LWOOBJID propertyId) override {};
	bool IsNameInUse(const std::string_view name) override { return false; };
	nlohmann::json GetCharacterById(const LWOOBJID charId) override { return nlohmann::json::object(); };
	std::optional<IPropertyContents::Model> GetModel(const LWOOBJID modelID) override { return {}; }
	std::optional<IProperty::Info> GetPropertyInfo(const LWOOBJID id) override { return {}; }
	std::optional<IUgc::Model> GetUgcModel(const LWOOBJID ugcId) override { return {}; }
	void DeleteAccount(const uint32_t accountId) override {};
	void InsertAuditLog(uint32_t accountId, const std::string_view accountName, const std::string_view action, const std::string_view description, uint32_t targetAccountId, LWOOBJID targetCharacterId) override {};
	std::string GetAuditLogTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true) override { return "{}"; };
};

#endif  //!TESTSQLDATABASE_H
