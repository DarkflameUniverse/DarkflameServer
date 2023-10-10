#include "MySQL.h"

#pragma warning (disable:4251)

#include "Game.h"
#include "dConfig.h"
#include "dLogger.h"

MySQLDatabase::MySQLDatabase(const std::string& host, const std::string& database, const std::string& username, const std::string& password) {
	this->m_Host = host;
	this->m_Database = database;
	this->m_Username = username;
	this->m_Password = password;

	m_Driver = sql::mariadb::get_driver_instance();

	sql::Properties properties;
	
	// The mariadb connector is *supposed* to handle unix:// and pipe:// prefixes to hostName, but there are bugs where
	// 1) it tries to parse a database from the connection string (like in tcp://localhost:3001/darkflame) based on the
	//    presence of a /
	// 2) even avoiding that, the connector still assumes you're connecting with a tcp socket
	// So, what we do in the presence of a unix socket or pipe is to set the hostname to the protocol and localhost,
	// which avoids parsing errors while still ensuring the correct connection type is used, and then setting the appropriate
	// property manually (which the URL parsing fails to do)
	const std::string UNIX_PROTO = "unix://";
	const std::string PIPE_PROTO = "pipe://";
	if (this->m_Host.find(UNIX_PROTO) == 0) {
		properties["hostName"] = "unix://localhost";
		properties["localSocket"] = this->m_Host.substr(UNIX_PROTO.length()).c_str();
	} else if (this->m_Host.find(PIPE_PROTO) == 0) {
		properties["hostName"] = "pipe://localhost";
		properties["pipe"] = this->m_Host.substr(PIPE_PROTO.length()).c_str();
	} else {
		properties["hostName"] = this->m_Host.c_str();
	}
	properties["user"] = this->m_Username.c_str();
	properties["password"] = this->m_Password.c_str();
	properties["autoReconnect"] = "true";

	this->m_Properties = properties;
	this->m_Database = database;
}

MySQLDatabase::~MySQLDatabase() {
	this->Destroy();
}

void MySQLDatabase::Connect() {
	try {
		this->m_Driver = get_m_Driver_instance();
		this->m_Properties["hostName"] = this->m_Host;
		this->m_Properties["userName"] = this->m_Username;
		this->m_Properties["password"] = this->m_Password;
		this->m_Properties["OPT_RECONNECT"] = true;
		
		this->m_Connection = this->m_Driver->connect(this->props);
		this->m_Connection->setSchema(this->m_Database);
		
	} catch (sql::SQLException& e) {
		throw MySqlException(e.what());
	}

	if (this->m_Properties.find("localSocket") != this->m_Properties.end() || this->m_Properties.find("pipe") != Database::props.end()) {
		this->m_Connection = m_Driver->connect(this->m_Properties);
	} else {
		this->m_Connection = m_Driver->connect(
			this->m_Properties["hostName"].c_str(),
			this->m_Properties["user"].c_str(),
			this->m_Properties["password"].c_str()
		);
	}
	this->m_Connection->setSchema(this->m_Database.c_str());
}

void MySQLDatabase::Destroy() {
	if (this->m_Connection != nullptr) {
		this->m_Connection->close();
		delete this->m_Connection;
		this->m_Connection = nullptr;
	}
}

sql::Statement* MySQLDatabase::CreateStmt() {
	sql::Statement* toReturn = con->createStatement();
	return toReturn;
}

sql::PreparedStatement* MySQLDatabase::CreatePreppedStmt(const std::string& query) {
	const char* test = query.c_str();
	size_t size = query.length();
	sql::SQLString str(test, size);

	if (!this->m_Connection) {
		Connect();
		Game::logger->Log("Database", "Trying to reconnect to MySQL");
	}

	if (!con->isValid() || con->isClosed()) {
		delete this->m_Connection;

		this->m_Connection = nullptr;

		Connect();
		Game::logger->Log("Database", "Trying to reconnect to MySQL from invalid or closed connection");
	}

	auto* stmt = this->m_Connection->prepareStatement(str);

	return stmt;
}

std::unique_ptr<sql::PreparedStatement> MySQLDatabase::CreatePreppedStmtUnique(const std::string& query) {
	return CreatePreppedStmt(query);
}

std::unique_ptr<sql::ResultSet> MySQLDatabase::GetResultsOfStatement(sql::Statement* stmt) {
	std::unique_ptr<sql::ResultSet> result(stmt->executeQuery());
	return result;
}


void MySQLDatabase::Commit() {
	Database::con->commit();
}

