#ifndef EPETTAMINGNOTIFYTYPE_H
#define EPETTAMINGNOTIFYTYPE_H

#include <cstdint>

enum class ePetTamingNotifyType : uint32_t {
	SUCCESS,
	QUIT,
	FAILED,
	BEGIN,
	READY,
	NAMINGPET
};

#endif  //!EPETTAMINGNOTIFYTYPE_H
