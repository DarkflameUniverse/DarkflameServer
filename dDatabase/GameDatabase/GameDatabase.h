#ifndef __GAMEDATABASE__H__
#define __GAMEDATABASE__H__

#include <optional>

#include "ILeaderboard.h"
#include "IPlayerCheatDetections.h"
#include "ICommandLog.h"
#include "IMail.h"
#include "IObjectIdTracker.h"
#include "IPlayKeys.h"
#include "IServers.h"
#include "IBugReports.h"
#include "IPropertyContents.h"
#include "IProperty.h"
#include "IPetNames.h"
#include "ICharXml.h"
#include "IMigrationHistory.h"
#include "IUgc.h"
#include "IFriends.h"
#include "ICharInfo.h"
#include "IAccounts.h"
#include "IActivityLog.h"
#include "IAccountsRewardCodes.h"

namespace sql {
	class Statement;
	class PreparedStatement;
};

#ifdef _DEBUG
#  define DLU_SQL_TRY_CATCH_RETHROW(x) do { try { x; } catch (sql::SQLException& ex) { LOG("SQL Error: %s", ex.what()); throw; } } while(0)
#else
#  define DLU_SQL_TRY_CATCH_RETHROW(x) x
#endif // _DEBUG

class GameDatabase :
	public IPlayKeys, public ILeaderboard, public IObjectIdTracker, public IServers,
	public IMail, public ICommandLog, public IPlayerCheatDetections, public IBugReports,
	public IPropertyContents, public IProperty, public IPetNames, public ICharXml,
	public IMigrationHistory, public IUgc, public IFriends, public ICharInfo,
	public IAccounts, public IActivityLog, public IAccountsRewardCodes {
public:
	virtual ~GameDatabase() = default;
	// TODO: These should be made private.
	virtual void Connect() = 0;
	virtual void Destroy(std::string source = "") = 0;
	virtual void ExecuteCustomQuery(const std::string_view query) = 0;
	virtual sql::PreparedStatement* CreatePreppedStmt(const std::string& query) = 0;
	virtual void Commit() = 0;
	virtual bool GetAutoCommit() = 0;
	virtual void SetAutoCommit(bool value) = 0;
	virtual void DeleteCharacter(const uint32_t characterId) = 0;
};

#endif  //!__GAMEDATABASE__H__
