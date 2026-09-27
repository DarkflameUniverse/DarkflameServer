#ifndef __ICHARXML__H__
#define __ICHARXML__H__

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

class ICharXml {
public:
	// Get the character xml for the given character id.
	virtual std::string GetCharacterXml(const LWOOBJID charId) = 0;

	// Overwrite the character xml for the given character id (dashboard edits, restores, maintenance). Bumps the save
	// generation, so a world still holding the older version can't save over this one.
	virtual void UpdateCharacterXml(const LWOOBJID charId, const std::string_view lxfml) = 0;

	/**
	 * Stale save guard. Every write to a character's xml bumps its save generation: a world loading the character to
	 * play it, a world saving it and the dashboard writing it. A world keeps the generation it loaded (or last saved)
	 * and only saves over that one, so a world that lost the character to another world or to the dashboard can't
	 * overwrite the newer data.
	 */
	struct CharacterXml {
		std::string xml;
		uint64_t generation{};
	};

	// Take over the character for playing: bump its save generation and return the xml with the new generation.
	// nullopt when the character has no xml.
	virtual std::optional<CharacterXml> ClaimCharacterXml(const LWOOBJID charId) = 0;

	// Save if the stored generation is still `generation`: the xml is written, the generation becomes generation + 1
	// and true is returned. Otherwise nothing is written and false is returned (someone newer saved it).
	virtual bool SaveCharacterXml(const LWOOBJID charId, const std::string_view lxfml, const uint64_t generation) = 0;

	// The stored save generation (0 when the character has no xml)
	virtual uint64_t GetCharacterSaveGeneration(const LWOOBJID charId) = 0;

	// Insert the character xml for the given character id.
	virtual void InsertCharacterXml(const LWOOBJID characterId, const std::string_view lxfml) = 0;

	// Get paginated list of characters with optional search/filtering for DataTables
	// Returns a JSON-formatted string with the character data and metadata
	virtual std::string GetCharactersTable(uint32_t start, uint32_t length, const std::string_view search = "", uint32_t orderColumn = 0, bool orderAsc = true) = 0;
};

#endif  //!__ICHARXML__H__
