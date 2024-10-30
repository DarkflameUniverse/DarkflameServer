#ifndef ERENAMERESPONSE_H
#define ERENAMERESPONSE_H

#include <cstdint>

//! An enum for character rename responses
enum class eRenameResponse : uint8_t {
	SUCCESS = 0,
	UNKNOWN_ERROR,
	NAME_UNAVAILABLE,
	NAME_IN_USE
};


#endif  //!ERENAMERESPONSE_H
