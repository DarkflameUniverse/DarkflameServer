#ifndef __ERENAMERESPONSE__H__
#define __ERENAMERESPONSE__H__

#include <cstdint>

//! An enum for character rename responses
enum class eRenameResponse : uint8_t {
	SUCCESS = 0,
	UNKNOWN_ERROR,
	NAME_UNAVAILABLE,
	NAME_IN_USE
};


#endif  //!__ERENAMERESPONSE__H__
