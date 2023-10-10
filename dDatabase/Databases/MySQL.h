#pragma once

#include "DatabaseBase.h"

#include <string>
#include <conncpp.hpp>

class MySqlException : public std::runtime_error {
public:
	MySqlException() : std::runtime_error("MySQL error!") {}
	MySqlException(const std::string& msg) : std::runtime_error(msg.c_str()) {}
};

class MySQLDatabase : public DatabaseBase {
public:
	MySQLDatabase(const std::string& host, const std::string& database, const std::string& username, const std::string& password);
	~MySQLDatabase();

	void Connect() override;
	void Destroy() override;

	sql::Statement* CreateStmt();
	sql::PreparedStatement* CreatePreppedStmt(const std::string& query);
	
	std::unique_ptr<sql::PreparedStatement> CreatePreppedStmtUnique(const std::string& query);
	std::unique_ptr<sql::ResultSet> GetResultsOfStatement(sql::Statement* stmt);

	void Commit() override;
	bool GetAutoCommit() override;
	void SetAutoCommit(bool value) override;

	SocketDescriptor GetMasterServerIP() override;

	void CreateServer(const std::string& name, const std::string& ip, uint16_t port, uint32_t state, uint32_t version) override;
	void SetServerIpAndPortByName(const std::string& name, const std::string& ip, uint16_t port) override;

	void InsertIntoActivityLog(uint32_t playerId, uint32_t activityId, uint32_t timestamp, uint32_t zoneId) override;
	void InsertIntoCommandLog(uint32_t playerId, const std::string& command) override;

	CharacterInfo GetCharacterInfoByID(uint32_t id) override;
	CharacterInfo GetCharacterInfoByName(const std::string& name) override;
	std::string GetCharacterXMLByID(uint32_t id) override;
	std::vector<std::string> GetAllCharacterNames() override;
	std::vector<CharacterInfo> GetAllCharactersByAccountID(uint32_t accountId) override;
	bool IsCharacterNameAvailable(const std::string& name) override;

	void CreateCharacterXML(uint32_t id, const std::string& xml) override;
	void UpdateCharacterXML(uint32_t id, const std::string& xml) override;
	void CreateCharacter(uint32_t id, uint32_t account_id, const std::string& name, const std::string& pending_name, bool needs_rename, uint64_t last_login) override;

	void DeleteCharacter(uint32_t id) override;

	bool AreBestFriends(uint32_t charId1, uint32_t charId2) override;

	AccountInfo GetAccountByName(const std::string& name) override;
	AccountInfo GetAccountByID(uint32_t id) override;
	uint32_t GetLatestCharacterOfAccount(uint32_t id) override;

	void CreatePetName(uint64_t id, const std::string& name, bool approved) override;
	
	void DeletePetName(uint64_t id) override;

	PetName GetPetName(uint64_t id) override;

	bool IsKeyActive(uint32_t id) override;

	uint32_t GetObjectIDTracker() override;

	void SetObjectIDTracker(uint32_t id) override;
private:
	std::string m_Host;
	std::string m_Database;
	std::string m_Username;
	std::string m_Password;

	sql::Connection* m_Connection;
	sql::Driver* m_Driver;
	sql::Properties m_Properties;
};
