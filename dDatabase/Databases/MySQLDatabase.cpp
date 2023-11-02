#include "MySQLDatabase.h"

#include "Database.h"
#include "Game.h"
#include "dConfig.h"
#include "Logger.h"

using namespace DatabaseStructs;

namespace {
	sql::Driver* driver;
	sql::Connection* con;
	sql::Properties properties;
	std::string databaseName;
};

void MySQLDatabase::Connect() {
	driver = sql::mariadb::get_driver_instance();

	// The mariadb connector is *supposed* to handle unix:// and pipe:// prefixes to hostName, but there are bugs where
	// 1) it tries to parse a database from the connection string (like in tcp://localhost:3001/darkflame) based on the
	//    presence of a /
	// 2) even avoiding that, the connector still assumes you're connecting with a tcp socket
	// So, what we do in the presence of a unix socket or pipe is to set the hostname to the protocol and localhost,
	// which avoids parsing errors while still ensuring the correct connection type is used, and then setting the appropriate
	// property manually (which the URL parsing fails to do)
	const std::string UNIX_PROTO = "unix://";
	const std::string PIPE_PROTO = "pipe://";
	std::string mysql_host = Game::config->GetValue("mysql_host");
	if (mysql_host.find(UNIX_PROTO) == 0) {
		properties["hostName"] = "unix://localhost";
		properties["localSocket"] = mysql_host.substr(UNIX_PROTO.length()).c_str();
	} else if (mysql_host.find(PIPE_PROTO) == 0) {
		properties["hostName"] = "pipe://localhost";
		properties["pipe"] = mysql_host.substr(PIPE_PROTO.length()).c_str();
	} else {
		properties["hostName"] = mysql_host.c_str();
	}
	properties["user"] = Game::config->GetValue("mysql_username").c_str();
	properties["password"] = Game::config->GetValue("mysql_password").c_str();
	properties["autoReconnect"] = "true";

	databaseName = Game::config->GetValue("mysql_database").c_str();

	// `connect(const Properties& props)` segfaults in windows debug, but
	// `connect(const SQLString& host, const SQLString& user, const SQLString& pwd)` doesn't handle pipes/unix sockets correctly
	if (properties.find("localSocket") != properties.end() || properties.find("pipe") != properties.end()) {
		con = driver->connect(properties);
	} else {
		con = driver->connect(properties["hostName"].c_str(), properties["user"].c_str(), properties["password"].c_str());
	}
	con->setSchema(databaseName.c_str());
}

void MySQLDatabase::Destroy(std::string source, bool log) {
	if (!con) return;

	if (log) {
		if (source != "") LOG("Destroying MySQL connection from %s!", source.c_str());
		else LOG("Destroying MySQL connection!");
	}

	con->close();
	delete con;
}

sql::Statement* MySQLDatabase::CreateStmt() {
	sql::Statement* toReturn = con->createStatement();
	return toReturn;
}

std::unique_ptr<sql::PreparedStatement> MySQLDatabase::CreatePreppedStmtUnique(const std::string& query) {
	auto* stmt = CreatePreppedStmt(query);
	return std::unique_ptr<sql::PreparedStatement>(stmt);
}

sql::PreparedStatement* MySQLDatabase::CreatePreppedStmt(const std::string& query) {
	const char* test = query.c_str();
	size_t size = query.length();
	sql::SQLString str(test, size);

	if (!con) {
		Connect();
		LOG("Trying to reconnect to MySQL");
	}

	if (!con->isValid() || con->isClosed()) {
		delete con;

		con = nullptr;

		Connect();
		LOG("Trying to reconnect to MySQL from invalid or closed connection");
	}

	auto* stmt = con->prepareStatement(str);

	return stmt;
}

void MySQLDatabase::Commit() {
	con->commit();
}

bool MySQLDatabase::GetAutoCommit() {
	// TODO This should not just access a pointer.  A future PR should update this
	// to check for null and throw an error if the connection is not valid.
	return con->getAutoCommit();
}

