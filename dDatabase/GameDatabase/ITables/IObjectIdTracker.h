#ifndef __IOBJECTIDTRACKER__H__
#define __IOBJECTIDTRACKER__H__

#include <cstdint>
#include <optional>

class IObjectIdTracker {
public:
	// Only the first 48 bits of the ids are the id, the last 16 bits are reserved for flags.
	struct Range {
		uint64_t minID{}; // Only the first 48 bits are the id, the last 16 bits are reserved for flags.
		uint64_t maxID{}; // Only the first 48 bits are the id, the last 16 bits are reserved for flags.
	};

	// Get the current persistent id.
	virtual std::optional<uint64_t> GetCurrentPersistentId() = 0;

	// Insert the default persistent id.
	virtual void InsertDefaultPersistentId() = 0;

	virtual Range GetPersistentIdRange() = 0;
};

#endif  //!__IOBJECTIDTRACKER__H__
