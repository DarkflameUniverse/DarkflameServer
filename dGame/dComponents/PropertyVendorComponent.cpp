#include "PropertyVendorComponent.h"

#include "PropertyDataMessage.h"
#include "GameMessages.h"
#include "Character.h"
#include "EntityManager.h"
#include "dZoneManager.h"
#include "Game.h"
#include "Logger.h"
#include "PropertyManagementComponent.h"
#include "UserManager.h"

PropertyVendorComponent::PropertyVendorComponent(Entity* parent) : Component(parent) {
}

void PropertyVendorComponent::OnUse(Entity* originator) {
	if (PropertyManagementComponent::Instance() == nullptr) return;

	OnQueryPropertyData(originator, originator->GetSystemAddress());

	if (PropertyManagementComponent::Instance()->GetOwnerId() == LWOOBJID_EMPTY) {
		Log("Property vendor opening!");

		GameMessages::SendOpenPropertyVendor(m_Parent->GetObjectID(), originator->GetSystemAddress());

		return;
	}
}

void PropertyVendorComponent::OnQueryPropertyData(Entity* originator, const SystemAddress& sysAddr) {
	if (PropertyManagementComponent::Instance() == nullptr) return;

	PropertyManagementComponent::Instance()->OnQueryPropertyData(originator, sysAddr, m_Parent->GetObjectID());
}

void PropertyVendorComponent::OnBuyFromVendor(Entity* originator, const bool confirmed, const LOT lot, const uint32_t count) {
	if (PropertyManagementComponent::Instance() == nullptr) return;

	if (PropertyManagementComponent::Instance()->Claim(originator->GetObjectID()) == false) {
		Log("FAILED TO CLAIM PROPERTY.  PLAYER ID IS %llu", originator->GetObjectID());
		return;
	}

	GameMessages::SendPropertyRentalResponse(m_Parent->GetObjectID(), 0, 0, 0, 0, originator->GetSystemAddress());

	auto* controller = dZoneManager::Instance()->GetZoneControlObject();

	controller->OnFireEventServerSide(m_Parent, "propertyRented");

	PropertyManagementComponent::Instance()->SetOwner(originator);

	PropertyManagementComponent::Instance()->OnQueryPropertyData(originator, originator->GetSystemAddress());

	Log("Fired event; (%d) (%i) (%i)", confirmed, lot, count);
}

