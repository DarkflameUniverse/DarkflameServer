#ifndef __ESQLITEDATATYPE__H__
#define __ESQLITEDATATYPE__H__

#include <cstdint>

enum class eSqliteDataType : int32_t {
	none = 0,
	int32,
	real = 3,
	text_4,
	int_bool,
	int64,
	text_8 = 8
};

#endif  //!__ESQLITEDATATYPE__H__
