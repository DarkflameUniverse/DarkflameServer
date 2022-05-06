#pragma once

#include "RakNetDefines.h"
#include "GameMessages.h"
#include "dLogger.h"

class ControlBehaviors {
    public:
        void DoActions(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments, std::string command);
    private:
        void sendBehaviorListToClient(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments);
        void modelTypeChanged(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments);
        void toggleExecutionUpdates(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments);
        void addStrip(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments);
        void removeStrip(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments);
        void mergeStrips(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments);
        void splitStrip(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments);
        void updateStripUI(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments);
        void addAction(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments);
        void migrateActions(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments);
        void rearrangeStrip(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments);
        void add(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments);
        void removeActions(Entity* entity, const SystemAddress& sysAddr, AMFArrayValue* arguments);
};