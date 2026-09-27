#ifndef __MYSQLDATABASE__H__
#define __MYSQLDATABASE__H__

#include <conncpp.hpp>
#include <memory>

#include "GameDatabase.h"

typedef std::unique_ptr<sql::PreparedStatement>& UniquePreppedStmtRef;
typedef std::unique_ptr<sql::ResultSet> UniqueResultSet;

// This struct is used to keep the PreparedStatement alive alongside the ResultSet, since the ResultSet will be invalidated if the PreparedStatement is destroyed.
// Declaring the members in reverse order of usage to ensure the PreparedStatement is destroyed after the ResultSet. This is guaranteed by the C++ standard.
struct PreparedStmtResultSet {
	std::unique_ptr<sql::PreparedStatement> m_stmt;
	std::unique_ptr<sql::ResultSet> m_resultSet;

	PreparedStmtResultSet(sql::PreparedStatement* stmt = nullptr, sql::ResultSet* resultSet = nullptr)
		: m_stmt(stmt), m_resultSet(resultSet) {}

	sql::ResultSet* operator->() const {
		return m_resultSet.get();
	}
};

// Purposefully no definition for this to provide linker errors in the case someone tries to
// bind a parameter to a type that isn't defined.
template<typename ParamType>
inline void SetParam(UniquePreppedStmtRef stmt, const int index, const ParamType param);

// This is a function to set each parameter in a prepared statement.
// This is accomplished with a combination of parameter packing and Fold Expressions.
// The constexpr if statement is used to prevent the compiler from trying to call SetParam with 0 arguments.
template<typename... Args>
void SetParams(UniquePreppedStmtRef stmt, Args&&... args) {
	if constexpr (sizeof...(args) != 0) {
		int i = 1;
		(SetParam(stmt, i++, args), ...);
	}
}

class MySQLDatabase : public GameDatabase {
public:
	void Connect() override;
	void Destroy(std::string source = "") override;

