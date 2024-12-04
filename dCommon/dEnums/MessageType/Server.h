#pragma once
#include <cstdint>

namespace MessageType {
	//! The Internal Server Packet Identifiers
	enum class Server : uint32_t {
		VERSION_CONFIRM = 0,
		DISCONNECT_NOTIFY,
		GENERAL_NOTIFY
	};
}
