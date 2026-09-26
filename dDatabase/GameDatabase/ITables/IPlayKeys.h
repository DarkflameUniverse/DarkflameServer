#ifndef __IPLAYKEYS__H__
#define __IPLAYKEYS__H__

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "json.hpp"

class IPlayKeys {
public:
	// Get the playkey id for the given playkey.
	// Optional of bool may seem pointless, however the optional indicates if the playkey exists
	// and the bool indicates if the playkey is active.
	virtual std::optional<bool> IsPlaykeyActive(const int32_t playkeyId) = 0;

	// Get paginated list of play keys with optional search/filtering for DataTables
	// Returns a JSON-formatted string with the play key data and metadata
	virtual std::string GetPlayKeysTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true) = 0;

	virtual uint32_t GetPlayKeyCount() = 0;

	virtual void CreatePlayKey(const std::string_view keyString, const uint32_t uses, const std::string_view notes = "") = 0;

	virtual void SetPlayKeyActive(const int32_t playkeyId, const bool active) = 0;

	// Get a single play key including the accounts that registered with it, or {"error": ...} if missing
	virtual nlohmann::json GetPlayKey(const int32_t playkeyId) = 0;

	virtual void UpdatePlayKey(const int32_t playkeyId, const uint32_t uses, const std::string_view notes, const bool active) = 0;

	// Delete a play key, detaching any accounts that registered with it
	virtual void DeletePlayKey(const int32_t playkeyId) = 0;

	// Id of an active play key that still has uses left, for account registration
	virtual std::optional<int32_t> GetRedeemablePlayKeyId(const std::string_view keyString) = 0;

	virtual void SetAccountPlayKey(const uint32_t accountId, const int32_t playkeyId) = 0;
};

#endif  //!__IPLAYKEYS__H__