	void Commit() override;
	bool GetAutoCommit() override;
	void SetAutoCommit(bool value) override;
	void Rollback() override;
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
	std::optional<ICharXml::CharacterXml> ClaimCharacterXml(const LWOOBJID charId) override;
	bool SaveCharacterXml(const LWOOBJID charId, const std::string_view lxfml, const uint64_t generation) override;
	uint64_t GetCharacterSaveGeneration(const LWOOBJID charId) override;
	std::string GetCharactersTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true) override;
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
	std::string GetBugReportsTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true, int8_t resolvedFilter = -1) override;
	nlohmann::json GetBugReport(const uint32_t id) override;
	void ResolveBugReport(const uint32_t id, const uint32_t resolverAccountId, const std::string_view resolution) override;
	nlohmann::json GetPlayKey(const int32_t playkeyId) override;
	void UpdatePlayKey(const int32_t playkeyId, const uint32_t uses, const std::string_view notes, const bool active) override;
	void DeletePlayKey(const int32_t playkeyId) override;
	std::vector<std::pair<LWOOBJID, std::string>> GetCharacterIdsAndNames() override;
	nlohmann::json GetPendingNamesTable(const uint32_t start, const uint32_t length) override;
	void SetCharacterPermissionMap(const LWOOBJID characterId, const uint64_t permissionMap) override;
	IDashboardStats::Snapshot GetDashboardSnapshot() override;
	void RecordEconomy(const std::vector<CurrencyFlow>& currency, const std::vector<UScoreFlow>& uscore,
		const std::vector<ItemFlow>& items, const std::vector<ItemTransfer>& transfers, const std::vector<MapEvent>& mapEvents,
		const std::vector<PlayerStat>& stats) override;
	nlohmann::json GetCurrencyFlows(uint32_t fromDay, uint32_t toDay, bool excludeStaff) override;
	nlohmann::json GetUScoreFlows(uint32_t fromDay, uint32_t toDay, bool excludeStaff) override;
	nlohmann::json GetItemFlows(uint32_t fromDay, uint32_t toDay, LOT lot, bool excludeStaff) override;
	nlohmann::json GetTopEarners(uint32_t fromDay, uint32_t toDay, uint32_t limit, bool excludeStaff) override;
	nlohmann::json GetTopItems(uint32_t fromDay, uint32_t toDay, uint32_t limit, bool excludeStaff) override;
	nlohmann::json GetTransfers(uint32_t start, uint32_t length, LWOOBJID characterId, LOT lot) override;
	nlohmann::json GetTransfersForItems(const std::vector<LWOOBJID>& itemIds) override;
	void ForEachMailAttachment(const std::function<void(const MailAttachment&)>& visit) override;
	std::vector<Webhook> GetWebhooks() override;
	std::optional<Webhook> GetWebhook(uint32_t id) override;
	void InsertWebhook(const Webhook& webhook) override;
	void UpdateWebhook(const Webhook& webhook) override;
	void DeleteWebhook(uint32_t id) override;
	void RecordWebhookResult(uint32_t id, int64_t time, int32_t status, const std::string& error) override;
	std::optional<std::string> GetDashboardState(const std::string& name) override;
	void SetDashboardState(const std::string& name, const std::string& value) override;
	void DeleteDashboardState(const std::string& name) override;
	std::string GetDashboardPreferences(uint32_t accountId) override;
	void SetDashboardPreferences(uint32_t accountId, const std::string& prefs) override;
	bool InsertEconomyFlag(const EconomyFlag& flag) override;
	nlohmann::json GetEconomyFlagsTable(uint32_t start, uint32_t length, int32_t status) override;
	void ReviewEconomyFlag(uint64_t id, eFlagStatus status, uint32_t reviewerAccountId, const std::string& note) override;
	uint32_t GetOpenEconomyFlagCount() override;
	std::vector<std::pair<LWOOBJID, int64_t>> GetDailyIncome(uint32_t day) override;
	std::map<LOT, int64_t> GetItemCreationTotals(uint32_t fromDay, uint32_t toDay) override;
	uint32_t CompactEconomy(uint32_t cutoffDay, uint32_t mapCutoffDay) override;
	uint32_t PruneTransfers(int64_t beforeTime) override;
	uint32_t PruneLog(eLog log, int64_t beforeTime) override;
	void ReportConfigFromFile(const Setting& setting) override;
	std::vector<Setting> GetServerConfig(const std::vector<std::string>& files) override;
	void SetWebConfigValue(const std::string& file, const std::string& name, const std::optional<std::string>& value, bool webWins, const std::string& by) override;
	void DeleteServerConfig(const std::string& file, const std::string& name) override;
	uint64_t InsertSettingChange(const SettingChange& change) override;
	std::vector<SettingChange> GetSettingChanges(const std::string& file, const std::string& name, uint32_t start, uint32_t length, uint64_t& total) override;
	std::optional<SettingChange> GetSettingChange(uint64_t id) override;
	// IModeration
	void SetStrikeStep(uint64_t strikeId, const std::string& step, uint32_t count) override;
	std::vector<AppliedStrikeStep> GetAppliedStrikeSteps(uint32_t accountId, int64_t since) override;
	uint64_t InsertPlayerReport(const PlayerReport& report) override;
	std::vector<PlayerReport> GetPlayerReports(const PlayerReportQuery& query) override;
	uint32_t CountPlayerReports(const PlayerReportQuery& query) override;
	std::optional<PlayerReport> GetPlayerReport(uint64_t id) override;
	void SetPlayerReportStatus(uint64_t id, uint8_t status, const std::string& handledBy, const std::string& resolution, int64_t time) override;
	std::vector<ChatFilterWord> GetChatFilterWords() override;
	void SetChatFilterWord(const ChatFilterWord& word) override;
	bool DeleteChatFilterWord(const std::string& word) override;
	void RecordLoginAddress(uint32_t accountId, const std::string& address, int64_t time) override;
	std::vector<LinkedAccount> GetLinkedAccounts(uint32_t accountId) override;
	uint32_t CountLoginAddresses(uint32_t accountId) override;
	std::vector<SlashCommand> GetSlashCommands() override;
	void SetSlashCommand(const SlashCommand& command) override;
	void DeleteSlashCommand(const std::string& name) override;
	std::vector<TaskSettings> GetScheduledTasks() override;
	void SetScheduledTask(const std::string& name, const std::optional<std::string>& schedule, bool enabled, const std::string& by) override;
	void SetTaskLastScheduled(const std::string& name, int64_t time) override;
	void InsertTaskRun(const TaskRun& run) override;
	nlohmann::json GetTaskRunsTable(const std::string& task, uint32_t start, uint32_t length) override;
	std::optional<TaskRun> GetTaskRun(uint64_t id) override;
	std::vector<TaskRun> GetLatestTaskRuns() override;
	uint32_t PruneTaskRuns(int64_t beforeTime) override;
	void InsertCharacterSnapshot(const CharacterSnapshot& snapshot) override;
	std::vector<CharacterSnapshot> GetCharacterSnapshots(LWOOBJID characterId) override;
	std::optional<CharacterSnapshot> GetCharacterSnapshot(uint64_t id) override;
	std::map<LWOOBJID, std::string> GetLatestSnapshotHashes() override;
	uint32_t PruneCharacterSnapshots(int64_t beforeTime, uint32_t keep) override;
	void InsertAccountNote(const AccountNote& note) override;
	std::vector<AccountNote> GetAccountNotes(uint32_t accountId) override;
	std::optional<AccountNote> GetAccountNote(uint64_t id) override;
	void DeleteAccountNote(uint64_t id) override;
	uint64_t InsertStrike(const Strike& strike) override;
	std::vector<Strike> GetStrikes(uint32_t accountId) override;
	std::optional<Strike> GetStrike(uint64_t id) override;
	void RevokeStrike(uint64_t id, const std::string& revokedBy, const std::string& reason, int64_t time) override;
	uint32_t CountActiveStrikes(uint32_t accountId, int64_t since) override;
	uint32_t GetAccountWarningCount(uint32_t accountId) override;
	void SetAccountBan(uint32_t accountId, bool banned, int64_t expires, const std::string& reason) override;
	std::vector<uint32_t> LiftExpiredBans(int64_t now) override;
	void InsertHealthSample(const HealthSample& sample) override;
	std::vector<HealthSample> GetHealthSamples(int64_t from, int64_t to, int64_t bucketSeconds) override;
	uint32_t PruneHealthSamples(int64_t beforeTime) override;
	void InsertInstanceSamples(const std::vector<InstanceSample>& samples) override;
	std::vector<InstanceSample> GetInstanceSamples(int64_t from, int64_t to, int64_t bucketSeconds, uint32_t zoneId) override;
	void InsertPositionSamples(const std::vector<PositionSample>& samples) override;
	std::vector<PositionSample> GetPositionSamples(uint32_t zoneId, uint32_t instanceId, int64_t from, int64_t to, int64_t bucketSeconds, uint32_t limit) override;
	std::vector<PositionInstance> GetPositionInstances(uint32_t zoneId, int64_t from, int64_t to) override;
	uint32_t PrunePositionSamples(int64_t beforeTime) override;
	uint64_t InsertAiSuggestion(const AiSuggestion& suggestion) override;
	std::vector<AiSuggestion> GetAiSuggestions(const std::string& kind, int64_t itemId, uint32_t limit) override;
	std::optional<AiSuggestion> FindAiSuggestion(const std::string& kind, int64_t itemId, const std::string& fingerprint) override;
	AiUsage GetAiUsageSince(int64_t since) override;
	std::optional<EconomyFlagRow> GetEconomyFlagRow(uint64_t id) override;
	nlohmann::json GetMapCellsPerDay(uint32_t zoneId, std::optional<uint32_t> cloneId, uint8_t kind, uint32_t fromDay, uint32_t toDay, uint32_t limit) override;
	std::vector<ScheduledAnnouncement> GetScheduledAnnouncements() override;
	uint64_t InsertScheduledAnnouncement(const ScheduledAnnouncement& announcement) override;
	void UpdateScheduledAnnouncement(const ScheduledAnnouncement& announcement) override;
	void MarkAnnouncementSent(uint64_t id, int64_t time) override;
	void DeleteScheduledAnnouncement(uint64_t id) override;
	std::vector<ScheduledEvent> GetScheduledEvents() override;
	uint64_t InsertScheduledEvent(const ScheduledEvent& event) override;
	void UpdateScheduledEvent(const ScheduledEvent& event) override;
	void DeleteScheduledEvent(uint64_t id) override;
	std::vector<ZoneLimit> GetZoneLimits() override;
	void SetZoneLimit(const ZoneLimit& limit) override;
	void DeleteZoneLimit(uint32_t zoneId) override;
	// IServerTraffic
	void InsertTrafficMinutes(const std::vector<TrafficMinute>& minutes) override;
	std::vector<TrafficMinute> GetTrafficMinutes(int64_t from, int64_t to, int64_t bucketSeconds) override;
	uint32_t PruneTrafficMinutes(int64_t beforeTime) override;
	// IBbbAutosave
	std::optional<IBbbAutosave::Info> GetBbbAutosave(const LWOOBJID characterId) override;
	void SetBbbAutosave(const LWOOBJID characterId, const IBbbAutosave::Info& info) override;
	void DeleteBbbAutosave(const LWOOBJID characterId) override;

	// IPropertyReputation
	VisitorHistory GetPropertyVisitorHistory(LWOOBJID propertyId, uint32_t accountId, uint32_t day, uint32_t days) override;
	int64_t GetPropertyReputationOnDay(LWOOBJID propertyId, uint32_t day) override;
	void AddPropertyReputation(LWOOBJID propertyId, uint32_t accountId, uint32_t day, int64_t points, int64_t seconds) override;
	nlohmann::json GetPropertyReputationDays(LWOOBJID propertyId, uint32_t fromDay) override;

	// IPropertyRent
	std::vector<RentRate> GetPropertyRentRates() override;
	void SetPropertyRentRate(const RentRate& rate) override;
	bool DeletePropertyRentRate(uint32_t mapId) override;
	std::vector<OwnedProperty> GetPropertiesOfOwner(LWOOBJID ownerId) override;
	int64_t GetPropertyRentDue(LWOOBJID propertyId) override;
	void SetPropertyRent(LWOOBJID propertyId, int64_t amount, int64_t due) override;
	void SetPropertyPrivacy(LWOOBJID propertyId, int32_t privacyOption) override;

	// IContraband
	std::vector<ContrabandItem> GetContrabandItems() override;
	void SetContrabandItem(const ContrabandItem& item) override;
	bool DeleteContrabandItem(LOT lot) override;

	// IFeaturedProperties
	std::vector<FeaturedSlot> GetFeaturedPropertySlots() override;
	void SetFeaturedPropertySlot(const FeaturedSlot& slot) override;
	FeaturedSettings GetFeaturedPropertiesSettings() override;
	void SetFeaturedPropertiesSettings(const FeaturedSettings& settings) override;


	// IMessageCaptures
	uint64_t InsertMessageCaptureSession(const MessageCaptureSession& session) override;
	void UpdateMessageCaptureSession(const MessageCaptureSession& session) override;
	void InsertMessageCaptureEntries(const std::vector<MessageCaptureRecord>& entries) override;
	std::optional<MessageCaptureSession> GetMessageCaptureSession(uint64_t id) override;
	std::vector<MessageCaptureSession> GetMessageCaptureSessions(const SessionQuery& query) override;
	uint64_t CountMessageCaptureSessions(const SessionQuery& query) override;
	std::vector<MessageCaptureRecord> GetMessageCaptureEntries(uint64_t sessionId, uint32_t afterSeq, uint32_t limit) override;
	void DeleteMessageCaptureSession(uint64_t id) override;

	// ILiveOps
	std::vector<LiveEvent> GetLiveEvents(bool activeOnly, uint32_t limit) override;
	std::optional<LiveEvent> GetLiveEvent(uint64_t id) override;
	uint64_t InsertLiveEvent(const LiveEvent& event) override;
	bool EndLiveEvent(uint64_t id, eLiveEventState state, int64_t endedAt, const std::string& endedBy, const std::string& reason) override;
	std::vector<LiveEventInstance> GetLiveEventInstances(const std::vector<uint64_t>& eventIds) override;
	void SetLiveEventInstance(const LiveEventInstance& instance) override;
	void AddLiveEventScores(uint64_t eventId, const std::vector<std::pair<LWOOBJID, int64_t>>& scores, int64_t time) override;
	std::vector<LiveOpsScore> GetLiveEventScores(uint64_t eventId, uint32_t limit) override;
	std::vector<Challenge> GetChallenges(bool openOnly) override;
	std::optional<Challenge> GetChallenge(uint64_t id) override;
	uint64_t InsertChallenge(const Challenge& challenge) override;
	void UpdateChallenge(const Challenge& challenge) override;
	void DeleteChallenge(uint64_t id) override;
	void AddChallengeContributions(const std::vector<Contribution>& contributions, int64_t time) override;
	std::map<uint64_t, ChallengeTotal> GetChallengeTotals(const std::vector<uint64_t>& challengeIds) override;
	std::vector<LiveOpsScore> GetChallengeContributions(uint64_t challengeId, uint32_t limit) override;
	std::map<uint64_t, int64_t> GetCharacterContributions(LWOOBJID characterId) override;
	std::vector<Reward> GetChallengeRewards(uint64_t challengeId) override;
	void InsertChallengeReward(const Reward& reward) override;
	std::vector<Reward> GetUnclaimedChallengeCoins(LWOOBJID characterId) override;
	bool ClaimChallengeCoins(uint64_t challengeId, LWOOBJID characterId, int64_t time) override;
	nlohmann::json GetPropertiesOwnedBy(LWOOBJID characterId) override;
	nlohmann::json GetBugReportsBy(LWOOBJID characterId, uint32_t limit) override;
	nlohmann::json GetEconomyFlagsFor(LWOOBJID characterId, uint32_t limit) override;
	nlohmann::json GetFriendsOf(LWOOBJID characterId) override;
	nlohmann::json GetCheatDetectionsFor(uint32_t accountId, uint32_t limit) override;
	nlohmann::json GetAuditAbout(uint32_t accountId, uint32_t limit) override;
	nlohmann::json GetPropertyRecord(LWOOBJID propertyId) override;
	nlohmann::json GetPropertyModelRecords(LWOOBJID propertyId) override;
	uint64_t InsertChatMessage(const ChatMessage& message) override;
	std::vector<ChatMessage> GetChatMessages(const ChatQuery& query) override;
	uint64_t CountChatMessages(const ChatQuery& query) override;
	void InsertModerationDecision(const std::string& kind, int64_t subjectId, const std::string& subject, bool approved, const std::string& reason, int64_t time) override;
	nlohmann::json GetModerationDecisions(const std::string& kind, int64_t subjectId, uint32_t limit) override;
	Totp GetTotp(uint32_t accountId) override;
	void SetTotp(uint32_t accountId, const std::string& encryptedSecret, int64_t enabledAt) override;
	bool UseTotpStep(uint32_t accountId, int64_t step) override;
	void ReplaceRecoveryCodes(uint32_t accountId, const std::vector<std::string>& codeHashes) override;
	bool UseRecoveryCode(uint32_t accountId, const std::string& codeHash) override;
	uint32_t GetRecoveryCodesLeft(uint32_t accountId) override;
	nlohmann::json GetBugReportsAfter(uint32_t afterId, uint32_t limit) override;
	uint32_t GetMaxBugReportId() override;
	nlohmann::json GetTransfersForCharacters(const std::vector<LWOOBJID>& characterIds, uint32_t start, uint32_t length) override;
	nlohmann::json GetMapZones(eMapEvent kind, uint32_t fromDay, uint32_t toDay) override;
	nlohmann::json GetMapZonesAllKinds(uint32_t fromDay, uint32_t toDay) override;
	nlohmann::json GetMapLots(uint32_t zone, std::optional<uint32_t> clone, eMapEvent kind, uint32_t fromDay, uint32_t toDay, uint32_t limit) override;
	nlohmann::json GetMapCells(uint32_t zone, std::optional<uint32_t> clone, eMapEvent kind, uint32_t fromDay, uint32_t toDay, LOT lot) override;
	nlohmann::json GetMapEventsPerDay(uint32_t fromDay, uint32_t toDay, const PlaceFilter& place) override;
	nlohmann::json GetPlayerStatsPerDay(uint32_t fromDay, uint32_t toDay, bool excludeStaff, const PlaceFilter& place) override;
	nlohmann::json GetPlayerStatsPerZone(uint32_t fromDay, uint32_t toDay, bool excludeStaff) override;
	nlohmann::json GetMapEventsByLot(eMapEvent kind, uint32_t fromDay, uint32_t toDay, const PlaceFilter& place, uint32_t limit) override;
	nlohmann::json GetCloneOwners(const std::vector<uint32_t>& clones) override;
	nlohmann::json GetCloneVisitors(uint32_t zoneId, uint32_t cloneId, int64_t from, int64_t to, uint32_t limit) override;
	uint32_t ApprovePreviouslyApprovedPetNames() override;
	std::vector<std::pair<LWOOBJID, std::string>> GetAllPetNames() override;
	void ForEachCharacterXml(const std::function<void(LWOOBJID, const std::string&)>& visit) override;
	void ForEachCharacterXmlContaining(const std::string& needle, const std::function<void(LWOOBJID, const std::string&)>& visit) override;
	uint32_t FixPropertyCloneIds() override;
	std::optional<IAccountEmails::EmailInfo> GetAccountEmail(const uint32_t accountId) override;
	void SetAccountEmail(const uint32_t accountId, const std::string_view email, const bool confirmed) override;
	std::optional<uint32_t> GetAccountIdByConfirmedEmail(const std::string_view email) override;
	void InsertAccountToken(const std::string_view tokenHash, const uint32_t accountId, const std::string_view purpose, const std::string_view data, const int64_t expiresAt) override;
	std::optional<IAccountEmails::AccountToken> ConsumeAccountToken(const std::string_view tokenHash, const std::string_view purpose) override;
	void DeleteAccountTokens(const uint32_t accountId, const std::string_view purpose) override;
	void DeleteExpiredAccountTokens() override;
	int64_t GetSessionsValidAfter(const uint32_t accountId) override;
	void SetSessionsValidAfter(const uint32_t accountId, const int64_t time) override;
	std::optional<LWOOBJID> GetModelPropertyId(const LWOOBJID modelID) override;
	std::optional<int32_t> GetRedeemablePlayKeyId(const std::string_view keyString) override;
	void SetAccountPlayKey(const uint32_t accountId, const int32_t playkeyId) override;
	void DeleteBugReport(const uint32_t id) override;
	uint32_t GetBugReportCount() override;
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
	std::string GetPlayKeysTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true) override;
	uint32_t GetPlayKeyCount() override;
	void CreatePlayKey(const std::string_view keyString, const uint32_t uses, const std::string_view notes = "") override;
	void SetPlayKeyActive(const int32_t playkeyId, const bool active) override;
	std::vector<IUgc::Model> GetUgcModels(const LWOOBJID& propertyId) override;
	void AddIgnore(const LWOOBJID playerId, const LWOOBJID ignoredPlayerId) override;
	void RemoveIgnore(const LWOOBJID playerId, const LWOOBJID ignoredPlayerId) override;
	std::vector<IIgnoreList::Info> GetIgnoreList(const LWOOBJID playerId) override;
	void InsertRewardCode(const uint32_t account_id, const uint32_t reward_code) override;
	std::vector<uint32_t> GetRewardCodesByAccountID(const uint32_t account_id) override;
	void AddBehavior(const IBehaviors::Info& info) override;
	std::string GetBehavior(const LWOOBJID behaviorId) override;
	void RemoveBehavior(const LWOOBJID characterId) override;
	void UpdateAccountGmLevel(const uint32_t accountId, const eGameMasterLevel gmLevel) override;
	IProperty::PropertyEntranceResult GetProperties(const IProperty::PropertyLookup& params) override;
	std::vector<ILeaderboard::Entry> GetDescendingLeaderboard(const uint32_t activityId) override;
	std::vector<ILeaderboard::Entry> GetAscendingLeaderboard(const uint32_t activityId) override;
	std::vector<ILeaderboard::Entry> GetNsLeaderboard(const uint32_t activityId) override;
	std::vector<ILeaderboard::Entry> GetAgsLeaderboard(const uint32_t activityId) override;
	void SaveScore(const LWOOBJID playerId, const uint32_t gameId, const Score& score) override;
	void UpdateScore(const LWOOBJID playerId, const uint32_t gameId, const Score& score) override;
	std::optional<ILeaderboard::Score> GetPlayerScore(const LWOOBJID playerId, const uint32_t gameId) override;
	void IncrementNumWins(const LWOOBJID playerId, const uint32_t gameId) override;
	void IncrementTimesPlayed(const LWOOBJID playerId, const uint32_t gameId) override;
	std::map<uint32_t, uint32_t> GetLeaderboardSizes() override;
	void DeleteLeaderboardScore(const LWOOBJID playerId, const uint32_t gameId) override;
	void ResetLeaderboard(const uint32_t gameId) override;
	void InsertUgcBuild(const std::string& modules, const LWOOBJID bigId, const std::optional<LWOOBJID> characterId) override;
	void DeleteUgcBuild(const LWOOBJID bigId) override;
	uint32_t GetAccountCount() override;
	uint32_t GetCharacterCount() override;
	void RecordFailedAttempt(const uint32_t accountId) override;
	void ClearFailedAttempts(const uint32_t accountId) override;
	void SetLockout(const uint32_t accountId, const int64_t lockoutUntil) override;
	bool IsLockedOut(const uint32_t accountId) override;
	void SetAccountLocked(const uint32_t accountId, const bool locked) override;
	uint8_t GetFailedAttempts(const uint32_t accountId) override;
	nlohmann::json GetAccountsTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true) override;
	nlohmann::json GetAccountById(uint32_t accountId) override;
	uint32_t CountActiveAccountsAtGmLevel(const uint8_t gmLevel, const uint32_t excludeAccountId) override;
	bool IsNameInUse(const std::string_view name) override;
	nlohmann::json GetCharacterById(const LWOOBJID charId) override;
	std::optional<IPropertyContents::Model> GetModel(const LWOOBJID modelID) override;
	std::optional<IUgc::Model> GetUgcModel(const LWOOBJID ugcId) override;
	std::vector<IUgc::PendingModel> GetUgcModelsToProcess(const uint32_t limit) override;
	void SetUgcModelProcessed(const LWOOBJID id, const eProcessState state, const uint32_t attempts, const std::string_view error, const bool bakeAo) override;
	std::optional<IUgc::ProcessInfo> GetUgcProcessInfo(const LWOOBJID id) override;
	uint64_t ResetUgcModelProcessing(const std::optional<LWOOBJID> id, const bool failedOnly) override;
	std::vector<IUgc::ProcessInfo> GetUgcProcessList(const std::optional<eProcessState> state, const uint32_t offset, const uint32_t limit) override;
	std::vector<std::pair<IUgc::eProcessState, uint64_t>> GetUgcProcessCounts() override;
	std::vector<IUgcModularBuild::PendingBuild> GetModularBuildsToProcess(const uint32_t limit) override;
	void SetModularBuildProcessed(const LWOOBJID id, const IUgc::eProcessState state, const uint32_t attempts, const std::string_view error) override;
	std::optional<IUgc::ProcessInfo> GetModularBuildProcessInfo(const LWOOBJID id) override;
	uint64_t ResetModularBuildProcessing(const std::optional<LWOOBJID> id, const bool failedOnly) override;
	std::vector<IUgc::ProcessInfo> GetModularBuildProcessList(const std::optional<IUgc::eProcessState> state, const uint32_t offset, const uint32_t limit) override;
	std::vector<std::pair<IUgc::eProcessState, uint64_t>> GetModularBuildProcessCounts() override;
	std::optional<IProperty::Info> GetPropertyInfo(const LWOOBJID id) override;
	std::string GetPropertiesTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true, bool pendingOnly = false) override;
	uint32_t GetPropertyCount() override;
	IProperty::ShowcaseResult GetShowcaseProperties(const IProperty::ShowcaseQuery& query) override;
	void ApproveProperty(const LWOOBJID propertyId) override;
	std::string GetActivityLogTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true) override;
	uint32_t GetActivityLogCount() override;
	std::string GetCommandLogTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true) override;
	std::string GetPetNamesTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true, bool pendingOnly = false) override;
	void ApprovePetName(const int64_t id) override;
	void RejectPetName(const int64_t id) override;
	std::vector<LWOOBJID> GetPetsWithUnknownOwner() override;
	void SetPetOwner(const LWOOBJID petId, const LWOOBJID ownerId) override;
	void SetPetLotIfMissing(const LWOOBJID petId, const LOT petLot) override;
	nlohmann::json GetAccountCharacters(uint32_t accountId) override;
	void DeleteAccount(const uint32_t accountId) override;
	void InsertAuditLog(uint32_t accountId, const std::string_view accountName, const std::string_view action, const std::string_view description, uint32_t targetAccountId, LWOOBJID targetCharacterId) override;
	std::string GetAuditLogTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true) override;
	sql::PreparedStatement* CreatePreppedStmt(const std::string& query);
