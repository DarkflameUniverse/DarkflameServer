#include "SQLiteDatabase.h"

void SQLiteDatabase::SetMasterInfo(const MasterInfo& info) {
	// We only want our 1 entry anyways, so we can just delete all and reinsert the one we want
	// since it would be two queries anyways.
	ExecuteDelete("DELETE FROM servers;");
	ExecuteInsert("INSERT INTO `servers` (`name`, `ip`, `port`, `state`, `version`, `master_password`) VALUES ('master', ?, ?, 0, 171022, ?)", info.ip, info.port, info.password);
}

std::optional<IServers::MasterInfo> SQLiteDatabase::GetMasterInfo() {
	auto [_, result] = ExecuteSelect("SELECT ip, port, master_password FROM servers WHERE name='master' LIMIT 1;");

	if (result.eof()) {
		return std::nullopt;
	}

	MasterInfo toReturn;

	toReturn.ip = result.getStringField("ip");
	toReturn.port = result.getIntField("port");
	toReturn.password = result.getStringField("master_password");

	return toReturn;
}
