#include "Database.h"
#include "Game.h"
#include "dConfig.h"
#include "dLogger.h"
using namespace std;

#pragma warning (disable:4251) //Disables SQL warnings

sql::Driver * Database::driver;
sql::Connection * Database::con;
sql::Properties Database::props;
std::string Database::database;

void Database::Connect(const string& host, const string& database, const string& username, const string& password) {

	//To bypass debug issues:
	std::string newHost = "tcp://" + host;
	const char* szHost = newHost.c_str();
	const char* szDatabase = database.c_str();
	const char* szUsername = username.c_str();
	const char* szPassword = password.c_str();

	driver = sql::mariadb::get_driver_instance();

	sql::Properties properties;
	properties["hostName"] = szHost;
	properties["user"] = szUsername;
	properties["password"] = szPassword;
	properties["autoReconnect"] = "true";

	Database::props = properties;
	Database::database = database;

	Database::Connect();
}

void Database::Connect() {
	con = driver->connect(Database::props);
	con->setSchema(Database::database);
}

void Database::Destroy(std::string source, bool log) {
	if (!con) return;
	
	if (log) {
		if (source != "") Game::logger->Log("Database", "Destroying MySQL connection from %s!\n", source.c_str());
		else Game::logger->Log("Database", "Destroying MySQL connection!\n");
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
		Game::logger->Log("Database", "Trying to reconnect to MySQL\n");
	}

	if (!con->isValid() || con->isClosed())
	{
		delete con;

		con = nullptr;

		Connect();
		Game::logger->Log("Database", "Trying to reconnect to MySQL from invalid or closed connection\n");
	}
	
	auto* stmt = con->prepareStatement(str);

	return stmt;
} //CreatePreppedStmt

void Database::Commit() {
	Database::con->commit();
}