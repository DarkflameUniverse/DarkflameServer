#pragma once

#include <string>
#include <conncpp.hpp>

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
	static void Destroy();
	static sql::Statement* CreateStmt();
	static sql::PreparedStatement* CreatePreppedStmt(const std::string& query);
};
