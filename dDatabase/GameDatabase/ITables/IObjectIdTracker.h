#ifndef __IOBJECTIDTRACKER__H__
#define __IOBJECTIDTRACKER__H__

#include <cstdint>
#include <optional>

class IObjectIdTracker {
public:
	// Get the current persistent id.
	virtual std::optional<uint32_t> GetCurrentPersistentId() = 0;

	// Insert the default persistent id.
	virtual void InsertDefaultPersistentId() = 0;

	// Update the persistent id.
	virtual void UpdatePersistentId(const uint32_t newId) = 0;
};

#endif  //!__IOBJECTIDTRACKER__H__
