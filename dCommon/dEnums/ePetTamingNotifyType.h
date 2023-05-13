#ifndef __EPETTAMINGNOTIFYTYPE__H__
#define __EPETTAMINGNOTIFYTYPE__H__

#include <cstdint>

enum class ePetTamingNotifyType : uint32_t {
	SUCCESS,
	QUIT,
	FAILED,
	BEGIN,
	READY,
	NAMINGPET
};

#endif  //!__EPETTAMINGNOTIFYTYPE__H__
