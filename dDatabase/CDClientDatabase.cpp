#include "CDClientDatabase.h"
#include "CDComponentsRegistryTable.h"

// Static Variables
static CppSQLite3DB* conn = new CppSQLite3DB();

//! Opens a connection with the CDClient
void CDClientDatabase::Connect(const std::string& filename) {
	conn->open(filename.c_str());
}

//! Queries the CDClient
CppSQLite3Query CDClientDatabase::ExecuteQuery(const std::string& query) {
	return conn->execQuery(query.c_str());
}

//! Makes prepared statements
CppSQLite3Statement CDClientDatabase::CreatePreppedStmt(const std::string& query) {
	return conn->compileStatement(query.c_str());
}
