#include "PropertyVendorComponent.h"
#include "ObjectMessages.h"

#include "PropertyMessages.h"
#include "GameMessages.h"
#include "Character.h"
#include "EntityManager.h"
#include "dZoneManager.h"
#include "Game.h"
#include "Logger.h"
#include "PropertyManagementComponent.h"
#include "UserManager.h"
#include "Mission.h"
#include "MissionComponent.h"

PropertyVendorComponent::PropertyVendorComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {
	RegisterMsg(&PropertyVendorComponent::OnRequestUse);
}

uint32_t PropertyVendorComponent::GetRequiredMission() const {
	return m_Parent->GetVarAs<uint32_t>(u"property_required_mission");
}

bool PropertyVendorComponent::MayRent(Entity& player) const {
	const auto required = GetRequiredMission();
	if (required == 0) return true;

	const auto* const missions = player.GetComponent<MissionComponent>();
	const auto* const mission = missions ? missions->GetMission(required) : nullptr;
	// The client asks its list of finished missions: one that was completed and is repeated counts as well
	return mission != nullptr && (mission->IsComplete() || mission->GetCompletions() > 0);
}

bool PropertyVendorComponent::OnRequestUse(GameMessages::RequestUseEvent& event) {
	if (event.msg.IsMissionChoice()) return false;
	auto* const originator = event.user;
	// LWOPropertyVendorComponent::onCheckUseRequirements (0x00c2e210): no use without the level's mission
	if (!MayRent(*originator)) return true;
	if (PropertyManagementComponent::Instance() == nullptr) return true;

	OnQueryPropertyData(originator, originator->GetSystemAddress());

	if (PropertyManagementComponent::Instance()->GetOwnerId() == LWOOBJID_EMPTY) {
		LOG("Property vendor opening!");

		GameMessages::OpenPropertyVendor msg;
		msg.target = m_Parent->GetObjectID();
		msg.Send(originator->GetSystemAddress());

		return true;
	}
	return true;
}

void PropertyVendorComponent::OnQueryPropertyData(Entity* originator, const SystemAddress& sysAddr) {
	if (PropertyManagementComponent::Instance() == nullptr) return;

	PropertyManagementComponent::Instance()->OnQueryPropertyData(originator, sysAddr, m_Parent->GetObjectID());
}

void PropertyVendorComponent::OnBuyFromVendor(Entity* originator, const bool confirmed, const LOT lot, const uint32_t count) {
	if (!MayRent(*originator)) {
		LOG("Player %llu rents a property without mission %u", originator->GetObjectID(), GetRequiredMission());
		GameMessages::PropertyRentalResponse refused;
		refused.target = m_Parent->GetObjectID();
		refused.code = RENTAL_MISSING_ACHIEVEMENT;
		refused.Send(originator->GetSystemAddress());
		return;
	}

	if (PropertyManagementComponent::Instance() == nullptr) return;

	if (PropertyManagementComponent::Instance()->Claim(originator->GetObjectID()) == false) {
		LOG("FAILED TO CLAIM PROPERTY.  PLAYER ID IS %llu", originator->GetObjectID());
		return;
	}

	GameMessages::PropertyRentalResponse msg;
	msg.target = m_Parent->GetObjectID();
	msg.cloneid = 0;
	msg.code = 0;
	msg.propertyID = 0;
	msg.rentdue = 0;
	msg.Send(originator->GetSystemAddress());

	auto* controller = Game::zoneManager->GetZoneControlObject();

	controller->OnFireEventServerSide(m_Parent, "propertyRented");

	PropertyManagementComponent::Instance()->SetOwner(originator);

	PropertyManagementComponent::Instance()->OnQueryPropertyData(originator, originator->GetSystemAddress());

	LOG("Fired event; (%d) (%i) (%i)", confirmed, lot, count);
}

