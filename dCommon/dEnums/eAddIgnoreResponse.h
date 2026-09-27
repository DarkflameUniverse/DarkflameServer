#ifndef EADDIGNORERESPONSE_H
#define EADDIGNORERESPONSE_H

#include <cstdint>

enum class eAddIgnoreResponse : uint8_t {
	SUCCESS,
	ALREADY_IGNORED,
	PLAYER_NOT_FOUND,
	GENERAL_ERROR,
};

#endif // EADDIGNORERESPONSE_H
