#ifndef __ISERVEROPERATIONS__H__
#define __ISERVEROPERATIONS__H__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/**
 * Things staff schedule to happen to the running server from the dashboard: repeating announcements, scheduled events
 * (feature flags, vanity changes, live events, announcements and restarts switched on and off together) and per-zone
 * instance limits the master applies.
 */
class IServerOperations {
public:
	struct ScheduledAnnouncement {
		uint64_t id{};
		std::string title;
		std::string message;
		std::vector<uint32_t> zones; // empty: every world
		std::string schedule;        // a Cron.h schedule, in UTC
		int64_t startsAt{};          // 0: straight away
		int64_t endsAt{};            // 0: never
		bool enabled{ true };
		int64_t lastSentAt{};
		uint32_t sentCount{};
		int64_t createdAt{};
		std::string createdBy;
		int64_t updatedAt{};
		std::string updatedBy;
	};

	virtual std::vector<ScheduledAnnouncement> GetScheduledAnnouncements() = 0;
	virtual uint64_t InsertScheduledAnnouncement(const ScheduledAnnouncement& announcement) = 0;
	// Everything but the send counters
	virtual void UpdateScheduledAnnouncement(const ScheduledAnnouncement& announcement) = 0;
	// Records one send
	virtual void MarkAnnouncementSent(uint64_t id, int64_t time) = 0;
	virtual void DeleteScheduledAnnouncement(uint64_t id) = 0;

	// Stored as numbers, so only append
	enum class eEventState : uint8_t { SCHEDULED = 0, ACTIVE = 1, ENDED = 2, CANCELLED = 3, MISSED = 4 };

	/**
	 * A scheduled event (the Events page): parts that switch on and off together (a feature flag in event_1..event_8,
	 * vanity changes, a live event, announcements, a restart; EventParts.h), on once between two times or by recurring
	 * rules (ScheduleRules.h). The world servers read the vanity parts of the events that are on.
	 */
	struct ScheduledEvent {
		uint64_t id{};
		std::string name;
		std::string note;
		uint8_t mode{ 1 };     // ScheduleRules::eMode: off, on by its schedule, always on
		std::string schedule;  // recurring rules as JSON; empty: once, from startsAt to endsAt
		int64_t startsAt{};
		int64_t endsAt{};
		int32_t priority{};    // higher is laid on later and wins where events change the same vanity NPCs or files
		std::string parts;     // JSON: [{kind, config, applied, state, status}]
		eEventState state{ eEventState::SCHEDULED }; // ACTIVE while its parts are on; a once event ends ENDED, MISSED or CANCELLED
		std::string status;    // the last thing that happened to it
		int64_t createdAt{};
		std::string createdBy;
		int64_t updatedAt{};
		std::string updatedBy;
	};

	virtual std::vector<ScheduledEvent> GetScheduledEvents() = 0;
	virtual uint64_t InsertScheduledEvent(const ScheduledEvent& event) = 0;
	virtual void UpdateScheduledEvent(const ScheduledEvent& event) = 0;
	virtual void DeleteScheduledEvent(uint64_t id) = 0;

	struct ZoneLimit {
		uint32_t zoneId{};
		std::optional<uint32_t> softCap; // nullopt: the client's ZoneTable population_soft_cap
		std::optional<uint32_t> hardCap; // nullopt: ZoneTable population_hard_cap
		uint32_t spareInstances{};       // keep this many instances with room running
		int64_t updatedAt{};
		std::string updatedBy;
	};

	virtual std::vector<ZoneLimit> GetZoneLimits() = 0;
	virtual void SetZoneLimit(const ZoneLimit& limit) = 0;
	virtual void DeleteZoneLimit(uint32_t zoneId) = 0;
};

#endif  //!__ISERVEROPERATIONS__H__
