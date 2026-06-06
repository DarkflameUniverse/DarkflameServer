#ifndef __IPLAYKEYS__H__
#define __IPLAYKEYS__H__

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

class IPlayKeys {
public:
	// Get the playkey id for the given playkey.
	// Optional of bool may seem pointless, however the optional indicates if the playkey exists
	// and the bool indicates if the playkey is active.
	virtual std::optional<bool> IsPlaykeyActive(const int32_t playkeyId) = 0;

	// Get paginated list of play keys with optional search/filtering for DataTables
	// Returns a JSON-formatted string with the play key data and metadata
	virtual std::string GetPlayKeysTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true) = 0;
};

#endif  //!__IPLAYKEYS__H__
