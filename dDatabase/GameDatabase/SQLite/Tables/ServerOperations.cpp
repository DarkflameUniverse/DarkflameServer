#include "SQLiteDatabase.h"

#include "GeneralUtils.h"

namespace {
	std::string JoinZones(const std::vector<uint32_t>& zones) {
		std::string text;
		for (const auto zone : zones) text += (text.empty() ? "" : ",") + std::to_string(zone);
		return text;
	}

	std::vector<uint32_t> SplitZones(const std::string& text) {
		std::vector<uint32_t> zones;
		for (const auto& part : GeneralUtils::SplitString(text, ',')) {
			if (const auto zone = GeneralUtils::TryParse<uint32_t>(part)) zones.push_back(*zone);
		}
		return zones;
	}

	IServerOperations::ScheduledAnnouncement ReadAnnouncement(CppSQLite3Query& result) {
		IServerOperations::ScheduledAnnouncement row;
		row.id = static_cast<uint64_t>(result.getInt64Field("id"));
		row.title = result.getStringField("title");
		row.message = result.getStringField("message");
		row.zones = SplitZones(result.getStringField("zones"));
		row.schedule = result.getStringField("schedule");
		row.startsAt = result.getInt64Field("starts_at");
		row.endsAt = result.getInt64Field("ends_at");
		row.enabled = result.getIntField("enabled") != 0;
		row.lastSentAt = result.getInt64Field("last_sent_at");
		row.sentCount = static_cast<uint32_t>(result.getIntField("sent_count"));
		row.createdAt = result.getInt64Field("created_at");
		row.createdBy = result.getStringField("created_by");
		row.updatedAt = result.getInt64Field("updated_at");
		row.updatedBy = result.getStringField("updated_by");
		return row;
	}

	IServerOperations::ScheduledEvent ReadEvent(CppSQLite3Query& result) {
		IServerOperations::ScheduledEvent row;
		row.id = static_cast<uint64_t>(result.getInt64Field("id"));
		row.name = result.getStringField("name");
		row.note = result.getStringField("note");
		row.mode = static_cast<uint8_t>(result.getIntField("mode"));
		row.schedule = result.getStringField("schedule");
		row.startsAt = result.getInt64Field("starts_at");
		row.endsAt = result.getInt64Field("ends_at");
		row.priority = result.getIntField("priority");
		row.parts = result.getStringField("parts");
		row.state = static_cast<IServerOperations::eEventState>(result.getIntField("state"));
		row.status = result.getStringField("status");
		row.createdAt = result.getInt64Field("created_at");
		row.createdBy = result.getStringField("created_by");
		row.updatedAt = result.getInt64Field("updated_at");
		row.updatedBy = result.getStringField("updated_by");
		return row;
	}
}

std::vector<IServerOperations::ScheduledAnnouncement> SQLiteDatabase::GetScheduledAnnouncements() {
	std::vector<ScheduledAnnouncement> rows;
	auto [_, result] = ExecuteSelect("SELECT * FROM scheduled_announcements ORDER BY id;");
	for (; !result.eof(); result.nextRow()) rows.push_back(ReadAnnouncement(result));
	return rows;
}

