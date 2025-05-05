#ifndef __CHATIGNORELIST__H__
#define __CHATIGNORELIST__H__

struct Packet;

#include <cstdint>

/**
 * @brief The ignore list allows players to ignore someone silently. Requests will generally be blocked by the client, but they should also be checked
 * on the server as well so the sender can get a generic error code in response.
 *
 */
namespace ChatIgnoreList {
	void GetIgnoreList(Packet* packet);
	void AddIgnore(Packet* packet);
	void RemoveIgnore(Packet* packet);

	enum class AddResponse : uint8_t {
		SUCCESS,
		ALREADY_IGNORED,
		PLAYER_NOT_FOUND,
		GENERAL_ERROR,
	};
};

#endif  //!__CHATIGNORELIST__H__
