#include "MySQLDatabase.h"

#include "Database.h"
#include "Game.h"
#include "dConfig.h"
#include "Logger.h"

namespace {
	std::string databaseName;
	sql::Properties properties;
	sql::Driver* driver = nullptr;
	sql::Connection* con = nullptr;
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

void MySQLDatabase::Destroy(std::string source) {
	if (!con) return;

	if (source.empty()) LOG("Destroying MySQL connection!");
	else LOG("Destroying MySQL connection from %s!", source.c_str());

	con->close();
	delete con;
	con = nullptr;
}

void MySQLDatabase::ExecuteCustomQuery(const std::string_view query) {
	std::unique_ptr<sql::Statement>(con->createStatement())->execute(query.data());
}

sql::PreparedStatement* MySQLDatabase::CreatePreppedStmt(const std::string& query) {
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

	return con->prepareStatement(sql::SQLString(query.c_str(), query.length()));
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

void MySQLDatabase::DeleteCharacter(const uint32_t characterId) {
	ExecuteDelete("DELETE FROM charxml WHERE id=? LIMIT 1;", characterId);
	ExecuteDelete("DELETE FROM command_log WHERE character_id=?;", characterId);
	ExecuteDelete("DELETE FROM friends WHERE player_id=? OR friend_id=?;", characterId, characterId);
	ExecuteDelete("DELETE FROM leaderboard WHERE character_id=?;", characterId);
	ExecuteDelete("DELETE FROM properties_contents WHERE property_id IN (SELECT id FROM properties WHERE owner_id=?);", characterId);
	ExecuteDelete("DELETE FROM properties WHERE owner_id=?;", characterId);
	ExecuteDelete("DELETE FROM ugc WHERE character_id=?;", characterId);
	ExecuteDelete("DELETE FROM activity_log WHERE character_id=?;", characterId);
	ExecuteDelete("DELETE FROM mail WHERE receiver_id=?;", characterId);
	ExecuteDelete("DELETE FROM charinfo WHERE id=? LIMIT 1;", characterId);
}
