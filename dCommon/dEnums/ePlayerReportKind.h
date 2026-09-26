#ifndef __EPLAYERREPORTKIND__H__
#define __EPLAYERREPORTKIND__H__

#include <cstdint>

// What a player reported from the game's Report Abuse window (player_reports.kind holds the name)
enum class ePlayerReportKind : uint8_t {
	PLAYER,   // another player: ReportBug with nOtherPlayerID set
	MODEL,    // an object in the world, usually a model on a property: ReportOffensiveModel
	PROPERTY, // a property, picked by its plaque: ReportOffensiveProperty
};

#endif  //!__EPLAYERREPORTKIND__H__
