#ifndef FVRACESERVER_H
#define FVRACESERVER_H

#include "RaceImaginationServer.h"

class FvRaceServer : public RaceImaginationServer {
public:
	void OnStartup(Entity* self) override;
};

#endif  //!FVRACESERVER_H
