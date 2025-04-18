#ifndef __EHTTPMETHODS__H__
#define __EHTTPMETHODS__H__

#ifdef DARKFLAME_PLATFORM_WIN32
#pragma push_macro("DELETE")
#undef DELETE
#endif

enum class eHTTPMethod {
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

#ifdef DARKFLAME_PLATFORM_WIN32
#pragma pop_macro("DELETE")
#endif

#endif // __EHTTPMETHODS__H__
