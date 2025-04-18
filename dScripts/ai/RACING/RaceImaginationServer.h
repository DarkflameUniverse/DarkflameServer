#ifndef RACEIMAGINATIONSERVER_H
#define RACEIMAGINATIONSERVER_H

#include "CppScripts.h"

class RaceImaginationServer : public virtual CppScripts::Script {
public:
	void OnZoneLoadedInfo(Entity* self, const GameMessages::ZoneLoadedInfo& info) override;
};

#endif  //!RACEIMAGINATIONSERVER_H
