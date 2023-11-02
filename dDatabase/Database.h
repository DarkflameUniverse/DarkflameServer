#pragma once

#include <string>
#include <conncpp.hpp>

namespace Database {
	void Connect(const std::string& host, const std::string& database, const std::string& username, const std::string& password);
	void Connect();
	void Destroy(std::string source = "", bool log = true);

	sql::Statement* CreateStmt();
	sql::PreparedStatement* CreatePreppedStmt(const std::string& query);
	void Commit();
	bool GetAutoCommit();
	void SetAutoCommit(bool value);

	std::string GetDatabase() ;
	sql::Properties GetProperties();
};
