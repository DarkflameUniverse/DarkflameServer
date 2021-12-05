#include "SbLupTeleport.h"
#include "dZoneManager.h"
#include "EntityManager.h"
#include "GeneralUtils.h"
#include "GameMessages.h"

void SbLupTeleport::OnStartup(Entity* self) 
{
    self->SetVar(u"currentZone", (int32_t) dZoneManager::Instance()->GetZoneID().GetMapID());
    self->SetVar(u"choiceZone", m_ChoiceZoneID);
    self->SetVar(u"teleportAnim", m_TeleportAnim);
    self->SetVar(u"teleportString", m_TeleportString);
    self->SetVar(u"spawnPoint", m_SpawnPoint);

    args = {};

    AMFStringValue* callbackClient = new AMFStringValue();
    callbackClient->SetStringValue(std::to_string(self->GetObjectID()));
    args.InsertValue("callbackClient", callbackClient);

    AMFStringValue* strIdentifier = new AMFStringValue();
    strIdentifier->SetStringValue("choiceDoor");
    args.InsertValue("strIdentifier", strIdentifier);

    AMFStringValue* title = new AMFStringValue();
    title->SetStringValue("%[LUP_Starbase3001_Launchpad]");
    args.InsertValue("title", title);

    AMFArrayValue* choiceOptions = new AMFArrayValue();

    {
        AMFArrayValue* nsArgs = new AMFArrayValue();

        AMFStringValue* image = new AMFStringValue();
        image->SetStringValue("textures/ui/zone_thumnails/Deep_Freeze.dds");
        nsArgs->InsertValue("image", image);

        AMFStringValue* caption = new AMFStringValue();
        caption->SetStringValue("%[ZoneTable_1601_DisplayDescription]");
        nsArgs->InsertValue("caption", caption);

        AMFStringValue* identifier = new AMFStringValue();
        identifier->SetStringValue("zoneID_1601");
        nsArgs->InsertValue("identifier", identifier);

        AMFStringValue* tooltipText = new AMFStringValue();
        tooltipText->SetStringValue("%[ZoneTable_1601_summary]");
        nsArgs->InsertValue("tooltipText", tooltipText);

        choiceOptions->PushBackValue(nsArgs);
    }

    {
        AMFArrayValue* ntArgs = new AMFArrayValue();

        AMFStringValue* image = new AMFStringValue();
        image->SetStringValue("textures/ui/zone_thumnails/Robot_City.dds");
        ntArgs->InsertValue("image", image);

        AMFStringValue* caption = new AMFStringValue();
        caption->SetStringValue("%[ZoneTable_1602_DisplayDescription]");
        ntArgs->InsertValue("caption", caption);

        AMFStringValue* identifier = new AMFStringValue();
        identifier->SetStringValue("zoneID_1602");
        ntArgs->InsertValue("identifier", identifier);

        AMFStringValue* tooltipText = new AMFStringValue();
        tooltipText->SetStringValue("%[ZoneTable_1602_summary]");
        ntArgs->InsertValue("tooltipText", tooltipText);

        choiceOptions->PushBackValue(ntArgs);
    }

    {
        AMFArrayValue* ntArgs = new AMFArrayValue();

        AMFStringValue* image = new AMFStringValue();
        image->SetStringValue("textures/ui/zone_thumnails/Moon_Base.dds");
        ntArgs->InsertValue("image", image);

        AMFStringValue* caption = new AMFStringValue();
        caption->SetStringValue("%[ZoneTable_1603_DisplayDescription]");
        ntArgs->InsertValue("caption", caption);

        AMFStringValue* identifier = new AMFStringValue();
        identifier->SetStringValue("zoneID_1603");
        ntArgs->InsertValue("identifier", identifier);

        AMFStringValue* tooltipText = new AMFStringValue();
        tooltipText->SetStringValue("%[ZoneTable_1603_summary]");
        ntArgs->InsertValue("tooltipText", tooltipText);

        choiceOptions->PushBackValue(ntArgs);
    }

    {
        AMFArrayValue* ntArgs = new AMFArrayValue();

        AMFStringValue* image = new AMFStringValue();
        image->SetStringValue("textures/ui/zone_thumnails/Porto_Bello.dds");
        ntArgs->InsertValue("image", image);

        AMFStringValue* caption = new AMFStringValue();
        caption->SetStringValue("%[ZoneTable_1604_DisplayDescription]");
        ntArgs->InsertValue("caption", caption);

        AMFStringValue* identifier = new AMFStringValue();
        identifier->SetStringValue("zoneID_1604");
        ntArgs->InsertValue("identifier", identifier);

        AMFStringValue* tooltipText = new AMFStringValue();
        tooltipText->SetStringValue("%[ZoneTable_1604_summary]");
        ntArgs->InsertValue("tooltipText", tooltipText);

        choiceOptions->PushBackValue(ntArgs);
    }

    args.InsertValue("options", choiceOptions);
}

void SbLupTeleport::OnUse(Entity* self, Entity* user) 
{
    auto* player = user;

    //if (CheckChoice(self, player))
    {
        GameMessages::SendUIMessageServerToSingleClient(player, player->GetSystemAddress(), "QueueChoiceBox", &args);
    }
    /*else
    {
        BaseOnUse(self, player);
    }*/
}

void SbLupTeleport::OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) 
{
    BaseOnMessageBoxResponse(self, sender, button, identifier, userData);
}

void SbLupTeleport::OnChoiceBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& buttonIdentifier, const std::u16string& identifier) 
{
    BaseChoiceBoxRespond(self, sender, button, buttonIdentifier, identifier);
}

void SbLupTeleport::OnTimerDone(Entity* self, std::string timerName) 
{
    BaseOnTimerDone(self, timerName);
}

void SbLupTeleport::OnFireEventServerSide(Entity *self, Entity *sender, std::string args, int32_t param1, int32_t param2, int32_t param3) 
{
    BaseOnFireEventServerSide(self, sender, args, param1, param2, param3);
}
