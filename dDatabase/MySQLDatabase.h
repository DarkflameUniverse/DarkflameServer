#ifndef __MYSQLDATABASE__H__
#define __MYSQLDATABASE__H__

#include <conncpp.hpp>

#include "GameDatabase.h"

class MySQLDatabase : public GameDatabase {
public:
	void Connect() override;
	void Destroy(std::string source = "", bool log = true) override;

	sql::Statement* CreateStmt() override;
	sql::PreparedStatement* CreatePreppedStmt(const std::string& query) override;
	void Commit() override;
	bool GetAutoCommit() override;
	void SetAutoCommit(bool value) override;
};

#endif  //!__MYSQLDATABASE__H__
