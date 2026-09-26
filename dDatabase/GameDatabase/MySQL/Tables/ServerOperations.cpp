#include "MySQLDatabase.h"

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

	std::string Text(PreparedStmtResultSet& result, const char* field) {
		return result->isNull(field) ? "" : std::string(result->getString(field).c_str());
	}

	IServerOperations::ScheduledAnnouncement ReadAnnouncement(PreparedStmtResultSet& result) {
		IServerOperations::ScheduledAnnouncement row;
		row.id = result->getUInt64("id");
		row.title = Text(result, "title");
		row.message = Text(result, "message");
		row.zones = SplitZones(Text(result, "zones"));
		row.schedule = Text(result, "schedule");
		row.startsAt = result->getInt64("starts_at");
		row.endsAt = result->getInt64("ends_at");
		row.enabled = result->getInt("enabled") != 0;
		row.lastSentAt = result->getInt64("last_sent_at");
		row.sentCount = result->getUInt("sent_count");
		row.createdAt = result->getInt64("created_at");
		row.createdBy = Text(result, "created_by");
		row.updatedAt = result->getInt64("updated_at");
		row.updatedBy = Text(result, "updated_by");
		return row;
	}

	IServerOperations::ScheduledEvent ReadEvent(PreparedStmtResultSet& result) {
		IServerOperations::ScheduledEvent row;
		row.id = result->getUInt64("id");
		row.name = Text(result, "name");
		row.note = Text(result, "note");
		row.mode = static_cast<uint8_t>(result->getUInt("mode"));
		row.schedule = Text(result, "schedule");
		row.startsAt = result->getInt64("starts_at");
		row.endsAt = result->getInt64("ends_at");
		row.priority = result->getInt("priority");
		row.parts = Text(result, "parts");
		row.state = static_cast<IServerOperations::eEventState>(result->getInt("state"));
		row.status = Text(result, "status");
		row.createdAt = result->getInt64("created_at");
		row.createdBy = Text(result, "created_by");
		row.updatedAt = result->getInt64("updated_at");
		row.updatedBy = Text(result, "updated_by");
		return row;
	}
}

std::vector<IServerOperations::ScheduledAnnouncement> MySQLDatabase::GetScheduledAnnouncements() {
	std::vector<ScheduledAnnouncement> rows;
	auto result = ExecuteSelect("SELECT * FROM scheduled_announcements ORDER BY id;");
	while (result->next()) rows.push_back(ReadAnnouncement(result));
	return rows;
}

uint64_t MySQLDatabase::InsertScheduledAnnouncement(const ScheduledAnnouncement& row) {
	ExecuteInsert("INSERT INTO scheduled_announcements (title, message, zones, schedule, starts_at, ends_at, enabled, created_at, created_by, updated_at, updated_by) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		row.title, row.message, JoinZones(row.zones), row.schedule, row.startsAt, row.endsAt, row.enabled, row.createdAt, row.createdBy, row.updatedAt, row.updatedBy);
	auto result = ExecuteSelect("SELECT LAST_INSERT_ID() AS id;");
	return result->next() ? result->getUInt64("id") : 0;
}

void MySQLDatabase::UpdateScheduledAnnouncement(const ScheduledAnnouncement& row) {
	ExecuteUpdate("UPDATE scheduled_announcements SET title = ?, message = ?, zones = ?, schedule = ?, starts_at = ?, ends_at = ?, enabled = ?, updated_at = ?, updated_by = ? WHERE id = ?;",
		row.title, row.message, JoinZones(row.zones), row.schedule, row.startsAt, row.endsAt, row.enabled, row.updatedAt, row.updatedBy, row.id);
}

void MySQLDatabase::MarkAnnouncementSent(uint64_t id, int64_t time) {
	ExecuteUpdate("UPDATE scheduled_announcements SET last_sent_at = ?, sent_count = sent_count + 1 WHERE id = ?;", time, id);
}

void MySQLDatabase::DeleteScheduledAnnouncement(uint64_t id) {
	ExecuteDelete("DELETE FROM scheduled_announcements WHERE id = ?;", id);
}

std::vector<IServerOperations::ScheduledEvent> MySQLDatabase::GetScheduledEvents() {
	std::vector<ScheduledEvent> rows;
	auto result = ExecuteSelect("SELECT * FROM scheduled_events ORDER BY id;");
	while (result->next()) rows.push_back(ReadEvent(result));
	return rows;
}

uint64_t MySQLDatabase::InsertScheduledEvent(const ScheduledEvent& row) {
	ExecuteInsert("INSERT INTO scheduled_events (feature, name, note, mode, schedule, starts_at, ends_at, priority, parts, state, status, created_at, created_by, updated_at, updated_by) "
		"VALUES ('', ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		row.name, row.note, static_cast<uint32_t>(row.mode), row.schedule, row.startsAt, row.endsAt, row.priority, row.parts, static_cast<uint8_t>(row.state), row.status,
		row.createdAt, row.createdBy, row.updatedAt, row.updatedBy);
	auto result = ExecuteSelect("SELECT LAST_INSERT_ID() AS id;");
	return result->next() ? result->getUInt64("id") : 0;
}

void MySQLDatabase::UpdateScheduledEvent(const ScheduledEvent& row) {
	ExecuteUpdate("UPDATE scheduled_events SET name = ?, note = ?, mode = ?, schedule = ?, starts_at = ?, ends_at = ?, priority = ?, parts = ?, state = ?, status = ?, "
		"updated_at = ?, updated_by = ? WHERE id = ?;",
		row.name, row.note, static_cast<uint32_t>(row.mode), row.schedule, row.startsAt, row.endsAt, row.priority, row.parts, static_cast<uint8_t>(row.state), row.status,
		row.updatedAt, row.updatedBy, row.id);
}

void MySQLDatabase::DeleteScheduledEvent(uint64_t id) {
	ExecuteDelete("DELETE FROM scheduled_events WHERE id = ?;", id);
}

std::vector<IServerOperations::ZoneLimit> MySQLDatabase::GetZoneLimits() {
	std::vector<ZoneLimit> rows;
	auto result = ExecuteSelect("SELECT * FROM zone_limits ORDER BY zone_id;");
	while (result->next()) {
		ZoneLimit row;
		row.zoneId = result->getUInt("zone_id");
		if (!result->isNull("soft_cap")) row.softCap = result->getUInt("soft_cap");
		if (!result->isNull("hard_cap")) row.hardCap = result->getUInt("hard_cap");
		row.spareInstances = result->getUInt("spare_instances");
		row.updatedAt = result->getInt64("updated_at");
		row.updatedBy = Text(result, "updated_by");
		rows.push_back(std::move(row));
	}
	return rows;
}

void MySQLDatabase::SetZoneLimit(const ZoneLimit& row) {
	ExecuteInsert("INSERT INTO zone_limits (zone_id, soft_cap, hard_cap, spare_instances, updated_at, updated_by) VALUES (?, ?, ?, ?, ?, ?) "
		"ON DUPLICATE KEY UPDATE soft_cap = VALUES(soft_cap), hard_cap = VALUES(hard_cap), spare_instances = VALUES(spare_instances), "
		"updated_at = VALUES(updated_at), updated_by = VALUES(updated_by);",
		row.zoneId, row.softCap, row.hardCap, row.spareInstances, row.updatedAt, row.updatedBy);
}

void MySQLDatabase::DeleteZoneLimit(uint32_t zoneId) {
	ExecuteDelete("DELETE FROM zone_limits WHERE zone_id = ?;", zoneId);
}