private:
	// Each instance has its own connection (Database::CreateConnection makes a second one for another thread)
	sql::Connection* m_Con = nullptr;


	// Generic query functions that can be used for any query.
	// Return type may be different depending on the query, so it is up to the caller to check the return type.
	// The first argument is the query string, and the rest are the parameters to bind to the query.
	// The return type is a PreparedStmtResultSet which keeps the PreparedStatement alive alongside the ResultSet.
	template<typename... Args>
	inline PreparedStmtResultSet ExecuteSelect(const std::string& query, Args&&... args) {
		PreparedStmtResultSet toReturn;
		toReturn.m_stmt.reset(CreatePreppedStmt(query));
		SetParams(toReturn.m_stmt, std::forward<Args>(args)...);
		DLU_SQL_TRY_CATCH_RETHROW(toReturn.m_resultSet.reset(toReturn.m_stmt->executeQuery()));
		// Return the PreparedStmtResultSet, which now owns both the PreparedStatement and ResultSet via unique_ptr and will ensure they are properly cleaned up.
		return toReturn;
	}

	template<typename... Args>
	inline void ExecuteDelete(const std::string& query, Args&&... args) {
		std::unique_ptr<sql::PreparedStatement> preppedStmt(CreatePreppedStmt(query));
		SetParams(preppedStmt, std::forward<Args>(args)...);
		DLU_SQL_TRY_CATCH_RETHROW(preppedStmt->execute());
	}

	template<typename... Args>
	inline int32_t ExecuteUpdate(const std::string& query, Args&&... args) {
		std::unique_ptr<sql::PreparedStatement> preppedStmt(CreatePreppedStmt(query));
		SetParams(preppedStmt, std::forward<Args>(args)...);
		DLU_SQL_TRY_CATCH_RETHROW(return preppedStmt->executeUpdate());
	}

	template<typename... Args>
	inline bool ExecuteInsert(const std::string& query, Args&&... args) {
		std::unique_ptr<sql::PreparedStatement> preppedStmt(CreatePreppedStmt(query));
		SetParams(preppedStmt, std::forward<Args>(args)...);
		DLU_SQL_TRY_CATCH_RETHROW(return preppedStmt->execute());
	}
};

