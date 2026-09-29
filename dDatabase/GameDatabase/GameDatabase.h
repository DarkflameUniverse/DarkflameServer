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
#include "IIgnoreList.h"
#include "IAccountsRewardCodes.h"
#include "IBehaviors.h"
#include "IUgcModularBuild.h"
#include "IAuditLog.h"
#include "IDashboardStats.h"
#include "IAccountEmails.h"
#include "IDashboardMaintenance.h"
#include "IEconomyLedger.h"
#include "IDashboardAdmin.h"
#include "IServerConfig.h"
#include "ISlashCommands.h"
#include "IModeration.h"
#include "IScheduledTasks.h"
#include "ICharacterSnapshots.h"
#include "IAccountNotes.h"
#include "IAccountStrikes.h"
#include "IServerHealth.h"
#include "IChatLog.h"
#include "IRelatedData.h"
#include "IServerOperations.h"
#include "IPlayerPositions.h"
#include "IAiSuggestions.h"
#include "ILiveOps.h"
#include "IFeaturedProperties.h"
#include "IMessageCaptures.h"
#include "IContraband.h"
#include "IPropertyRent.h"
#include "IPropertyReputation.h"
#include "IBbbAutosave.h"
#include "IServerTraffic.h"
#include "IApiKeys.h"
#include "IUgcLookup.h"
#include "IPermissionGrants.h"
#include "IGuilds.h"

#ifdef _DEBUG
#  define DLU_SQL_TRY_CATCH_RETHROW(x) do { try { x; } catch (std::exception& ex) { LOG("SQL Error: %s", ex.what()); throw; } } while(0)
#else
#  define DLU_SQL_TRY_CATCH_RETHROW(x) x
#endif // _DEBUG

class GameDatabase :
	public IPlayKeys, public ILeaderboard, public IObjectIdTracker, public IServers,
	public IMail, public ICommandLog, public IPlayerCheatDetections, public IBugReports,
	public IPropertyContents, public IProperty, public IPetNames, public ICharXml,
	public IMigrationHistory, public IUgc, public IFriends, public ICharInfo,
	public IAccounts, public IActivityLog, public IAccountsRewardCodes, public IIgnoreList,
	public IBehaviors, public IUgcModularBuild, public IAuditLog, public IDashboardStats, public IAccountEmails, public IDashboardMaintenance, public IEconomyLedger, public IDashboardAdmin, public IServerConfig, public IScheduledTasks, public ICharacterSnapshots, public IAccountNotes, public IServerHealth, public IRelatedData, public IChatLog, public IAccountStrikes, public ISlashCommands, public IModeration, public IServerOperations, public IPlayerPositions, public IAiSuggestions, public ILiveOps, public IFeaturedProperties, public IMessageCaptures, public IContraband, public IPropertyRent, public IPropertyReputation, public IBbbAutosave, public IServerTraffic, public IApiKeys, public IUgcLookup, public IPermissionGrants, public IGuilds {
public:
	virtual ~GameDatabase() = default;
	// TODO: These should be made private.
	virtual void Connect() = 0;
	virtual void Destroy(std::string source = "") = 0;
	virtual void ExecuteCustomQuery(const std::string_view query) = 0;
	virtual void Commit() = 0;
	virtual bool GetAutoCommit() = 0;
	virtual void SetAutoCommit(bool value) = 0;
	// Undo the open transaction (does nothing when none is open)
	virtual void Rollback() = 0;
	virtual void DeleteCharacter(const LWOOBJID characterId) = 0;
};

/**
 * Runs the statements made while it lives as one transaction: Commit() makes them permanent, and leaving the scope
 * without Commit (an exception) rolls them back, so the connection is never left inside an open transaction (which on
 * SQLite would keep the write lock and leave later saves uncommitted). Inside a transaction someone else opened it does
 * nothing: the outer owner commits or rolls back.
 */
class DatabaseTransaction {
public:
	explicit DatabaseTransaction(GameDatabase& db) : m_Db(db), m_Owns(db.GetAutoCommit()) {
		if (m_Owns) m_Db.SetAutoCommit(false);
	}
	DatabaseTransaction(const DatabaseTransaction&) = delete;
	DatabaseTransaction& operator=(const DatabaseTransaction&) = delete;

	void Commit() {
		if (m_Owns && !m_Done) {
			m_Db.Commit();
			m_Db.SetAutoCommit(true);
		}
		m_Done = true;
	}

	~DatabaseTransaction() {
		if (!m_Owns || m_Done) return;
		try {
			m_Db.Rollback();
			m_Db.SetAutoCommit(true);
		} catch (...) {
			// A destructor must not throw; the connection reports its own errors on the next statement
		}
	}

private:
	GameDatabase& m_Db;
	bool m_Owns;
	bool m_Done{};
};

#endif  //!__GAMEDATABASE__H__
