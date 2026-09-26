#ifndef __ICHARACTERSNAPSHOTS__H__
#define __ICHARACTERSNAPSHOTS__H__

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "dCommonVars.h"

/**
 * Earlier versions of characters' XML, taken before the dashboard changes a character and once a day for characters
 * that changed, so staff can see what changed and put a character back. The XML is stored zlib-compressed.
 */
class ICharacterSnapshots {
public:
	struct CharacterSnapshot {
		uint64_t id{};
		LWOOBJID characterId{};
		int64_t takenAt{};
		std::string reason;
		std::string actor;
		uint32_t size{};       // uncompressed bytes
		std::string hash;      // sha256 of the XML, to skip unchanged characters
		std::string compressed;
	};

	virtual void InsertCharacterSnapshot(const CharacterSnapshot& snapshot) = 0;

	// Newest first, without the XML
	virtual std::vector<CharacterSnapshot> GetCharacterSnapshots(LWOOBJID characterId) = 0;

	virtual std::optional<CharacterSnapshot> GetCharacterSnapshot(uint64_t id) = 0;

	// The newest snapshot's hash for every character that has one
	virtual std::map<LWOOBJID, std::string> GetLatestSnapshotHashes() = 0;

	// Delete snapshots older than beforeTime, always keeping each character's newest `keep`
	virtual uint32_t PruneCharacterSnapshots(int64_t beforeTime, uint32_t keep) = 0;
};

#endif  //!__ICHARACTERSNAPSHOTS__H__