void MySQLDatabase::SetAutoCommit(bool value) {
	// TODO This should not just access a pointer.  A future PR should update this
	// to check for null and throw an error if the connection is not valid.
	con->setAutoCommit(value);
}

std::unique_ptr<sql::ResultSet> MySQLDatabase::ExecuteQueryUnique(const std::string& query) {
	auto* result = CreatePreppedStmtUnique(query)->executeQuery();
	return std::unique_ptr<sql::ResultSet>(result);
}

std::unique_ptr<sql::ResultSet> MySQLDatabase::ExecuteQueryUnique(const std::unique_ptr<sql::PreparedStatement>& query) {
	auto* result = query->executeQuery();
	return std::unique_ptr<sql::ResultSet>(result);
}

// queries
std::optional<MasterInfo> MySQLDatabase::GetMasterInfo() {

	auto result = ExecuteQueryUnique("SELECT ip, port FROM servers WHERE name='master' LIMIT 1;");

	if (!result->next()) {
		return std::nullopt;
	}

	MasterInfo toReturn;

	toReturn.ip = result->getString("ip").c_str();
	toReturn.port = result->getInt("port");

	return toReturn;
}

std::optional<ApprovedNames> MySQLDatabase::GetApprovedCharacterNames() {
	auto result = ExecuteQueryUnique("SELECT name FROM charinfo;");

	ApprovedNames toReturn;
	if (!result->next()) return std::nullopt;

	do {
		toReturn.names.push_back(result->getString("name").c_str());
	} while (result->next());

	return toReturn;
}

std::optional<FriendsList> MySQLDatabase::GetFriendsList(const uint32_t charId) {
	auto stmt = CreatePreppedStmtUnique(
		R"QUERY(
			SELECT fr.requested_player, best_friend, ci.name FROM 
			(
				SELECT CASE 
				WHEN player_id = ? THEN friend_id 
				WHEN friend_id = ? THEN player_id 
				END AS requested_player, best_friend FROM friends
			) AS fr 
			JOIN charinfo AS ci ON ci.id = fr.requested_player 
			WHERE fr.requested_player IS NOT NULL AND fr.requested_player != ?;
		)QUERY");
	stmt->setUInt(1, charId);
	stmt->setUInt(2, charId);
	stmt->setUInt(3, charId);

	FriendsList toReturn;

	auto friendsList = ExecuteQueryUnique(stmt);

	if (!friendsList->next()) {
		return std::nullopt;
	}

	toReturn.friends.reserve(friendsList->rowsCount());

	do {
		FriendData fd;
		fd.friendID = friendsList->getUInt(1);
		fd.isBestFriend = friendsList->getInt(2) == 3; // 0 = friends, 1 = left_requested, 2 = right_requested, 3 = both_accepted - are now bffs
		fd.friendName = friendsList->getString(3).c_str();

		toReturn.friends.push_back(fd);
	} while (friendsList->next());
	return toReturn;
}

bool MySQLDatabase::DoesCharacterExist(const std::string& name) {
	auto nameQuery(CreatePreppedStmtUnique("SELECT name FROM charinfo WHERE name = ? LIMIT 1;"));
	nameQuery->setString(1, name);
	auto result(nameQuery->executeQuery());
	return result->next();
}

std::optional<BestFriendStatus> MySQLDatabase::GetBestFriendStatus(const uint32_t playerAccountId, const uint32_t friendAccountId) {
	auto friendUpdate = CreatePreppedStmtUnique("SELECT * FROM friends WHERE (player_id = ? AND friend_id = ?) OR (player_id = ? AND friend_id = ?) LIMIT 1;");
	friendUpdate->setUInt(1, playerAccountId);
	friendUpdate->setUInt(2, friendAccountId);
	friendUpdate->setUInt(3, friendAccountId);
	friendUpdate->setUInt(4, playerAccountId);
	auto result(ExecuteQueryUnique(friendUpdate));

	if (!result->next()) {
		return std::nullopt;
	}

	BestFriendStatus toReturn;
	toReturn.playerAccountId = result->getUInt("player_id");
	toReturn.friendAccountId = result->getUInt("friend_id");
	toReturn.bestFriendStatus = result->getUInt("best_friend");

	return toReturn;
}

