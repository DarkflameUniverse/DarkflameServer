#include "Database.h"

#include "dConfig.h"

#include "Databases/MySQL.h"

void Database::Connect(dConfig* config) {
	bool useSqlite = true;
	if (config->GetValue("mysql_host") != "" && config->GetValue("mysql_database") != "" && config->GetValue("mysql_username") != "" && config->GetValue("mysql_password") != "") {
		useSqlite = false;
	}

	if (useSqlite) {

	} else {
		Database::Connection = new MySQLDatabase(config->GetValue("mysql_host"), config->GetValue("mysql_database"), config->GetValue("mysql_username"), config->GetValue("mysql_password"));
		Database::ConnectionType = eConnectionTypes::MYSQL;
	}

	Database::Connection->Connect();
}

void Database::Destroy() {
	Database::Connection->Destroy();
	delete Database::Connection;
}
