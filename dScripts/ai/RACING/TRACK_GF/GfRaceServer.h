#ifndef GFRACESERVER_H
#define GFRACESERVER_H

#include "RaceImaginationServer.h"

class GfRaceServer : public RaceImaginationServer {
public:
	void OnStartup(Entity* self) override;
};

#endif  //!GFRACESERVER_H
