#include "SQLiteDatabase.h"

std::vector<IPropertyRent::RentRate> SQLiteDatabase::GetPropertyRentRates() {
	std::vector<RentRate> rates;
	auto [_, result] = ExecuteSelect("SELECT * FROM property_rent_rates ORDER BY map_id;");
	for (; !result.eof(); result.nextRow()) {
		rates.push_back({ static_cast<uint32_t>(result.getIntField("map_id")), result.getInt64Field("price"), result.getIntField("period_days"),
			result.getStringField("updated_by"), result.getInt64Field("updated_at") });
	}
	return rates;
}

void SQLiteDatabase::SetPropertyRentRate(const RentRate& rate) {
	ExecuteInsert(
		"INSERT INTO property_rent_rates (map_id, price, period_days, updated_by, updated_at) VALUES (?, ?, ?, ?, ?) "
		"ON CONFLICT(map_id) DO UPDATE SET price = excluded.price, period_days = excluded.period_days, updated_by = excluded.updated_by, updated_at = excluded.updated_at;",
		rate.mapId, rate.price, rate.periodDays, rate.updatedBy, rate.updatedAt);
}

bool SQLiteDatabase::DeletePropertyRentRate(uint32_t mapId) {
	return ExecuteUpdate("DELETE FROM property_rent_rates WHERE map_id = ?;", mapId) > 0;
}

std::vector<IPropertyRent::OwnedProperty> SQLiteDatabase::GetPropertiesOfOwner(LWOOBJID ownerId) {
	std::vector<OwnedProperty> properties;
	auto [_, result] = ExecuteSelect("SELECT id, zone_id, privacy_option, rent_due, name FROM properties WHERE owner_id = ? ORDER BY id;", ownerId);
	for (; !result.eof(); result.nextRow()) {
		properties.push_back({ result.getInt64Field("id"), static_cast<uint32_t>(result.getIntField("zone_id")), result.getIntField("privacy_option"),
			result.getInt64Field("rent_due"), result.getStringField("name") });
	}
	return properties;
}

int64_t SQLiteDatabase::GetPropertyRentDue(LWOOBJID propertyId) {
	auto [_, result] = ExecuteSelect("SELECT rent_due FROM properties WHERE id = ? LIMIT 1;", propertyId);
	return result.eof() ? 0 : result.getInt64Field("rent_due");
}

void SQLiteDatabase::SetPropertyRent(LWOOBJID propertyId, int64_t amount, int64_t due) {
	ExecuteUpdate("UPDATE properties SET rent_amount = ?, rent_due = ? WHERE id = ?;", amount, due, propertyId);
}

void SQLiteDatabase::SetPropertyPrivacy(LWOOBJID propertyId, int32_t privacyOption) {
	ExecuteUpdate("UPDATE properties SET privacy_option = ? WHERE id = ?;", privacyOption, propertyId);
}
