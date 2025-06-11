// Darkflame Universe
// Copyright 2025

#ifndef NSWINTERRACESERVER_H
#define NSWINTERRACESERVER_H

#include "CppScripts.h"
#include "RaceImaginationServer.h"

class NsWinterRaceServer : public RaceImaginationServer {
public:
	void OnStartup(Entity* self) override;
};

#endif //!NSWINTERRACESERVER_H
