#ifndef __ESQLITEDATATYPE__H__
#define __ESQLITEDATATYPE__H__

#include <cstdint>

enum class eSqliteDataType : int32_t {
	NONE = 0,
	INT32,
	REAL = 3,
	TEXT_4,
	INT_BOOL,
	INT64,
	TEXT_8 = 8
};

#endif  //!__ESQLITEDATATYPE__H__
