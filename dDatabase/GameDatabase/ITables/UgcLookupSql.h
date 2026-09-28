#ifndef UGCLOOKUPSQL_H
#define UGCLOOKUPSQL_H

#include <string>
#include <vector>

#include "IUgcLookup.h"

/**
 * The SQL IUgcLookup runs, the same on MySQL and SQLite. Ids are numbers, so they are written into the query; the
 * search text is bound (TEXT_BINDS times, always, so the statement's parameters don't change with the search).
 */
namespace UgcLookupSql {
	constexpr int TEXT_BINDS = 6;

	inline std::string IdList(const std::vector<LWOOBJID>& ids) {
		std::string list;
		for (const auto id : ids) list += (list.empty() ? "" : ",") + std::to_string(id);
		return list;
	}

	// The tables every entry query reads, for models (ugc AS u) or modular builds (ugc_modular_build AS b)
	inline std::string From(bool modular) {
		return modular
			? "FROM ugc_modular_build AS b LEFT JOIN charinfo AS c ON c.id = b.character_id LEFT JOIN accounts AS a ON a.id = c.account_id "
			: "FROM ugc AS u LEFT JOIN charinfo AS c ON c.id = u.character_id LEFT JOIN accounts AS a ON a.id = u.account_id ";
	}

	// The columns every entry query selects
	inline std::string Select(bool modular) {
		return (modular
			? "SELECT b.ugc_id AS id, b.character_id, c.name AS character_name, COALESCE(c.account_id, 0) AS account_id, a.name AS account_name, "
			  "b.is_optimized, b.process_error, b.ldf_config AS detail, b.process_attempts, b.processed_at, 0 AS process_after, 0 AS bake_ao, "
			  "0 AS brick_count, 0 AS triangle_count, b.process_ms, b.process_cpu_ms, b.process_memory_kb "
			: "SELECT u.id, u.character_id, c.name AS character_name, u.account_id, a.name AS account_name, "
			  "u.is_optimized, u.process_error, u.filename AS detail, u.process_attempts, u.processed_at, u.process_after, u.bake_ao, "
			  "u.brick_count, u.triangle_count, u.process_ms, u.process_cpu_ms, u.process_memory_kb ") + From(modular);
	}

	// Whether a search has anything to match (else a list is of everything)
	inline bool Searching(const IUgcLookup::UgcSearch& search) { return !search.text.empty() || search.number.has_value(); }

	// A list's state filter and order (after Where, or "WHERE 1=1 " when not searching)
	inline std::string ListFilter(const IUgcLookup::UgcListQuery& query, bool modular) {
		const std::string table = modular ? "b." : "u.";
		std::string sql = query.state ? "AND " + table + "is_optimized = " + std::to_string(static_cast<int32_t>(*query.state)) + " " : "";
		return sql;
	}

	inline std::string ListOrder(const IUgcLookup::UgcListQuery& query, bool modular) {
		using eSort = IUgcLookup::eSort;
		const std::string id = modular ? "b.ugc_id" : "u.id";
		// A sort's own direction, or the other one when the query is reversed; ties stay newest first
		const auto dir = [&query](bool descending) { return descending != query.reverse ? " DESC" : " ASC"; };
		switch (query.sort) {
		case eSort::OLDEST: return "ORDER BY " + id + dir(false) + " ";
		case eSort::OWNER: return "ORDER BY c.name" + std::string(dir(false)) + ", " + id + " DESC ";
		case eSort::NAME: return std::string("ORDER BY ") + (modular ? "b.ldf_config" : "u.filename") + dir(false) + ", " + id + " DESC ";
		case eSort::BRICKS: return modular ? "ORDER BY " + id + dir(true) + " " : "ORDER BY u.brick_count" + std::string(dir(true)) + ", u.id DESC ";
		case eSort::TRIANGLES: return modular ? "ORDER BY " + id + dir(true) + " " : "ORDER BY u.triangle_count" + std::string(dir(true)) + ", u.id DESC ";
		case eSort::MADE: return "ORDER BY " + std::string(modular ? "b" : "u") + ".processed_at" + dir(true) + ", " + id + " DESC ";
		case eSort::SLOWEST: return "ORDER BY " + std::string(modular ? "b" : "u") + ".process_ms" + dir(true) + ", " + id + " DESC ";
		default: return "ORDER BY " + id + dir(true) + " ";
		}
	}