// Below are each of the definitions of SetParam for each supported type.

template<>
inline void SetParam(UniquePreppedStmtRef stmt, const int index, const std::string_view param) {
	LOG_DEBUG("%s", param.data());
	stmt->setString(index, param.data());
}

template<>
inline void SetParam(UniquePreppedStmtRef stmt, const int index, const char* param) {
	LOG_DEBUG("%s", param);
	stmt->setString(index, param);
}

template<>
inline void SetParam(UniquePreppedStmtRef stmt, const int index, const std::string param) {
	LOG_DEBUG("%s", param.c_str());
	stmt->setString(index, param.c_str());
}

template<>
inline void SetParam(UniquePreppedStmtRef stmt, const int index, const int8_t param) {
	LOG_DEBUG("%u", param);
	stmt->setByte(index, param);
}

template<>
inline void SetParam(UniquePreppedStmtRef stmt, const int index, const uint8_t param) {
	LOG_DEBUG("%d", param);
	stmt->setByte(index, param);
}

template<>
inline void SetParam(UniquePreppedStmtRef stmt, const int index, const int16_t param) {
	LOG_DEBUG("%u", param);
	stmt->setShort(index, param);
}

template<>
inline void SetParam(UniquePreppedStmtRef stmt, const int index, const uint16_t param) {
	LOG_DEBUG("%d", param);
	stmt->setShort(index, param);
}

