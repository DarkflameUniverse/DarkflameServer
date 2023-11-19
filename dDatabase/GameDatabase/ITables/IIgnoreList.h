#ifndef __IIGNORELIST__H__
#define __IIGNORELIST__H__

#include <cstdint>
#include <string>
#include <vector>

class IIgnoreList {
public:
	struct Info {
		std::string name;
		uint32_t id;
	};

	virtual std::vector<Info> GetIgnoreList(const uint32_t playerId) = 0;
	virtual void AddIgnore(const uint32_t playerId, const uint32_t ignoredPlayerId) = 0;
	virtual void RemoveIgnore(const uint32_t playerId, const uint32_t ignoredPlayerId) = 0;
};

#endif  //!__IIGNORELIST__H__
