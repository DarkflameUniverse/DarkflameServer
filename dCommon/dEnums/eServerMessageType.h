#ifndef __ESERVERMESSAGETYPE__H__
#define __ESERVERMESSAGETYPE__H__

#include <cstdint>
//! The Internal Server Packet Identifiers
enum class eServerMessageType : uint32_t {
	VERSION_CONFIRM = 0,
	DISCONNECT_NOTIFY,
	GENERAL_NOTIFY
};

#endif  //!__ESERVERMESSAGETYPE__H__