bool MySQLDatabase::GetAutoCommit() {
	// TODO This should not just access a pointer.  A future PR should update this
	// to check for null and throw an error if the connection is not valid.
	return m_Connection->getAutoCommit();
}

void MySQLDatabase::SetAutoCommit(bool value) {
	// TODO This should not just access a pointer.  A future PR should update this
	// to check for null and throw an error if the connection is not valid.
	m_Connection->setAutoCommit(value);
}

SocketDescriptor MySQLDatabase::GetMasterServerIP() {
	auto stmt = CreatePreppedStmtUnique("SELECT ip, port FROM servers WHERE name = 'master';");
	auto res = GetResultsOfStatement(stmt.get());

	while (res->next()) {
		return SocketDescriptor(res->getInt("port"), res->getString("ip"));
	}

	return SocketDescriptor(0, "");
}

void MySQLDatabase::CreateServer(const std::string& name, const std::string& ip, uint16_t port, uint32_t state, uint32_t version) {
	auto stmt = CreatePreppedStmtUnique("INSERT INTO servers (name, ip, port, state, version) VALUES (?, ?, ?, ?, ?);");
	stmt->setString(1, name);
	stmt->setString(2, ip);
	stmt->setInt(3, port);
	stmt->setInt(4, state);
	stmt->setInt(5, version);

	stmt->execute();
}

void MySQLDatabase::SetServerIpAndPortByName(const std::string& name, const std::string& ip, uint16_t port) {
	auto stmt = CreatePreppedStmtUnique("UPDATE servers SET ip = ?, port = ? WHERE name = ?;");
	stmt->setString(1, ip);
	stmt->setInt(2, port);
	stmt->setString(3, name);

	stmt->execute();
}

std::vector<std::string> MySQLDatabase::GetAllCharacterNames() {
	auto stmt = CreatePreppedStmtUnique("SELECT name FROM charinfo;");
	auto res = GetResultsOfStatement(stmt.get());

	std::vector<std::string> names;

	while (res->next()) {
		names.push_back(res->getString("name"));
	}

	return names;
}

bool MySQLDatabase::IsCharacterNameAvailable(const std::string& name) {
	auto stmt = CreatePreppedStmtUnique("SELECT name FROM charinfo WHERE name = ? OR pending_name = ?;");
	stmt->setString(1, name);
	stmt->setString(2, name);

	auto res = GetResultsOfStatement(stmt.get());

	return !res->next();
}

void MySQLDatabase::InsertIntoActivityLog(uint32_t playerId, uint32_t activityId, uint32_t timestamp, uint32_t zoneId) {
	auto stmt = CreatePreppedStmtUnique("INSERT INTO activity_log (character_id, activity, time, map_id) VALUES (?, ?, ?, ?);");

	stmt->setUInt(1, playerId);
	stmt->setUInt(2, activityId);
	stmt->setUInt(3, timestamp);
	stmt->setUInt(4, zoneId);
	
	stmt->executeUpdate();
}

void MySQLDatabase::InsertIntoCommandLog(uint32_t playerId, const std::string& command) {
	auto stmt = CreatePreppedStmtUnique("INSERT INTO command_log (character_id, command) VALUES (?, ?);");

	stmt->setUInt(1, playerId);
	stmt->setString(2, command);

	stmt->executeUpdate();
}

CharacterInfo MySQLDatabase::GetCharacterInfoByID(uint32_t id) {
	auto stmt = CreatePreppedStmtUnique("SELECT account_id, name, pending_name, needs_rename, prop_clone_id, permission_map FROM charinfo WHERE id = ? LIMIT 1;");
	stmt->setUInt(1, id);

	auto res = GetResultsOfStatement(stmt.get());

	while (res->next()) {
		CharacterInfo info{};
		info.AccountID = res->getUInt("account_id");
		info.ID = id;
		info.Name = res->getString("name");
		info.PendingName = res->getString("pending_name");
		info.NameRejected = res->getBoolean("needs_rename");
		info.PropertyCloneID = res->getUInt("prop_clone_id");
		info.PermissionMap = res->getUInt("permission_map");

		return info;
	}

	return CharacterInfo{};
}

CharacterInfo MySQLDatabase::GetCharacterInfoByName(const std::string& name) {
	auto stmt = CreatePreppedStmtUnique("SELECT id FROM charinfo WHERE name = ?");
	stmt->setString(1, name);

	auto res = GetResultsOfStatement(stmt.get());

	while (res->next()) {
		return GetCharacterByID(res->getUInt("id"));
	}

	return CharacterInfo{};
}