void MySQLDatabase::SetBestFriendStatus(const uint32_t playerAccountId, const uint32_t friendAccountId, const uint32_t bestFriendStatus) {
	auto updateQuery = CreatePreppedStmtUnique("UPDATE friends SET best_friend = ? WHERE (player_id = ? AND friend_id = ?) OR (player_id = ? AND friend_id = ?) LIMIT 1;");
	updateQuery->setUInt(1, bestFriendStatus);
	updateQuery->setUInt(2, playerAccountId);
	updateQuery->setUInt(3, friendAccountId);
	updateQuery->setUInt(4, friendAccountId);
	updateQuery->setUInt(5, playerAccountId);
	updateQuery->executeUpdate();
}

void MySQLDatabase::AddFriend(const uint32_t playerAccountId, const uint32_t friendAccountId) {
	auto friendUpdate = CreatePreppedStmtUnique("INSERT IGNORE INTO friends (player_id, friend_id, best_friend) VALUES (?, ?, 0);");
	friendUpdate->setUInt(1, playerAccountId);
	friendUpdate->setUInt(2, friendAccountId);
	friendUpdate->execute();
}

std::optional<uint32_t> MySQLDatabase::GetAccountIdFromCharacterName(const std::string& name) {
	auto nameQuery(CreatePreppedStmtUnique("SELECT id FROM charinfo WHERE name = ? LIMIT 1;"));
	nameQuery->setString(1, name);
	auto result(nameQuery->executeQuery());

	if (!result->next()) {
		return std::nullopt;
	}

	return result->getUInt(1);
}

void MySQLDatabase::RemoveFriend(const uint32_t playerAccountId, const uint32_t friendAccountId) {
	auto friendUpdate = CreatePreppedStmtUnique("DELETE FROM friends WHERE (player_id = ? AND friend_id = ?) OR (player_id = ? AND friend_id = ?) LIMIT 1;");
	friendUpdate->setUInt(1, playerAccountId);
	friendUpdate->setUInt(2, friendAccountId);
	friendUpdate->setUInt(3, friendAccountId);
	friendUpdate->setUInt(4, playerAccountId);
	friendUpdate->execute();
}

void MySQLDatabase::UpdateActivityLog(const uint32_t accountId, const eActivityType activityType, const LWOMAPID mapId) {
	auto activityUpdate = CreatePreppedStmtUnique("INSERT INTO activity_log (character_id, activity, time, map_id) VALUES (?, ?, ?, ?);");
	activityUpdate->setUInt(1, accountId);
	activityUpdate->setUInt(2, static_cast<uint32_t>(activityType));
	activityUpdate->setUInt(3, static_cast<uint32_t>(time(NULL)));
	activityUpdate->setUInt(4, mapId);
	activityUpdate->execute();
}

void MySQLDatabase::DeleteUgcModelData(const LWOOBJID& modelId) {
	{
		auto deleteQuery = CreatePreppedStmtUnique("DELETE FROM ugc WHERE id = ?;");
		deleteQuery->setUInt64(1, modelId);
		deleteQuery->execute();
	}
	{
		auto deleteQuery = CreatePreppedStmtUnique("DELETE FROM properties_contents WHERE ugc_id = ?;");
		deleteQuery->setUInt64(1, modelId);
		deleteQuery->execute();
	}
}

void MySQLDatabase::UpdateUgcModelData(const LWOOBJID& modelId, std::istringstream& lxfml) {
	auto update = CreatePreppedStmtUnique("UPDATE ugc SET lxfml = ? WHERE id = ?;");
	update->setBlob(1, &lxfml);
	update->setUInt64(2, modelId);
	update->executeUpdate();
}

