#ifndef __EACCOUNTLINK__H__
#define __EACCOUNTLINK__H__

#include <cstdint>

// What two accounts have in common, from data the servers store (see IModeration::GetLinkedAccounts)
enum class eAccountLink : uint8_t {
	PLAY_KEY,      // signed up with the same play key
	EMAIL,         // the same email address
	LOGIN_ADDRESS, // logged in to the game from the same network address (account_login_addresses)
};

#endif  //!__EACCOUNTLINK__H__
