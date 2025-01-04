#include "MySQLDatabase.h"

void MySQLDatabase::SetMasterInfo(const MasterInfo& info) {
	// We only want our 1 entry anyways, so we can just delete all and reinsert the one we want
	// since it would be two queries anyways.
	ExecuteDelete("TRUNCATE TABLE servers;");
	ExecuteInsert("INSERT INTO `servers` (`name`, `ip`, `port`, `state`, `version`, `master_password`) VALUES ('master', ?, ?, 0, 171022, ?)", info.ip, info.port, info.password);
}

std::optional<IServers::MasterInfo> MySQLDatabase::GetMasterInfo() {
	auto result = ExecuteSelect("SELECT ip, port, master_password FROM servers WHERE name='master' LIMIT 1;");

	if (!result->next()) {
		return std::nullopt;
	}

	MasterInfo toReturn;

	toReturn.ip = result->getString("ip").c_str();
	toReturn.port = result->getInt("port");
	toReturn.password = result->getString("master_password").c_str();

	return toReturn;
}
