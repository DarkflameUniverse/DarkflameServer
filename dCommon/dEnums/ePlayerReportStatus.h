#ifndef __EPLAYERREPORTSTATUS__H__
#define __EPLAYERREPORTSTATUS__H__

#include <cstdint>

// Where a player report stands (player_reports.status)
enum class ePlayerReportStatus : uint8_t {
	OPEN,
	ACTIONED,  // staff acted on it (and may have given a strike)
	DISMISSED, // nothing to act on
};

#endif  //!__EPLAYERREPORTSTATUS__H__
