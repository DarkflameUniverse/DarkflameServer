#include "MySQLDatabase.h"

void MySQLDatabase::SetMasterIp(const std::string_view ip, const uint32_t port) {
	// We only want our 1 entry anyways, so we can just delete all and reinsert the one we want
	// since it would be two queries anyways.
	ExecuteDelete("TRUNCATE TABLE servers;");
	ExecuteInsert("INSERT INTO `servers` (`name`, `ip`, `port`, `state`, `version`) VALUES ('master', ?, ?, 0, 171022)", ip, port);
}

std::optional<IServers::MasterInfo> MySQLDatabase::GetMasterInfo() {
	auto result = ExecuteSelect("SELECT ip, port FROM servers WHERE name='master' LIMIT 1;");

	if (!result->next()) {
		return std::nullopt;
	}

	MasterInfo toReturn;

	toReturn.ip = result->getString("ip").c_str();
	toReturn.port = result->getInt("port");

	return toReturn;
}