std::vector<UgcModel> MySQLDatabase::GetUgcModels() {
	auto result = ExecuteQueryUnique("SELECT id, lxfml FROM ugc;");

	std::vector<UgcModel> models;
	models.reserve(result->rowsCount());
	while (result->next()) {
		UgcModel model;
		model.id = result->getInt64("id");

		// shoutouts mariadb
		auto blob = result->getBlob("lxfml");
		model.lxfmlData << blob->rdbuf();
		delete blob;
		models.push_back(std::move(model));
	}
	return std::move(models);
}

void MySQLDatabase::CreateMigrationHistoryTable() {
	ExecuteQueryUnique("CREATE TABLE IF NOT EXISTS migration_history (name TEXT NOT NULL, date TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP());");
}

bool MySQLDatabase::IsMigrationRun(const std::string_view str) {
	auto stmt = CreatePreppedStmtUnique("SELECT name FROM migration_history WHERE name = ?;");
	stmt->setString(1, str.data());
	return ExecuteQueryUnique(stmt)->next();
}

void MySQLDatabase::InsertMigration(const std::string_view str) {
	auto stmt = CreatePreppedStmtUnique("INSERT INTO migration_history (name) VALUES (?);");
	stmt->setString(1, str.data());
	stmt->execute();
}

std::optional<CharacterInfo> MySQLDatabase::GetCharacterInfo(const uint32_t charId) {
	auto stmt = CreatePreppedStmtUnique("SELECT name, pending_name, needs_rename, prop_clone_id, permission_map FROM charinfo WHERE id = ? LIMIT 1;");
	stmt->setUInt(1, charId);
	auto result = ExecuteQueryUnique(stmt);

	if (!result->next()) {
		return std::nullopt;
	}

	CharacterInfo toReturn;
	toReturn.name = result->getString("name").c_str();
	toReturn.pendingName = result->getString("pending_name").c_str();
	toReturn.needsRename = result->getBoolean("needs_rename");
	toReturn.cloneId = result->getUInt64("prop_clone_id");
	toReturn.permissionMap = static_cast<ePermissionMap>(result->getUInt("permission_map"));

	return toReturn;
}

std::string MySQLDatabase::GetCharacterXml(const uint32_t charId) {
	auto stmt = CreatePreppedStmtUnique("SELECT xml_data FROM charxml WHERE id = ? LIMIT 1;");
	stmt->setUInt(1, charId);
	auto result = ExecuteQueryUnique(stmt);

	if (!result->next()) {
		return "";
	}

	return result->getString("xml_data").c_str();
}

void MySQLDatabase::UpdateCharacterXml(const uint32_t charId, const std::string_view lxfml) {
	auto stmt = CreatePreppedStmtUnique("UPDATE charxml SET xml_data = ? WHERE id = ?;");
	stmt->setString(1, lxfml.data());
	stmt->setUInt(2, charId);
	stmt->executeUpdate();
}

std::optional<UserInfo> MySQLDatabase::GetUserInfo(const std::string_view username) {
	auto stmt = CreatePreppedStmtUnique("SELECT id, gm_level FROM accounts WHERE name = ? LIMIT 1;");
	stmt->setString(1, username.data());
	auto result = ExecuteQueryUnique(stmt);

	if (!result->next()) {
		return std::nullopt;
	}

	UserInfo toReturn;
	toReturn.accountId = result->getUInt("id");
	toReturn.maxGMLevel = static_cast<eGameMasterLevel>(result->getInt("gm_level"));

	return toReturn;
}

std::optional<uint32_t> MySQLDatabase::GetLastUsedCharacterId(const uint32_t accountId) {
	auto stmt = CreatePreppedStmtUnique("SELECT id FROM charinfo WHERE account_id = ? ORDER BY last_login DESC LIMIT 1;");
	stmt->setUInt(1, accountId);
	auto result = ExecuteQueryUnique(stmt);

	if (!result->next()) {
		return std::nullopt;
	}

	return result->getUInt("id");
}

bool MySQLDatabase::IsUsernameAvailable(const std::string_view username) {
	auto stmt = CreatePreppedStmtUnique("SELECT id FROM charinfo WHERE name = ? OR pending_name = ? LIMIT 1;");
	stmt->setString(1, username.data());
	stmt->setString(2, username.data());
	auto result = ExecuteQueryUnique(stmt);

	return !result->next();
}

