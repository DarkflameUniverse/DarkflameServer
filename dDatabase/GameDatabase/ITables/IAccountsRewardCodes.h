#ifndef IACCOUNTSREWARDCODES_H
#define IACCOUNTSREWARDCODES_H

#include <cstdint>
#include <vector>

class IAccountsRewardCodes {
public:
	virtual void InsertRewardCode(const uint32_t account_id, const uint32_t reward_code) = 0;
	virtual std::vector<uint32_t> GetRewardCodesByAccountID(const uint32_t account_id) = 0;
};

#endif  //!IACCOUNTSREWARDCODES_H