template<>
inline void SetParam(UniquePreppedStmtRef stmt, const int index, const uint32_t param) {
	LOG_DEBUG("%u", param);
	stmt->setUInt(index, param);
}

template<>
inline void SetParam(UniquePreppedStmtRef stmt, const int index, const int32_t param) {
	LOG_DEBUG("%d", param);
	stmt->setInt(index, param);
}

template<>
inline void SetParam(UniquePreppedStmtRef stmt, const int index, const int64_t param) {
	LOG_DEBUG("%llu", param);
	stmt->setInt64(index, param);
}

template<>
inline void SetParam(UniquePreppedStmtRef stmt, const int index, const uint64_t param) {
	LOG_DEBUG("%llu", param);
	stmt->setUInt64(index, param);
}

template<>
inline void SetParam(UniquePreppedStmtRef stmt, const int index, const float param) {
	LOG_DEBUG("%f", param);
	stmt->setFloat(index, param);
}

template<>
inline void SetParam(UniquePreppedStmtRef stmt, const int index, const double param) {
	LOG_DEBUG("%f", param);
	stmt->setDouble(index, param);
}

template<>
inline void SetParam(UniquePreppedStmtRef stmt, const int index, const bool param) {
	LOG_DEBUG("%s", param ? "true" : "false");
	stmt->setBoolean(index, param);
}