	// WHERE for a search; binds the search's %text% TEXT_BINDS times
	inline std::string Where(const IUgcLookup::UgcSearch& search, bool modular) {
		using eField = IUgcLookup::UgcSearch::eField;
		const auto field = search.field;
		const bool any = field == eField::ANY;
		const bool num = search.number.has_value();
		const bool txt = !search.text.empty();
		const std::string n = num ? std::to_string(*search.number) : "NULL";
		const std::string id = modular ? "b.ugc_id" : "u.id";
		const std::string character = modular ? "b.character_id" : "u.character_id";
		const std::string account = modular ? "c.account_id" : "u.account_id";
		const std::string detail = modular ? "b.ldf_config" : "u.filename";
		const auto on = [](bool value) { return std::string(value ? "1=1" : "0=1"); };
		const std::string placed = "SELECT 1 FROM properties_contents AS pc WHERE pc.ugc_id = " + id;

		std::string where = "WHERE (0=1";
		if (num && (any || field == eField::ID)) {
			where += " OR " + id + " = " + n + " OR " + character + " = " + n + " OR " + account + " = " + n +
				" OR EXISTS (" + placed + " AND (pc.id = " + n + " OR pc.property_id = " + n + "))";
		}
		if (num && field == eField::OWNER) where += " OR " + character + " = " + n + " OR " + account + " = " + n;
		if (num && (any || field == eField::LOT)) {
			if (modular) where += " OR b.ldf_config LIKE '%:" + n + "+%' OR b.ldf_config LIKE '%:" + n + "'";
			else where += " OR EXISTS (" + placed + " AND pc.lot = " + n + ")";
		}
		// The bound text: owner (2), property (1), model (3)
		where += " OR (" + on(txt && (any || field == eField::OWNER)) + " AND (c.name LIKE ? OR a.name LIKE ?))";
		where += " OR (" + on((txt && any) || field == eField::PROPERTY) + " AND EXISTS (SELECT 1 FROM properties_contents AS pc "
			"JOIN properties AS p ON p.id = pc.property_id WHERE pc.ugc_id = " + id + " AND ((" + on(txt) + " AND p.name LIKE ?) OR p.id = " + n + ")))";
		where += " OR (" + on(txt && (any || field == eField::MODEL)) + " AND (" + detail + " LIKE ? OR EXISTS (" + placed +
			" AND (pc.model_name LIKE ? OR pc.model_description LIKE ?))))";
		return where + ") ";
	}

	inline std::string Placements(const std::vector<LWOOBJID>& ugcIds) {
		return "SELECT pc.ugc_id, pc.id, pc.lot, pc.property_id, p.name AS property_name, p.owner_id, c.name AS owner_name, p.zone_id, "
			"pc.model_name, pc.model_description FROM properties_contents AS pc JOIN properties AS p ON p.id = pc.property_id "
			"LEFT JOIN charinfo AS c ON c.id = p.owner_id WHERE pc.ugc_id IN (" + IdList(ugcIds) + ") ORDER BY pc.id;";
	}

	inline std::string Mail(const std::vector<LWOOBJID>& subkeys, LOT modelItemLot) {
		return "SELECT id, receiver_id, receiver_name, attachment_lot, attachment_subkey, attachment_config FROM mail WHERE attachment_count > 0 AND "
			"(attachment_lot = " + std::to_string(modelItemLot) + (subkeys.empty() ? "" : " OR attachment_subkey IN (" + IdList(subkeys) + ")") +
			") ORDER BY id DESC LIMIT 500;";
	}
}

#endif  //!UGCLOOKUPSQL_H