uint32_t MySQLDatabase::GetLatestCharacterOfAccount(uint32_t id) {
	auto stmt = CreatePreppedStmtUnique("SELECT id FROM charinfo WHERE account_id = ? ORDER BY last_login DESC LIMIT 1;");
	stmt->setUInt(1, id);

	auto res = GetResultsOfStatement(stmt.get());

	while (res->next()) {
		return res->getUInt("id");
	}

	return 0;
}

std::string MySQLDatabase::GetCharacterXMLByID(uint32_t id) {
	auto stmt = CreatePreppedStmtUnique("SELECT xml_data FROM charxml WHERE id = ? LIMIT 1;");
	stmt->setUInt(1, id);

	auto res = GetResultsOfStatement(stmt.get());

	while (res->next()) {
		return res->getString("xml_data");
	}

	return "";
}

void MySQLDatabase::CreateCharacterXML(uint32_t id, const std::string& xml) {
	auto replaceStmt = CreatePreppedStmtUnique("INSERT INTO charxml (id, xml_data) VALUES (?, ?);");
	replaceStmt->setUInt(1, id);
	replaceStmt->setString(2, xml);

	stmt->executeUpdate();
}

void MySQLDatabase::UpdateCharacterXML(uint32_t id, const std::string& xml) {
	auto replaceStmt = CreatePreppedStmtUnique("UPDATE charxml SET xml_data = ? WHERE id = ?;");
	replaceStmt->setString(1, xml);
	replaceStmt->setUInt(2, id);

	stmt->executeUpdate();
}

void MySQLDatabase::CreateCharacter(uint32_t id, uint32_t account_id, const std::string& name, const std::string& pending_name, bool needs_rename, uint64_t last_login) {
	auto replaceStmt = CreatePreppedStmtUnique("INSERT INTO charinfo (id, account_id, name, pending_name, needs_rename, last_login) VALUES (?, ?, ?, ?, ?, ?);");
	replaceStmt->setUInt(1, id);
	replaceStmt->setUInt(2, account_id);
	replaceStmt->setString(3, name);
	replaceStmt->setString(4, pending_name);
	replaceStmt->setBoolean(5, needs_rename);
	replaceStmt->setUInt64(6, last_login);

	replaceStmt->executeUpdate();
}

void MySQLDatabase::DeleteCharacter(uint32_t id) {
	auto stmt = CreatePreppedStmtUnique("DELETE FROM charxml WHERE id = ?;");
	stmt->setUInt(1, id);
	stmt->execute();

	stmt = CreatePreppedStmtUnique("DELETE FROM command_log WHERE character_id = ?;");
	stmt->setUInt(1, id);
	stmt->execute();

	stmt = CreatePreppedStmtUnique("DELETE FROM friends WHERE player_id = ? OR friend_id = ?;");
	stmt->setUInt(1, id);
	stmt->setUInt(2, id);
	stmt->execute();

	stmt = CreatePreppedStmtUnique("DELETE FROM leaderboard WHERE character_id = ?;");
	stmt->setUInt(1, id);
	stmt->execute();

	stmt = CreatePreppedStmtUnique("DELETE FROM properties_contents WHERE property_id IN (SELECT id FROM properties WHERE owner_id = ?)");
	stmt->setUInt(1, id);
	stmt->execute();

	stmt = CreatePreppedStmtUnique("DELETE FROM properties WHERE owner_id = ?;");
	stmt->setUInt(1, id);
	stmt->execute();

	stmt = CreatePreppedStmtUnique("DELETE FROM ugc WHERE character_id = ?;");
	stmt->setUInt(1, id);
	stmt->execute();

	stmt = CreatePreppedStmtUnique("DELETE FROM activity_log WHERE character_id = ?;");
	stmt->setUInt(1, id);
	stmt->execute();

	stmt = CreatePreppedStmtUnique("DELETE FROM mail WHERE receiver_id = ?;");
	stmt->setUInt(1, id);
	stmt->execute();

	stmt = CreatePreppedStmtUnique("DELETE FROM charinfo WHERE id = ?;");
	stmt->setUInt(1, id);
	stmt->execute();
}

AccountInfo MySQLDatabase::GetAccountByName(const std::string& name) {
	auto stmt = CreatePreppedStmtUnique("SELECT id FROM accounts WHERE name = ? LIMIT 1;");
	stmt->setString(1, name);
	auto res = GetResultsOfStatement(stmt.get());

	while (res->next()) {
		return GetAccountByID(res->getUInt("id"));
	}

	return AccountInfo{};
}