void MySQLDatabase::InsertNewCharacter(const uint32_t accountId, const uint32_t characterId, const std::string_view name, const std::string_view pendingName) {
	auto stmt = CreatePreppedStmtUnique("INSERT INTO `charinfo`(`id`, `account_id`, `name`, `pending_name`, `needs_rename`, `last_login`) VALUES (?,?,?,?,?,?)");
	stmt->setUInt(1, characterId);
	stmt->setUInt(2, accountId);
	stmt->setString(3, name.data());
	stmt->setString(4, pendingName.data());
	stmt->setBoolean(5, false);
	stmt->setUInt64(6, time(NULL));
	stmt->execute();
}

void MySQLDatabase::InsertCharacterXml(const uint32_t accountId, const std::string_view lxfml) {
	auto stmt = CreatePreppedStmtUnique("INSERT INTO `charxml` (`id`, `xml_data`) VALUES (?,?)");
	stmt->setUInt(1, accountId);
	stmt->setString(2, lxfml.data());
	stmt->execute();
}

std::vector<uint32_t> MySQLDatabase::GetCharacterIds(const uint32_t accountId) {
	auto stmt = CreatePreppedStmtUnique("SELECT id FROM charinfo WHERE account_id = ? ORDER BY last_login DESC LIMIT 4;");
	stmt->setUInt(1, accountId);
	auto result = ExecuteQueryUnique(stmt);

	std::vector<uint32_t> toReturn;
	toReturn.reserve(result->rowsCount());
	while (result->next()) {
		toReturn.push_back(result->getUInt("id"));
	}
	return toReturn;
}

bool MySQLDatabase::IsCharacterIdInUse(const uint32_t characterId) {
	auto stmt = CreatePreppedStmtUnique("SELECT id FROM charinfo WHERE id = ?;");
	stmt->setUInt(1, characterId);
	auto result = ExecuteQueryUnique(stmt);

	return result->next();
}

void MySQLDatabase::DeleteCharacter(const uint32_t characterId) {
	{
		auto stmt = CreatePreppedStmtUnique("DELETE FROM charxml WHERE id=? LIMIT 1;");
		stmt->setUInt(1, characterId);
		stmt->execute();
	}
	{
		auto stmt = CreatePreppedStmtUnique("DELETE FROM command_log WHERE character_id=?;");
		stmt->setUInt(1, characterId);
		stmt->execute();
	}
	{
		auto stmt = CreatePreppedStmtUnique("DELETE FROM friends WHERE player_id=? OR friend_id=?;");
		stmt->setUInt(1, characterId);
		stmt->setUInt(2, characterId);
		stmt->execute();
	}
	{
		auto stmt = CreatePreppedStmtUnique("DELETE FROM leaderboard WHERE character_id=?;");
		stmt->setUInt(1, characterId);
		stmt->execute();
	}
	{
		auto stmt = CreatePreppedStmtUnique("DELETE FROM properties_contents WHERE property_id IN (SELECT id FROM properties WHERE owner_id=?);");
		stmt->setUInt(1, characterId);
		stmt->execute();
	}
	{
		auto stmt = CreatePreppedStmtUnique("DELETE FROM properties WHERE owner_id=?;");
		stmt->setUInt(1, characterId);
		stmt->execute();
	}
	{
		auto stmt = CreatePreppedStmtUnique("DELETE FROM ugc WHERE character_id=?;");
		stmt->setUInt(1, characterId);
		stmt->execute();
	}
	{
		auto stmt = CreatePreppedStmtUnique("DELETE FROM activity_log WHERE character_id=?;");
		stmt->setUInt(1, characterId);
		stmt->execute();
	}
	{
		auto stmt = CreatePreppedStmtUnique("DELETE FROM mail WHERE receiver_id=?;");
		stmt->setUInt(1, characterId);
		stmt->execute();
	}
	{
		auto stmt = CreatePreppedStmtUnique("DELETE FROM charinfo WHERE id=? LIMIT 1;");
		stmt->setUInt(1, characterId);
		stmt->execute();
	}
}

