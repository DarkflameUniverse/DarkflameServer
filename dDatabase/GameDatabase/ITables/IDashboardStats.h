#ifndef __IDASHBOARDSTATS__H__
#define __IDASHBOARDSTATS__H__

#include <cstdint>

class IDashboardStats {
public:
	// Cheap aggregate values the dashboard compares between ticks to detect changes made by any writer
	// (game servers, other tools). A table is considered changed when any of its values differ.
	struct Snapshot {
		uint64_t accounts{};
		uint64_t accountsMaxId{};
		uint64_t characters{};
		uint64_t pendingNames{};
		uint64_t properties{};
		uint64_t pendingProperties{};
		uint64_t playKeys{};
		uint64_t bugReports{};
		uint64_t unresolvedBugReports{};
		uint64_t petNames{};
		uint64_t pendingPetNames{};
		uint64_t activityLogMaxId{};
		uint64_t chatLogMaxId{};
		uint64_t commandLogMaxId{};
		uint64_t auditLogMaxId{};
		uint64_t mailMaxId{};
		uint64_t openEconomyFlags{};
		uint64_t economyFlagsMaxId{};

		bool operator==(const Snapshot&) const = default;
	};

	virtual Snapshot GetDashboardSnapshot() = 0;
};

#endif  //!__IDASHBOARDSTATS__H__
