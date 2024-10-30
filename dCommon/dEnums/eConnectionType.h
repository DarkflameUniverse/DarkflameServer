#ifndef ECONNECTIONTYPE_H
#define ECONNECTIONTYPE_H

enum class eConnectionType : uint16_t {
	SERVER = 0,
	AUTH,
	CHAT,
	WORLD = 4,
	CLIENT,
	MASTER
};

#endif  //!ECONNECTIONTYPE_H
