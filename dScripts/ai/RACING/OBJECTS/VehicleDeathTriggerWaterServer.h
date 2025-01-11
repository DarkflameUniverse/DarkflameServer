#ifndef VEHICLEDEATHTRIGGERWATERSERVER_H
#define VEHICLEDEATHTRIGGERWATERSERVER_H

#include "CppScripts.h"

class VehicleDeathTriggerWaterServer : public CppScripts::Script {
public:
	void OnCollisionPhantom(Entity* self, Entity* target) override;
};

#endif  //!VEHICLEDEATHTRIGGERWATERSERVER_H
