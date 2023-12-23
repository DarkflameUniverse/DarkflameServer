#ifndef __NTNAOMIDIRTSERVER__H__
#define __NTNAOMIDIRTSERVER__H__

#include "VisToggleNotifierServer.h"

class NTNaomiDirtServer : public VisToggleNotifierServer {
public:
	void OnStartup(Entity* self) override;
};

#endif  //!__NTNAOMIDIRTSERVER__H__
