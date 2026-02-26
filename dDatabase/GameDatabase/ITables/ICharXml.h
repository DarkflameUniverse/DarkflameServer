#ifndef __ICHARXML__H__
#define __ICHARXML__H__

#include <cstdint>
#include <string>
#include <string_view>

class ICharXml {
public:
	// Get the character xml for the given character id.
	virtual std::string GetCharacterXml(const LWOOBJID charId) = 0;

	// Update the character xml for the given character id.
	virtual void UpdateCharacterXml(const LWOOBJID charId, const std::string_view lxfml) = 0;

	// Insert the character xml for the given character id.
	virtual void InsertCharacterXml(const LWOOBJID characterId, const std::string_view lxfml) = 0;

	// Get paginated list of characters with optional search/filtering for DataTables
	// Returns a JSON-formatted string with the character data and metadata
	virtual std::string GetCharactersTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true) = 0;
};

#endif  //!__ICHARXML__H__