bool MySQLDatabase::AreBestFriends(uint32_t goon1, uint32_t goon2) {
	auto stmt = CreatePreppedStmtUnique("SELECT * FROM friends WHERE (player_id = ? AND friend_id = ?) OR (player_id = ? AND friend_id = ?) LIMIT 1;");
	stmt->setUInt(1, goon1);
	stmt->setUInt(2, goon2);
	stmt->setUInt(3, goon2);
	stmt->setUInt(4, goon1);

	auto res = GetResultsOfStatement(stmt.get());

	while (res->next()) {
		return res->getInt("best_friend") == 3;
	}

	return false;
}

AccountInfo MySQLDatabase::GetAccountByID(uint32_t id) {
	auto stmt = CreatePreppedStmtUnique("SELECT name, password, gm_level, locked, banned, play_key_id, created_at, mute_expire FROM accounts WHERE id = ? LIMIT 1;");
	stmt->setUInt(1, id);

	auto res = GetResultsOfStatement(stmt.get());

	while (res->next()) {
		AccountInfo info{};
		info.ID = id;
		info.Name = res->getString("name");
		info.Password = res->getString("password");
		info.GMLevel = res->getUInt("gm_level");
		info.Locked = res->getBoolean("locked");
		info.Banned = res->getBoolean("banned");
		info.PlayKeyID = res->getUInt("play_key_id");
		info.CreatedAt = res->getUInt64("created_at");
		info.MuteExpire = res->getUInt64("mute_expire");
		
		return info;
	}

	return AccountInfo{};
}

std::vector<CharacterInfo> MySQLDatabase::GetAllCharactersByAccountID(uint32_t accountId) {
	auto stmt = CreatePreppedStmtUnique("SELECT id FROM charinfo WHERE account_id = ? LIMIT 4;");
	stmt->setUInt(1, accountId);

	auto res = GetResultsOfStatement(stmt.get());

	std::vector<CharacterInfo> characters;

	while (res->next()) {
		characters.push_back(GetCharacterByID(res->getUInt("id")));
	}

	return characters;
}

void MySQLDatabase::CreatePetName(uint64_t id, const std::string& name, bool approved) {
	auto stmt = CreatePreppedStmtUnique("INSERT INTO pet_names (id, name, approved) VALUES (?, ?, ?);");
	stmt->setUInt64(1, id);
	stmt->setString(2, name);
	stmt->setBoolean(3, approved);
	stmt->execute();
}

void MySQLDatabase::DeletePetName(uint64_t id) {
	auto stmt = CreatePreppedStmtUnique("DELETE FROM pet_names WHERE id = ?;");
	stmt->setUInt64(1, id);
	stmt->execute();
}

PetName MySQLDatabase::GetPetName(uint64_t id) {
	auto stmt = CreatePreppedStmtUnique("SELECT name, approved FROM pet_names WHERE id = ? LIMIT 1;");
	stmt->setUInt64(1, id);

	auto res = GetResultsOfStatement(stmt.get());

	while (res->next()) {
		PetName name{};
		name.ID = id;
		name.Name = res->getString("name");
		name.Approved = res->getBoolean("approved");

		return name;
	}

	return PetName{};
}

bool MySQLDatabase::IsKeyActive(uint32_t id) {
	auto stmt = CreatePreppedStmtUnique("SELECT * FROM play_keys WHERE id = ? AND active = 1 LIMIT 1;");
	stmt->setUInt(1, id);

	auto res = GetResultsOfStatement(stmt.get());

	while (res->next()) {
		return true;
	}

	return false;
}

uint32_t MySQLDatabase::GetObjectIDTracker() {
	auto stmt = CreatePreppedStmtUnique("SELECT last_object_id FROM object_id_tracker;");
	auto res = GetResultsOfStatement(stmt.get());

	while (res->next()) {
		uint32_t id = res->getUInt("last_object_id");

		return id;
	}

	auto stmt = CreatePreppedStmtUnique("INSERT INTO object_id_tracker (last_object_id) VALUES (1);");
	stmt->execute();

	return 1;
}

void MySQLDatabase::SetObjectIDTracker(uint32_t id) {
	auto stmt = CreatePreppedStmtUnique("UPDATE object_id_tracker SET last_object_id = ?;");
	stmt->setUInt(1, id);
	stmt->execute();
}
