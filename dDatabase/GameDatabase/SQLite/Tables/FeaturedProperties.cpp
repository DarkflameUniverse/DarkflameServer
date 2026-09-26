#include "SQLiteDatabase.h"

std::vector<IFeaturedProperties::FeaturedSlot> SQLiteDatabase::GetFeaturedPropertySlots() {
	std::vector<FeaturedSlot> slots;
	auto [_, result] = ExecuteSelect("SELECT * FROM featured_properties ORDER BY template_id;");
	for (; !result.eof(); result.nextRow()) {
		slots.push_back({ static_cast<uint32_t>(result.getIntField("template_id")), static_cast<uint8_t>(result.getIntField("mode")),
			result.getInt64Field("property_id"), result.getInt64Field("updated_at"), result.getStringField("updated_by"),
			static_cast<uint32_t>(result.getInt64Field("zone_id")) });
	}
	return slots;
}

void SQLiteDatabase::SetFeaturedPropertySlot(const FeaturedSlot& slot) {
	ExecuteInsert(
		"INSERT INTO featured_properties (template_id, mode, property_id, updated_at, updated_by, zone_id) VALUES (?, ?, ?, ?, ?, ?) "
		"ON CONFLICT(template_id) DO UPDATE SET mode = excluded.mode, property_id = excluded.property_id, updated_at = excluded.updated_at, updated_by = excluded.updated_by, zone_id = excluded.zone_id;",
		slot.templateId, static_cast<uint32_t>(slot.mode), slot.propertyId, slot.updatedAt, slot.updatedBy, slot.zoneId);
}

IFeaturedProperties::FeaturedSettings SQLiteDatabase::GetFeaturedPropertiesSettings() {
	FeaturedSettings settings;
	auto [_, result] = ExecuteSelect("SELECT * FROM featured_properties_settings WHERE id = 1;");
	if (!result.eof()) {
		settings.fullAuto = result.getIntField("full_auto") != 0;
		settings.updatedAt = result.getInt64Field("updated_at");
		settings.updatedBy = result.getStringField("updated_by");
	}
	return settings;
}

void SQLiteDatabase::SetFeaturedPropertiesSettings(const FeaturedSettings& settings) {
	ExecuteInsert(
		"INSERT INTO featured_properties_settings (id, full_auto, updated_at, updated_by) VALUES (1, ?, ?, ?) "
		"ON CONFLICT(id) DO UPDATE SET full_auto = excluded.full_auto, updated_at = excluded.updated_at, updated_by = excluded.updated_by;",
		settings.fullAuto ? 1 : 0, settings.updatedAt, settings.updatedBy);
}
