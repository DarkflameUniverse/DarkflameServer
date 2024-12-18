#include "TestSQLDatabase.h"

void TestSQLDatabase::Connect() {

}

void TestSQLDatabase::Destroy(std::string source) {

}

void TestSQLDatabase::Commit() {

}

bool TestSQLDatabase::GetAutoCommit() {
	return {};
}

void TestSQLDatabase::SetAutoCommit(bool value) {

}

void TestSQLDatabase::ExecuteCustomQuery(const std::string_view query) {

}

std::optional<IServers::MasterInfo> TestSQLDatabase::GetMasterInfo() {
	return {};
}

std::vector<std::string> TestSQLDatabase::GetApprovedCharacterNames() {
	return {};
}

std::vector<FriendData> TestSQLDatabase::GetFriendsList(uint32_t charID) {
	return {};
}

std::optional<IFriends::BestFriendStatus> TestSQLDatabase::GetBestFriendStatus(const uint32_t playerCharacterId, const uint32_t friendCharacterId) {
	return {};
}

void TestSQLDatabase::SetBestFriendStatus(const uint32_t playerAccountId, const uint32_t friendAccountId, const uint32_t bestFriendStatus) {

}

void TestSQLDatabase::AddFriend(const uint32_t playerAccountId, const uint32_t friendAccountId) {

}

void TestSQLDatabase::RemoveFriend(const uint32_t playerAccountId, const uint32_t friendAccountId) {

}

void TestSQLDatabase::UpdateActivityLog(const uint32_t characterId, const eActivityType activityType, const LWOMAPID mapId) {

}

void TestSQLDatabase::DeleteUgcModelData(const LWOOBJID& modelId) {

}

void TestSQLDatabase::UpdateUgcModelData(const LWOOBJID& modelId, std::istringstream& lxfml) {

}

std::vector<IUgc::Model> TestSQLDatabase::GetAllUgcModels() {
	return {};
}

void TestSQLDatabase::CreateMigrationHistoryTable() {

}

bool TestSQLDatabase::IsMigrationRun(const std::string_view str) {
	return {};
}

void TestSQLDatabase::InsertMigration(const std::string_view str) {

}

std::optional<ICharInfo::Info> TestSQLDatabase::GetCharacterInfo(const uint32_t charId) {
	return {};
}

std::optional<ICharInfo::Info> TestSQLDatabase::GetCharacterInfo(const std::string_view charId) {
	return {};
}

std::string TestSQLDatabase::GetCharacterXml(const uint32_t accountId) {
	return {};
}

void TestSQLDatabase::UpdateCharacterXml(const uint32_t characterId, const std::string_view lxfml) {

}

std::optional<IAccounts::Info> TestSQLDatabase::GetAccountInfo(const std::string_view username) {
	return {};
}

void TestSQLDatabase::InsertNewCharacter(const ICharInfo::Info info) {

}

void TestSQLDatabase::InsertCharacterXml(const uint32_t accountId, const std::string_view lxfml) {

}

std::vector<uint32_t> TestSQLDatabase::GetAccountCharacterIds(uint32_t accountId) {
	return {};
}

void TestSQLDatabase::DeleteCharacter(const uint32_t characterId) {

}

void TestSQLDatabase::SetCharacterName(const uint32_t characterId, const std::string_view name) {

}

void TestSQLDatabase::SetPendingCharacterName(const uint32_t characterId, const std::string_view name) {

}

void TestSQLDatabase::UpdateLastLoggedInCharacter(const uint32_t characterId) {

}

void TestSQLDatabase::SetPetNameModerationStatus(const LWOOBJID& petId, const IPetNames::Info& info) {

}

std::optional<IPetNames::Info> TestSQLDatabase::GetPetNameInfo(const LWOOBJID& petId) {
	return {};
}

std::optional<IProperty::Info> TestSQLDatabase::GetPropertyInfo(const LWOMAPID mapId, const LWOCLONEID cloneId) {
	return {};
}

void TestSQLDatabase::UpdatePropertyModerationInfo(const IProperty::Info& info) {

}

void TestSQLDatabase::UpdatePropertyDetails(const IProperty::Info& info) {

}

void TestSQLDatabase::InsertNewProperty(const IProperty::Info& info, const uint32_t templateId, const LWOZONEID& zoneId) {

}

