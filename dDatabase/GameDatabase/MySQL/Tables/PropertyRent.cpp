#include "MySQLDatabase.h"

std::vector<IPropertyRent::RentRate> MySQLDatabase::GetPropertyRentRates() {
	std::vector<RentRate> rates;
	auto result = ExecuteSelect("SELECT * FROM property_rent_rates ORDER BY map_id;");
	while (result->next()) {
		rates.push_back({ result->getUInt("map_id"), result->getInt64("price"), result->getInt("period_days"), result->getString("updated_by").c_str(), result->getInt64("updated_at") });
	}
	return rates;
}

void MySQLDatabase::SetPropertyRentRate(const RentRate& rate) {
	ExecuteInsert(
		"INSERT INTO property_rent_rates (map_id, price, period_days, updated_by, updated_at) VALUES (?, ?, ?, ?, ?) "
		"ON DUPLICATE KEY UPDATE price = VALUES(price), period_days = VALUES(period_days), updated_by = VALUES(updated_by), updated_at = VALUES(updated_at);",
		rate.mapId, rate.price, rate.periodDays, rate.updatedBy, rate.updatedAt);
}

bool MySQLDatabase::DeletePropertyRentRate(uint32_t mapId) {
	return ExecuteUpdate("DELETE FROM property_rent_rates WHERE map_id = ?;", mapId) > 0;
}

std::vector<IPropertyRent::OwnedProperty> MySQLDatabase::GetPropertiesOfOwner(LWOOBJID ownerId) {
	std::vector<OwnedProperty> properties;
	auto result = ExecuteSelect("SELECT id, zone_id, privacy_option, rent_due, name FROM properties WHERE owner_id = ? ORDER BY id;", ownerId);
	while (result->next()) {
		properties.push_back({ result->getInt64("id"), result->getUInt("zone_id"), result->getInt("privacy_option"), result->getInt64("rent_due"), result->getString("name").c_str() });
	}
	return properties;
}

int64_t MySQLDatabase::GetPropertyRentDue(LWOOBJID propertyId) {
	auto result = ExecuteSelect("SELECT rent_due FROM properties WHERE id = ? LIMIT 1;", propertyId);
	return result->next() ? result->getInt64("rent_due") : 0;
}

void MySQLDatabase::SetPropertyRent(LWOOBJID propertyId, int64_t amount, int64_t due) {
	ExecuteUpdate("UPDATE properties SET rent_amount = ?, rent_due = ? WHERE id = ?;", amount, due, propertyId);
}

void MySQLDatabase::SetPropertyPrivacy(LWOOBJID propertyId, int32_t privacyOption) {
	ExecuteUpdate("UPDATE properties SET privacy_option = ? WHERE id = ?;", privacyOption, propertyId);
}
