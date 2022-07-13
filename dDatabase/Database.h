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
	static sql::Properties props;
	static std::string database;
public:
	static void Connect(const std::string& host, const std::string& database, const std::string& username, const std::string& password);
	static void Connect();
	static void Destroy(std::string source = "", bool log = true);

	static sql::Statement* CreateStmt();
	static sql::PreparedStatement* CreatePreppedStmt(const std::string& query);
	static void Commit();

	static std::string GetDatabase() { return database; }
	static sql::Properties GetProperties() { return props; }
};