void MySQLDatabase::SetCharacterName(const uint32_t characterId, const std::string_view name) {
	auto stmt = CreatePreppedStmtUnique("UPDATE charinfo SET name = ?, pending_name = '', needs_rename = 0, last_login = ? WHERE id = ? LIMIT 1;");
	stmt->setString(1, name.data());
	stmt->setUInt64(2, time(NULL));
	stmt->setUInt(3, characterId);
	stmt->executeUpdate();
}

void MySQLDatabase::SetPendingCharacterName(const uint32_t characterId, const std::string_view name) {
	auto stmt = CreatePreppedStmtUnique("UPDATE charinfo SET pending_name = ?, needs_rename = 0, last_login = ? WHERE id = ? LIMIT 1");

	stmt->setString(1, name.data());
	stmt->setUInt64(2, time(NULL));
	stmt->setUInt(3, characterId);

	stmt->executeUpdate();
}

void MySQLDatabase::UpdateLastLoggedInCharacter(const uint32_t characterId) {
	auto stmt = CreatePreppedStmtUnique("UPDATE charinfo SET last_login = ? WHERE id = ? LIMIT 1");
	stmt->setUInt64(1, time(NULL));
	stmt->setUInt(2, characterId);
	stmt->executeUpdate();
}

void MySQLDatabase::SetPetNameModerationStatus(const LWOOBJID& petId, const std::string_view name, const int32_t approvalStatus) {
	auto stmt = CreatePreppedStmtUnique("INSERT INTO `pet_names` (`id`, `pet_name`, `approved`) VALUES (?, ?, ?) ON DUPLICATE KEY UPDATE pet_name = ?, approved = ?;");
	stmt->setUInt64(1, petId);
	stmt->setString(2, name.data());
	stmt->setInt(3, approvalStatus);
	stmt->setString(4, name.data());
	stmt->setInt(5, approvalStatus);
	stmt->execute();
}

std::optional<PetNameInfo> MySQLDatabase::GetPetNameInfo(const LWOOBJID& petId) {
	auto stmt = CreatePreppedStmtUnique("SELECT pet_name, approved FROM pet_names WHERE id = ? LIMIT 1;");
	stmt->setUInt64(1, petId);
	auto result = ExecuteQueryUnique(stmt);

	if (!result->next()) {
		return std::nullopt;
	}

	PetNameInfo toReturn;
	toReturn.petName = result->getString("pet_name").c_str();
	toReturn.approvalStatus = result->getInt("approved");

	return toReturn;
}

std::optional<PropertyInfo> MySQLDatabase::GetPropertyInfo(const uint32_t templateId, const uint32_t cloneId) {
	auto propertyLookup = CreatePreppedStmtUnique("SELECT * FROM properties WHERE template_id = ? AND clone_id = ?;");

	propertyLookup->setUInt(1, templateId);
	propertyLookup->setUInt(2, cloneId);

	auto propertyEntry = ExecuteQueryUnique(propertyLookup);

	if (!propertyEntry->next()) {
		return std::nullopt;
	}

	PropertyInfo toReturn;
	toReturn.id = propertyEntry->getUInt64("id");
	toReturn.ownerId = propertyEntry->getUInt64("owner_id");
	toReturn.cloneId = propertyEntry->getUInt64("clone_id");
	toReturn.name = propertyEntry->getString("name").c_str();
	toReturn.description = propertyEntry->getString("description").c_str();
	toReturn.privacyOption = propertyEntry->getInt("privacy_option");
	toReturn.rejectionReason = propertyEntry->getString("rejection_reason").c_str();
	toReturn.lastUpdatedTime = propertyEntry->getUInt("last_updated");
	toReturn.claimedTime = propertyEntry->getUInt("time_claimed");
	toReturn.reputation = propertyEntry->getUInt("reputation");
	
	return toReturn;
}
