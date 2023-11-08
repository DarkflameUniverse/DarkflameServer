#ifndef __ICHARXML__H__
#define __ICHARXML__H__

#include <cstdint>
#include <string>
#include <string_view>

class ICharXml {
public:
	// Get the character xml for the given character id.
	virtual std::string GetCharacterXml(const uint32_t charId) = 0;

	// Update the character xml for the given character id.
	virtual void UpdateCharacterXml(const uint32_t charId, const std::string_view lxfml) = 0;

	// Insert the character xml for the given character id.
	virtual void InsertCharacterXml(const uint32_t accountId, const std::string_view lxfml) = 0;
};

#endif  //!__ICHARXML__H__
