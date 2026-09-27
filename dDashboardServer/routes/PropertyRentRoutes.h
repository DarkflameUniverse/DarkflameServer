#pragma once

/**
 * The Property Rent page: each property world's rent from its PropertyTemplate row and the price and period staff
 * set instead (IPropertyRent). Viewing needs properties_view, changing property_rent_manage. Rent is only charged
 * while property_rent_enabled is on (see dCommon/PropertyRentRules.h).
 */
namespace PropertyRentRoutes {
	void RegisterRoutes();
}
