#ifndef __EHTTPMETHODS__H__
#define __EHTTPMETHODS__H__

#include <cstdint>

#include "magic_enum.hpp"

enum class eHTTPMethod : uint16_t {
	GET,
	POST,
	PUT,
	DELETE,
	HEAD,
	CONNECT,
	OPTIONS,
	TRACE,
	PATCH,
	INVALID
};

#endif // __EHTTPMETHODS__H__