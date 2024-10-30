#ifndef IPLAYKEYS_H
#define IPLAYKEYS_H

#include <cstdint>
#include <optional>

class IPlayKeys {
public:
	// Get the playkey id for the given playkey.
	// Optional of bool may seem pointless, however the optional indicates if the playkey exists
	// and the bool indicates if the playkey is active.
	virtual std::optional<bool> IsPlaykeyActive(const int32_t playkeyId) = 0;
};

#endif  //!IPLAYKEYS_H