std::vector<IPropertyContents::Model> TestSQLDatabase::GetPropertyModels(const LWOOBJID& propertyId) {
	return {};
}

void TestSQLDatabase::RemoveUnreferencedUgcModels() {

}

void TestSQLDatabase::InsertNewPropertyModel(const LWOOBJID& propertyId, const IPropertyContents::Model& model, const std::string_view name) {

}

void TestSQLDatabase::UpdateModel(const LWOOBJID& propertyId, const NiPoint3& position, const NiQuaternion& rotation, const std::array<std::pair<int32_t, std::string>, 5>& behaviors) {

}

void TestSQLDatabase::RemoveModel(const LWOOBJID& modelId) {

}

void TestSQLDatabase::UpdatePerformanceCost(const LWOZONEID& zoneId, const float performanceCost) {

}

void TestSQLDatabase::InsertNewBugReport(const IBugReports::Info& info) {

}

void TestSQLDatabase::InsertCheatDetection(const IPlayerCheatDetections::Info& info) {

}

void TestSQLDatabase::InsertNewMail(const IMail::MailInfo& mail) {

}

void TestSQLDatabase::InsertNewUgcModel(std::istringstream& sd0Data, const uint32_t blueprintId, const uint32_t accountId, const uint32_t characterId) {

}

std::vector<IMail::MailInfo> TestSQLDatabase::GetMailForPlayer(const uint32_t characterId, const uint32_t numberOfMail) {
	return {};
}

std::optional<IMail::MailInfo> TestSQLDatabase::GetMail(const uint64_t mailId) {
	return {};
}

uint32_t TestSQLDatabase::GetUnreadMailCount(const uint32_t characterId) {
	return {};
}

void TestSQLDatabase::MarkMailRead(const uint64_t mailId) {

}

void TestSQLDatabase::DeleteMail(const uint64_t mailId) {

}

void TestSQLDatabase::ClaimMailItem(const uint64_t mailId) {

}

void TestSQLDatabase::InsertSlashCommandUsage(const uint32_t characterId, const std::string_view command) {

}

void TestSQLDatabase::UpdateAccountUnmuteTime(const uint32_t accountId, const uint64_t timeToUnmute) {

}

void TestSQLDatabase::UpdateAccountBan(const uint32_t accountId, const bool banned) {

}

void TestSQLDatabase::UpdateAccountPassword(const uint32_t accountId, const std::string_view bcryptpassword) {

}

void TestSQLDatabase::InsertNewAccount(const std::string_view username, const std::string_view bcryptpassword) {

}

void TestSQLDatabase::SetMasterIp(const std::string_view ip, const uint32_t port) {

}

std::optional<uint32_t> TestSQLDatabase::GetCurrentPersistentId() {
	return {};
}

void TestSQLDatabase::InsertDefaultPersistentId() {

}

void TestSQLDatabase::UpdatePersistentId(const uint32_t id) {

}

std::optional<uint32_t> TestSQLDatabase::GetDonationTotal(const uint32_t activityId) {
	return {};
}

std::optional<bool> TestSQLDatabase::IsPlaykeyActive(const int32_t playkeyId) {
	return {};
}

std::vector<IUgc::Model> TestSQLDatabase::GetUgcModels(const LWOOBJID& propertyId) {
	return {};
}

void TestSQLDatabase::AddIgnore(const uint32_t playerId, const uint32_t ignoredPlayerId) {

}

void TestSQLDatabase::RemoveIgnore(const uint32_t playerId, const uint32_t ignoredPlayerId) {

}

std::vector<IIgnoreList::Info> TestSQLDatabase::GetIgnoreList(const uint32_t playerId) {
	return {};
}

void TestSQLDatabase::InsertRewardCode(const uint32_t account_id, const uint32_t reward_code) {

}

std::vector<uint32_t> TestSQLDatabase::GetRewardCodesByAccountID(const uint32_t account_id) {
	return {};
}

void TestSQLDatabase::AddBehavior(const IBehaviors::Info& info) {

}

std::string TestSQLDatabase::GetBehavior(const int32_t behaviorId) {
	return {};
}

void TestSQLDatabase::RemoveBehavior(const int32_t behaviorId) {

}

void TestSQLDatabase::UpdateAccountGmLevel(const uint32_t accountId, const eGameMasterLevel gmLevel) {

}