template<>
inline void SetParam(UniquePreppedStmtRef stmt, const int index, const std::istream* param) {
	LOG_DEBUG("Blob");
	// This is the one time you will ever see me use const_cast.
	stmt->setBlob(index, const_cast<std::istream*>(param));
}

template<>
inline void SetParam(UniquePreppedStmtRef stmt, const int index, const std::optional<uint32_t> param) {
	if (param) {
		LOG_DEBUG("%d", param.value());
		stmt->setInt(index, param.value());
	} else {
		LOG_DEBUG("Null");
		stmt->setNull(index, sql::DataType::SQLNULL);
	}
}

template<>
inline void SetParam(UniquePreppedStmtRef stmt, const int index, const std::optional<std::string> param) {
	if (param) {
		LOG_DEBUG("%s", param.value().c_str());
		stmt->setString(index, param.value().c_str());
	} else {
		LOG_DEBUG("Null");
		stmt->setNull(index, sql::DataType::SQLNULL);
	}
}

template<>
inline void SetParam(UniquePreppedStmtRef stmt, const int index, const std::optional<LWOOBJID> param) {
	if (param) {
		LOG_DEBUG("%d", param.value());
		stmt->setInt64(index, param.value());
	} else {
		LOG_DEBUG("Null");
		stmt->setNull(index, sql::DataType::SQLNULL);
	}
}

#endif  //!__MYSQLDATABASE__H__
