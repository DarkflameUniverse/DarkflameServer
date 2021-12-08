#include "Database.h"
#include "Game.h"
#include "dConfig.h"
#include "dLogger.h"
using namespace std;

#pragma warning (disable:4251) //Disables SQL warnings

sql::Driver * Database::driver;
sql::Connection * Database::con;

void Database::Connect(const string& host, const string& database, const string& username, const string& password) {
	driver = sql::mariadb::get_driver_instance();

	//To bypass debug issues:
	std::string newHost = "tcp://" + host;
	const char* szDatabase = database.c_str();
    
    sql::Properties properties;
    properties["hostName"] = newHost.c_str();
    properties["user"] = username.c_str();
    properties["password"] = password.c_str();
    properties["autoReconnect"] = "true";
	
    con = driver->connect(properties);
	con->setSchema(szDatabase);
} //Connect

void Database::Destroy() {
	if (!con) return;
	cout << "Destroying MySQL connection!" << endl;
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
		//Connect to the MySQL Database
		std::string mysql_host = Game::config->GetValue("mysql_host");
		std::string mysql_database = Game::config->GetValue("mysql_database");
		std::string mysql_username = Game::config->GetValue("mysql_username");
		std::string mysql_password = Game::config->GetValue("mysql_password");

		Connect(mysql_host, mysql_database, mysql_username, mysql_password);
		Game::logger->Log("Database", "Trying to reconnect to MySQL\n");
	}

	if (!con->isValid() || con->isClosed())
	{
		delete con;

		con = nullptr;

		//Connect to the MySQL Database
		std::string mysql_host = Game::config->GetValue("mysql_host");
		std::string mysql_database = Game::config->GetValue("mysql_database");
		std::string mysql_username = Game::config->GetValue("mysql_username");
		std::string mysql_password = Game::config->GetValue("mysql_password");

		Connect(mysql_host, mysql_database, mysql_username, mysql_password);
		Game::logger->Log("Database", "Trying to reconnect to MySQL from invalid or closed connection\n");
	}
	
	auto* stmt = con->prepareStatement(str);

	return stmt;
} //CreatePreppedStmt
