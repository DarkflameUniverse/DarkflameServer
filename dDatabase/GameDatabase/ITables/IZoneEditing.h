#ifndef __IZONEEDITING__H__
#define __IZONEEDITING__H__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/**
 * Who may edit a zone's files (ZoneEditLock.h, docs/ZonePublish.md): each zone's assigned editors, and the one
 * account holding its lock. The rules are ZoneEditLock's; these are only the rows.
 */
class IZoneEditing {
public:
	struct ZoneLock {
		uint32_t zoneId{};
		uint32_t accountId{};
		int64_t lockedAt{};
	};

	struct ZoneEditor {
		uint32_t zoneId{};
		uint32_t accountId{};
		std::string assignedBy;
		int64_t assignedAt{};
	};

	virtual std::optional<ZoneLock> GetZoneLock(uint32_t zoneId) = 0;
	// Every held lock, by zone
	virtual std::vector<ZoneLock> GetZoneLocks() = 0;
	// Takes the zone's lock when nobody holds it; false when someone does (one statement, so two takers can't both win)
	virtual bool TakeZoneLock(uint32_t zoneId, uint32_t accountId, int64_t time) = 0;
	// Whether there was a lock to release
	virtual bool ReleaseZoneLock(uint32_t zoneId) = 0;

	// A zone's editors, by account; zoneId 0: every zone's, by zone then account
	virtual std::vector<ZoneEditor> GetZoneEditors(uint32_t zoneId) = 0;
	virtual bool IsZoneEditor(uint32_t zoneId, uint32_t accountId) = 0;
	// Adds the account to the zone's editors unless it is one
	virtual void AddZoneEditor(const ZoneEditor& editor) = 0;
	// Whether the account was one of the zone's editors
	virtual bool RemoveZoneEditor(uint32_t zoneId, uint32_t accountId) = 0;
};

#endif  //!__IZONEEDITING__H__
