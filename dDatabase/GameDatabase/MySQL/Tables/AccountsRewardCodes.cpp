#include "MySQLDatabase.h"

void MySQLDatabase::InsertRewardCode(const uint32_t account_id, const uint32_t reward_code) {
	ExecuteInsert("INSERT IGNORE INTO accounts_rewardcodes (account_id, rewardcode) VALUES (?, ?);", account_id, reward_code);
}

std::vector<uint32_t> MySQLDatabase::GetRewardCodesByAccountID(const uint32_t account_id) {
	auto result = ExecuteSelect("SELECT rewardcode FROM accounts_rewardcodes WHERE account_id = ?;", account_id);

	std::vector<uint32_t> toReturn;
	toReturn.reserve(result->rowsCount());
	while (result->next()) {
		toReturn.push_back(result->getUInt("rewardcode"));
	}

	return toReturn;
}
