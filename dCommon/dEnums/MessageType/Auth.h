#pragma once
#include <cstdint>

namespace MessageType {
	enum class Auth : uint32_t {
		LOGIN_REQUEST = 0,
		LOGOUT_REQUEST,
		CREATE_NEW_ACCOUNT_REQUEST,
		LEGOINTERFACE_AUTH_RESPONSE,
		SESSIONKEY_RECEIVED_CONFIRM,
		RUNTIME_CONFIG
	};
}
