#include "Database.h"
#include "Game.h"
#include "dConfig.h"
#include "Logger.h"
using namespace std;

#pragma warning (disable:4251) //Disables SQL warnings

namespace {
	sql::Driver* driver;
	sql::Connection* con;
	sql::Properties props;
	std::string databaseName;
};

void Database::Connect(const string& host, const string& database, const string& username, const string& password) {

	//To bypass debug issues:
	const char* szDatabase = database.c_str();
	const char* szUsername = username.c_str();
	const char* szPassword = password.c_str();

	driver = sql::mariadb::get_driver_instance();

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
    if (host.find(UNIX_PROTO) == 0) {
		properties["hostName"] = "unix://localhost";
		properties["localSocket"] = host.substr(UNIX_PROTO.length()).c_str();
    } else if (host.find(PIPE_PROTO) == 0) {
		properties["hostName"] = "pipe://localhost";
		properties["pipe"] = host.substr(PIPE_PROTO.length()).c_str();
    } else {
		properties["hostName"] = host.c_str();
    }
	properties["user"] = szUsername;
	properties["password"] = szPassword;
	properties["autoReconnect"] = "true";

	props = properties;
	databaseName = database;

	Connect();
}

void Database::Connect() {
	// `connect(const Properties& props)` segfaults in windows debug, but
	// `connect(const SQLString& host, const SQLString& user, const SQLString& pwd)` doesn't handle pipes/unix sockets correctly
	if (props.find("localSocket") != props.end() || props.find("pipe") != props.end()) {
		con = driver->connect(props);
	} else {
		con = driver->connect(props["hostName"].c_str(), props["user"].c_str(), props["password"].c_str());
	}
	con->setSchema(databaseName.c_str());
}

void Database::Destroy(std::string source, bool log) {
	if (!con) return;

	if (log) {
		if (source != "") LOG("Destroying MySQL connection from %s!", source.c_str());
		else LOG("Destroying MySQL connection!");
	}

	con->close();
	delete con;
} //Destroy

sql::Statement* Database::CreateStmt() {
	sql::Statement* toReturn = con->createStatement();
	return toReturn;
} //CreateStmt

sql::PreparedStatement* Database::CreatePreppedStmt(const std::string& query) {
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
} //CreatePreppedStmt

void Database::Commit() {
	con->commit();
}

bool Database::GetAutoCommit() {
	// TODO This should not just access a pointer.  A future PR should update this
	// to check for null and throw an error if the connection is not valid.
	return con->getAutoCommit();
}

void Database::SetAutoCommit(bool value) {
	// TODO This should not just access a pointer.  A future PR should update this
	// to check for null and throw an error if the connection is not valid.
	con->setAutoCommit(value);
}
