#ifndef __NTPIPEVISIBILITYSERVER__H__
#define __NTPIPEVISIBILITYSERVER__H__

#include "CppScripts.h"

class NTPipeVisibilityServer : public CppScripts::Script {
public:
	void OnQuickBuildComplete(Entity* self, Entity* target) override;
};

#endif  //!__NTPIPEVISIBILITYSERVER__H__
