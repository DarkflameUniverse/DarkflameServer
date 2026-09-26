#ifndef __SERVICETYPE__H__
#define __SERVICETYPE__H__

enum class ServiceType : uint16_t {
	COMMON = 0,
	AUTH,
	CHAT,
	DASHBOARD,
	WORLD,
	CLIENT,
	MASTER,
	UNKNOWN
};

#endif //!__SERVICETYPE__H__
