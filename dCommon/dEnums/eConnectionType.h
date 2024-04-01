#ifndef __ECONNECTIONTYPE__H__
#define __ECONNECTIONTYPE__H__

enum class eConnectionType : uint16_t {
	SERVER = 0,
	AUTH,
	CHAT,
	WORLD = 4,
	CLIENT,
	MASTER
};

#endif  //!__ECONNECTIONTYPE__H__
