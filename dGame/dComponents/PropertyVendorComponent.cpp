#include "PropertyVendorComponent.h"

#include "PropertyDataMessage.h"
#include "GameMessages.h"
#include "Character.h"
#include "EntityManager.h"
#include "dZoneManager.h"
#include "Game.h"
#include "dLogger.h"
#include "PropertyManagementComponent.h"
#include "UserManager.h"

PropertyVendorComponent::PropertyVendorComponent(Entity* parent) : Component(parent) { }

void PropertyVendorComponent::OnUse(Entity* originator) {
	if (!PropertyManagementComponent::Instance()) return;

	OnQueryPropertyData(originator, originator->GetSystemAddress());

	if (PropertyManagementComponent::Instance()->GetOwnerId() != LWOOBJID_EMPTY) return;
	Game::logger->Log("PropertyVendorComponent", "Property vendor opening!");

	GameMessages::SendOpenPropertyVendor(m_ParentEntity->GetObjectID(), originator->GetSystemAddress());
}

void PropertyVendorComponent::OnQueryPropertyData(Entity* originator, const SystemAddress& sysAddr) {
	if (!PropertyManagementComponent::Instance()) return;

	PropertyManagementComponent::Instance()->OnQueryPropertyData(originator, sysAddr, m_ParentEntity->GetObjectID());
}

void PropertyVendorComponent::OnBuyFromVendor(Entity* originator, const bool confirmed, const LOT lot, const uint32_t count) {
	if (!PropertyManagementComponent::Instance()) return;

	if (!PropertyManagementComponent::Instance()->Claim(originator->GetObjectID())) {
		Game::logger->Log("PropertyVendorComponent", "Player %llu attempted to claim a property that did not belong to them.", originator->GetObjectID());
		return;
	}

	GameMessages::SendPropertyRentalResponse(m_ParentEntity->GetObjectID(), 0, 0, 0, 0, originator->GetSystemAddress());

	auto* controller = dZoneManager::Instance()->GetZoneControlObject();

	controller->OnFireEventServerSide(m_ParentEntity, "propertyRented");

	PropertyManagementComponent::Instance()->SetOwner(originator);

	PropertyManagementComponent::Instance()->OnQueryPropertyData(originator, originator->GetSystemAddress());

	Game::logger->Log("PropertyVendorComponent", "Fired event, (%d) (%i) (%i)", confirmed, lot, count);
}

