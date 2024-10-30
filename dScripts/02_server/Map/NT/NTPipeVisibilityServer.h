#ifndef NTPIPEVISIBILITYSERVER_H
#define NTPIPEVISIBILITYSERVER_H

#include "CppScripts.h"

class NTPipeVisibilityServer : public CppScripts::Script {
public:
	void OnQuickBuildComplete(Entity* self, Entity* target) override;
};

#endif  //!NTPIPEVISIBILITYSERVER_H
