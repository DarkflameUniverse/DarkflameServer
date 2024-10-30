#ifndef NTNAOMIDIRTSERVER_H
#define NTNAOMIDIRTSERVER_H

#include "VisToggleNotifierServer.h"

class NTNaomiDirtServer : public VisToggleNotifierServer {
public:
	void OnStartup(Entity* self) override;
};

#endif  //!NTNAOMIDIRTSERVER_H
