#include "MySQLDatabase.h"

#include "Database.h"
#include "Game.h"
#include "dConfig.h"
#include "Logger.h"
#include "dPlatforms.h"


void MySQLDatabase::Connect() {
	LOG("Using MySQL database");
	auto* driver = sql::mariadb::get_driver_instance();
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

	const std::string databaseName = Game::config->GetValue("mysql_database");
	// `connect(const Properties& props)` segfaults in windows debug, but
	// `connect(const SQLString& host, const SQLString& user, const SQLString& pwd)` doesn't handle pipes/unix sockets correctly
#if defined(DARKFLAME_PLATFORM_WIN32) && defined(_DEBUG)
		m_Con = driver->connect(properties["hostName"].c_str(), properties["user"].c_str(), properties["password"].c_str());
#else
		m_Con = driver->connect(properties);
#endif
	m_Con->setSchema(databaseName.c_str());
}

void MySQLDatabase::Destroy(std::string source) {
	if (!m_Con) return;

	if (source.empty()) LOG("Destroying MySQL connection!");
	else LOG("Destroying MySQL connection from %s!", source.c_str());

	m_Con->close();
	delete m_Con;
	m_Con = nullptr;
}

void MySQLDatabase::ExecuteCustomQuery(const std::string_view query) {
	std::unique_ptr<sql::Statement>(m_Con->createStatement())->execute(query.data());
}

sql::PreparedStatement* MySQLDatabase::CreatePreppedStmt(const std::string& query) {
	if (!m_Con) {
		Connect();
		LOG("Trying to reconnect to MySQL");
	}

	if (!m_Con->isValid() || m_Con->isClosed()) {
		delete m_Con;

		m_Con = nullptr;

		Connect();
		LOG("Trying to reconnect to MySQL from invalid or closed connection");
	}

	return m_Con->prepareStatement(sql::SQLString(query.c_str(), query.length()));
}

void MySQLDatabase::Commit() {
	m_Con->commit();
}

bool MySQLDatabase::GetAutoCommit() {
	// TODO This should not just access a pointer.  A future PR should update this
	// to check for null and throw an error if the connection is not valid.
	return m_Con->getAutoCommit();
}

void MySQLDatabase::SetAutoCommit(bool value) {
	// TODO This should not just access a pointer.  A future PR should update this
	// to check for null and throw an error if the connection is not valid.
	m_Con->setAutoCommit(value);
}

void MySQLDatabase::Rollback() {
	m_Con->rollback();
}

void MySQLDatabase::DeleteCharacter(const LWOOBJID characterId) {
	ExecuteDelete("DELETE FROM charxml WHERE id=? LIMIT 1;", characterId);
	ExecuteDelete("DELETE FROM command_log WHERE character_id=?;", characterId);
	ExecuteDelete("DELETE FROM friends WHERE player_id=? OR friend_id=?;", characterId, characterId);
	ExecuteDelete("DELETE FROM leaderboard WHERE character_id=?;", characterId);
	ExecuteDelete("DELETE FROM properties_contents WHERE property_id IN (SELECT id FROM properties WHERE owner_id=?);", characterId);
	ExecuteDelete("DELETE FROM properties WHERE owner_id=?;", characterId);
	ExecuteDelete("DELETE FROM ugc WHERE character_id=?;", characterId);
	ExecuteDelete("DELETE FROM activity_log WHERE character_id=?;", characterId);
	ExecuteDelete("DELETE FROM mail WHERE receiver_id=?;", characterId);
	ExecuteDelete("DELETE FROM ignore_list WHERE player_id=? OR ignored_player_id=?;", characterId, characterId);
	ExecuteDelete("DELETE FROM ugc_modular_build WHERE character_id=?;", characterId);
	ExecuteDelete("DELETE FROM pet_names WHERE owner_id=?;", characterId);
	ExecuteDelete("DELETE FROM player_positions WHERE character_id=?;", characterId);
	ExecuteDelete("DELETE FROM charinfo WHERE id=? LIMIT 1;", characterId);
}