uint64_t SQLiteDatabase::InsertScheduledAnnouncement(const ScheduledAnnouncement& row) {
	ExecuteInsert("INSERT INTO scheduled_announcements (title, message, zones, schedule, starts_at, ends_at, enabled, created_at, created_by, updated_at, updated_by) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		row.title, row.message, JoinZones(row.zones), row.schedule, row.startsAt, row.endsAt, row.enabled, row.createdAt, row.createdBy, row.updatedAt, row.updatedBy);
	auto [_, result] = ExecuteSelect("SELECT last_insert_rowid() AS id;");
	return result.eof() ? 0 : static_cast<uint64_t>(result.getInt64Field("id"));
}

void SQLiteDatabase::UpdateScheduledAnnouncement(const ScheduledAnnouncement& row) {
	ExecuteUpdate("UPDATE scheduled_announcements SET title = ?, message = ?, zones = ?, schedule = ?, starts_at = ?, ends_at = ?, enabled = ?, updated_at = ?, updated_by = ? WHERE id = ?;",
		row.title, row.message, JoinZones(row.zones), row.schedule, row.startsAt, row.endsAt, row.enabled, row.updatedAt, row.updatedBy, static_cast<int64_t>(row.id));
}

void SQLiteDatabase::MarkAnnouncementSent(uint64_t id, int64_t time) {
	ExecuteUpdate("UPDATE scheduled_announcements SET last_sent_at = ?, sent_count = sent_count + 1 WHERE id = ?;", time, static_cast<int64_t>(id));
}

void SQLiteDatabase::DeleteScheduledAnnouncement(uint64_t id) {
	ExecuteDelete("DELETE FROM scheduled_announcements WHERE id = ?;", static_cast<int64_t>(id));
}

std::vector<IServerOperations::ScheduledEvent> SQLiteDatabase::GetScheduledEvents() {
	std::vector<ScheduledEvent> rows;
	auto [_, result] = ExecuteSelect("SELECT * FROM scheduled_events ORDER BY id;");
	for (; !result.eof(); result.nextRow()) rows.push_back(ReadEvent(result));
	return rows;
}

uint64_t SQLiteDatabase::InsertScheduledEvent(const ScheduledEvent& row) {
	ExecuteInsert("INSERT INTO scheduled_events (feature, name, note, mode, schedule, starts_at, ends_at, priority, parts, state, status, created_at, created_by, updated_at, updated_by) "
		"VALUES ('', ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		row.name, row.note, static_cast<uint32_t>(row.mode), row.schedule, row.startsAt, row.endsAt, row.priority, row.parts, static_cast<uint8_t>(row.state), row.status,
		row.createdAt, row.createdBy, row.updatedAt, row.updatedBy);
	auto [_, result] = ExecuteSelect("SELECT last_insert_rowid() AS id;");
	return result.eof() ? 0 : static_cast<uint64_t>(result.getInt64Field("id"));
}

void SQLiteDatabase::UpdateScheduledEvent(const ScheduledEvent& row) {
	ExecuteUpdate("UPDATE scheduled_events SET name = ?, note = ?, mode = ?, schedule = ?, starts_at = ?, ends_at = ?, priority = ?, parts = ?, state = ?, status = ?, "
		"updated_at = ?, updated_by = ? WHERE id = ?;",
		row.name, row.note, static_cast<uint32_t>(row.mode), row.schedule, row.startsAt, row.endsAt, row.priority, row.parts, static_cast<uint8_t>(row.state), row.status,
		row.updatedAt, row.updatedBy, static_cast<int64_t>(row.id));
}

void SQLiteDatabase::DeleteScheduledEvent(uint64_t id) {
	ExecuteDelete("DELETE FROM scheduled_events WHERE id = ?;", static_cast<int64_t>(id));
}

std::vector<IServerOperations::ZoneLimit> SQLiteDatabase::GetZoneLimits() {
	std::vector<ZoneLimit> rows;
	auto [_, result] = ExecuteSelect("SELECT * FROM zone_limits ORDER BY zone_id;");
	for (; !result.eof(); result.nextRow()) {
		ZoneLimit row;
		row.zoneId = static_cast<uint32_t>(result.getIntField("zone_id"));
		if (!result.fieldIsNull("soft_cap")) row.softCap = static_cast<uint32_t>(result.getIntField("soft_cap"));
		if (!result.fieldIsNull("hard_cap")) row.hardCap = static_cast<uint32_t>(result.getIntField("hard_cap"));
		row.spareInstances = static_cast<uint32_t>(result.getIntField("spare_instances"));
		row.updatedAt = result.getInt64Field("updated_at");
		row.updatedBy = result.getStringField("updated_by");
		rows.push_back(std::move(row));
	}
	return rows;
}

void SQLiteDatabase::SetZoneLimit(const ZoneLimit& row) {
	ExecuteInsert("INSERT INTO zone_limits (zone_id, soft_cap, hard_cap, spare_instances, updated_at, updated_by) VALUES (?, ?, ?, ?, ?, ?) "
		"ON CONFLICT(zone_id) DO UPDATE SET soft_cap = excluded.soft_cap, hard_cap = excluded.hard_cap, spare_instances = excluded.spare_instances, "
		"updated_at = excluded.updated_at, updated_by = excluded.updated_by;",
		row.zoneId, row.softCap, row.hardCap, row.spareInstances, row.updatedAt, row.updatedBy);
}

void SQLiteDatabase::DeleteZoneLimit(uint32_t zoneId) {
	ExecuteDelete("DELETE FROM zone_limits WHERE zone_id = ?;", zoneId);
}
