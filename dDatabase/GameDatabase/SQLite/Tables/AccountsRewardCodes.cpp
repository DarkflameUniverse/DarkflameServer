#include "SQLiteDatabase.h"

void SQLiteDatabase::InsertRewardCode(const uint32_t account_id, const uint32_t reward_code) {
	ExecuteInsert("INSERT OR IGNORE INTO accounts_rewardcodes (account_id, rewardcode) VALUES (?, ?);", account_id, reward_code);
}

std::vector<uint32_t> SQLiteDatabase::GetRewardCodesByAccountID(const uint32_t account_id) {
	auto [_, result] = ExecuteSelect("SELECT rewardcode FROM accounts_rewardcodes WHERE account_id = ?;", account_id);

	std::vector<uint32_t> toReturn;
	while (!result.eof()) {
		toReturn.push_back(result.getIntField("rewardcode"));
		result.nextRow();
	}

	return toReturn;
}
