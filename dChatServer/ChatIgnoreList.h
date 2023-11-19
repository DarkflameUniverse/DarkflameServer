#ifndef __CHATIGNORELIST__H__
#define __CHATIGNORELIST__H__

struct Packet;

#include <cstdint>

namespace ChatIgnoreList {
	void GetIgnoreList(Packet* packet);
	void AddIgnore(Packet* packet);
	void RemoveIgnore(Packet* packet);

	enum class Response : uint8_t {
		ADD_IGNORE = 32,
		REMOVE_IGNORE = 33,
		GET_IGNORE = 34,
	};

	enum class AddResponse : uint8_t {
		SUCCESS,
		ALREADY_IGNORED,
		PLAYER_NOT_FOUND,
		GENERAL_ERROR,
	};
};

#endif  //!__CHATIGNORELIST__H__
