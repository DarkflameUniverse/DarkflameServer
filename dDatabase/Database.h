#pragma once

#include <string>

#if defined(MARIADB_CONNECTOR)
#include <mariadb/conncpp.hpp>
#elif defined(MYSQL_CONNECTOR)
#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/sqlstring.h>
#else
#error "Connector not defined"
#endif

class MySqlException : public std::runtime_error {
public:
	MySqlException() : std::runtime_error("MySQL error!") {}
	MySqlException(const std::string& msg) : std::runtime_error(msg.c_str()) {}
};

class Database {
private:
	static sql::Driver *driver;
	static sql::Connection *con;

public:
	static void Connect(const std::string& host, const std::string& database, const std::string& username, const std::string& password);
	static void Destroy(std::string source="");
	static sql::Statement* CreateStmt();
	static sql::PreparedStatement* CreatePreppedStmt(const std::string& query);
};
