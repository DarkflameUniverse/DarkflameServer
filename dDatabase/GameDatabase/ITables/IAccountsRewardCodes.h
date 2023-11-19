#ifndef __IACCOUNTSREWARDCODES__H__
#define __IACCOUNTSREWARDCODES__H__

#include <cstdint>
#include <vector>

class IAccountsRewardCodes {
public:
	virtual void InsertRewardCode(const uint32_t account_id, const uint32_t reward_code) = 0;
	virtual std::vector<uint32_t> GetRewardCodesByAccountID(const uint32_t account_id) = 0;
};
#endif  //!__IACCOUNTSREWARDCODES__H__