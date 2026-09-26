#include "MySQLDatabase.h"

std::vector<IFeaturedProperties::FeaturedSlot> MySQLDatabase::GetFeaturedPropertySlots() {
	std::vector<FeaturedSlot> slots;
	auto result = ExecuteSelect("SELECT * FROM featured_properties ORDER BY template_id;");
	while (result->next()) {
		slots.push_back({ result->getUInt("template_id"), static_cast<uint8_t>(result->getUInt("mode")), result->getInt64("property_id"),
			result->getInt64("updated_at"), result->getString("updated_by").c_str(), result->getUInt("zone_id") });
	}
	return slots;
}

void MySQLDatabase::SetFeaturedPropertySlot(const FeaturedSlot& slot) {
	ExecuteInsert(
		"INSERT INTO featured_properties (template_id, mode, property_id, updated_at, updated_by, zone_id) VALUES (?, ?, ?, ?, ?, ?) "
		"ON DUPLICATE KEY UPDATE mode = VALUES(mode), property_id = VALUES(property_id), updated_at = VALUES(updated_at), updated_by = VALUES(updated_by), zone_id = VALUES(zone_id);",
		slot.templateId, static_cast<uint32_t>(slot.mode), slot.propertyId, slot.updatedAt, slot.updatedBy, slot.zoneId);
}

IFeaturedProperties::FeaturedSettings MySQLDatabase::GetFeaturedPropertiesSettings() {
	FeaturedSettings settings;
	auto result = ExecuteSelect("SELECT * FROM featured_properties_settings WHERE id = 1;");
	if (result->next()) {
		settings.fullAuto = result->getInt("full_auto") != 0;
		settings.updatedAt = result->getInt64("updated_at");
		settings.updatedBy = result->getString("updated_by").c_str();
	}
	return settings;
}

void MySQLDatabase::SetFeaturedPropertiesSettings(const FeaturedSettings& settings) {
	ExecuteInsert(
		"INSERT INTO featured_properties_settings (id, full_auto, updated_at, updated_by) VALUES (1, ?, ?, ?) "
		"ON DUPLICATE KEY UPDATE full_auto = VALUES(full_auto), updated_at = VALUES(updated_at), updated_by = VALUES(updated_by);",
		settings.fullAuto ? 1 : 0, settings.updatedAt, settings.updatedBy);
}
