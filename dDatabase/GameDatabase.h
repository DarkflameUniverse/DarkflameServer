#ifndef __GAMEDATABASE__H__
#define __GAMEDATABASE__H__

namespace sql {
	class Statement;
	class PreparedStatement;
};

class GameDatabase {
public:
	virtual void Connect() = 0;
	virtual void Destroy(std::string source = "", bool log = true) = 0;
	virtual sql::Statement* CreateStmt() = 0;
	virtual sql::PreparedStatement* CreatePreppedStmt(const std::string& query) = 0;
	virtual void Commit() = 0;
	virtual bool GetAutoCommit() = 0;
	virtual void SetAutoCommit(bool value) = 0;
};

#endif  //!__GAMEDATABASE__H__
