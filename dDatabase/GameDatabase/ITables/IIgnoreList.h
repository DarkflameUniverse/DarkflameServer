#ifndef __IIGNORELIST__H__
#define __IIGNORELIST__H__

#include <cstdint>
#include <string>
#include <vector>

class IIgnoreList {
public:
	struct Info {
		std::string name;
		LWOOBJID id;
	};

	virtual std::vector<Info> GetIgnoreList(const LWOOBJID playerId) = 0;
	virtual void AddIgnore(const LWOOBJID playerId, const LWOOBJID ignoredPlayerId) = 0;
	virtual void RemoveIgnore(const LWOOBJID playerId, const LWOOBJID ignoredPlayerId) = 0;
};

#endif  //!__IIGNORELIST__H__
