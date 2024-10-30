#ifndef ESERVERMESSAGETYPE_H
#define ESERVERMESSAGETYPE_H

#include <cstdint>
//! The Internal Server Packet Identifiers
enum class eServerMessageType : uint32_t {
	VERSION_CONFIRM = 0,
	DISCONNECT_NOTIFY,
	GENERAL_NOTIFY
};

#endif  //!ESERVERMESSAGETYPE_H
