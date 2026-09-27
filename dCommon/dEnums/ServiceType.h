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
	UNKNOWN,
	UGC // the UGC server (dUgcServer): appended, the values above are on the wire
};

#endif //!__SERVICETYPE__H__
