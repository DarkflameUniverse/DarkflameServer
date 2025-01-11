#ifndef NSRACESERVER_H
#define NSRACESERVER_H

#include "CppScripts.h"
#include "RaceImaginationServer.h"

class NsRaceServer : public RaceImaginationServer {
public:
	void OnStartup(Entity* self) override;
};

#endif  //!NSRACESERVER_H
