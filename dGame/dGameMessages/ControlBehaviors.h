#pragma once

#include "RakNetDefines.h"
#include "GameMessages.h"
#include "dLogger.h"

class ControlBehaviors {
    public:
        void DoActions(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, std::string command, Entity* modelOwner);
    private:
        void sendBehaviorListToClient(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner);
        void modelTypeChanged(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner);
        void toggleExecutionUpdates(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner);
        void addStrip(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner);
        void removeStrip(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner);
        void mergeStrips(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner);
        void splitStrip(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner);
        void updateStripUI(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner);
        void addAction(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner);
        void migrateActions(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner);
        void rearrangeStrip(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner);
        void add(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner);
        void removeActions(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner);
        void rename(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner);
        void sendBehaviorBlocksToClient(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner);
        void updateAction(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner);
        void moveToInventory(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, Entity* modelOwner);
};