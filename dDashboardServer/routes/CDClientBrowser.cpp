#include "CDClientBrowser.h"

#include "CDClientDatabase.h"
#include "CDClientSchema.h"
#include "GameLabels.h"
#include "GeneralUtils.h"
#include "RouteUtils.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;
using namespace CDClientSchema;
using json = nlohmann::json;

namespace {
	constexpr uint32_t DEFAULT_PAGE = 50;

	// Read from the database itself once: every table and its columns
	const Schema& GetSchema() {
		static const Schema schema = [] {
			Schema loaded;
			std::vector<std::string> names;
			auto tables = CDClientDatabase::ExecuteQuery("SELECT name FROM sqlite_master WHERE type = 'table' ORDER BY name;");
			while (!tables.eof()) {
				names.emplace_back(tables.getStringField(0, ""));
				tables.nextRow();
			}
			for (const auto& name : names) {
				Table table{ name, {} };
				auto columns = CDClientDatabase::ExecuteQuery("PRAGMA table_info(" + Quote(name) + ");");
				while (!columns.eof()) {
					table.columns.push_back({ columns.getStringField("name", ""), columns.getStringField("type", "") });
					columns.nextRow();
				}
				loaded.Add(std::move(table));
			}
			return loaded;
		}();
		return schema;
	}

	std::string Text(const json& row, const std::string& key, const std::string& fallback = "") {
		const auto it = row.find(key);
		return it != row.end() && it->is_string() ? it->get<std::string>() : fallback;
	}

	void Bind(CppSQLite3Statement& statement, const std::vector<Value>& params) {
		for (size_t i = 0; i < params.size(); i++) {
			const int index = static_cast<int>(i + 1);
			std::visit([&](const auto& value) {
				using T = std::decay_t<decltype(value)>;
				if constexpr (std::is_same_v<T, std::string>) statement.bind(index, value.c_str());
				else if constexpr (std::is_same_v<T, int64_t>) statement.bind(index, static_cast<sqlite_int64>(value));
				else statement.bind(index, value);
			}, params[i]);
		}
	}

	json RowJson(CppSQLite3Query& query) {
		json row = json::object();
		for (int i = 0; i < query.numFields(); i++) {
			const std::string name = query.fieldName(i);
			switch (query.fieldDataType(i)) {
			case SQLITE_INTEGER: row[name] = query.getInt64Field(i); break;
			case SQLITE_FLOAT: row[name] = query.getFloatField(i); break;
			case SQLITE_NULL: row[name] = nullptr; break;
			default: row[name] = query.getStringField(i, ""); break;
			}
		}
		return row;
	}

	// Rows of a query built by BuildRowQuery (only tables and columns from the schema, values bound)
	json Rows(const std::string& sql, const std::vector<Value>& params, size_t max) {
		auto statement = CDClientDatabase::CreatePreppedStmt(sql);
		Bind(statement, params);
		auto query = statement.execQuery();
		json rows = json::array();
		while (!query.eof() && rows.size() < max) {
			rows.push_back(RowJson(query));
			query.nextRow();
		}
		return rows;
	}

	int64_t Count(const std::string& sql, const std::vector<Value>& params) {
		auto statement = CDClientDatabase::CreatePreppedStmt(sql);
		Bind(statement, params);
		auto query = statement.execQuery();
		return query.eof() ? 0 : query.getInt64Field(0);
	}

	json Columns(const Table& table) {
		json columns = json::array();
		for (const auto& column : table.columns) {
			const auto link = LinkFor(GetSchema(), table, column);
			columns.push_back({ {"name", column.name}, {"type", column.type}, {"link", link ? json(LinkName(*link)) : json(nullptr)} });
		}
		return columns;
	}

	std::string ComponentTypeName(int64_t type) {
		const auto name = magic_enum::enum_name(static_cast<eReplicaComponentType>(type));
		return name.empty() ? "Type " + std::to_string(type) : GameLabels::Words(name);
	}
}

void RegisterCDClientBrowserRoutes() {
	Route(eHTTPMethod::GET, "/cdclient", Perm("dev_cdclient"), "The CDClient browser",
		[](HTTPReply& reply, const HTTPContext& context) {
			RenderPage(reply, context, "cdclient.jinja2", "cdclient");
		});

	Route(eHTTPMethod::GET, "/api/cdclient/schema", Perm("dev_cdclient"), "Every CDClient table with its columns and the table each linking column points at",
		[](HTTPReply& reply, const HTTPContext&) {
			static const auto schema = [] {
				json tables = json::array();
				for (const auto& [name, table] : GetSchema().Tables()) tables.push_back({ {"name", name}, {"columns", Columns(table)} });
				json links = json::object();
				for (const auto link : magic_enum::enum_values<eLink>()) {
					const auto target = Target(link);
					links[LinkName(link)] = { {"table", target.table}, {"column", target.column} };
				}
				json componentTypes = json::array();
				for (const auto type : magic_enum::enum_values<eReplicaComponentType>()) {
					const auto* table = ComponentTable(GetSchema(), type);
					componentTypes.push_back({ {"type", static_cast<int64_t>(type)}, {"name", ComponentTypeName(static_cast<int64_t>(type))}, {"table", table ? json(table->name) : json(nullptr)} });
				}
				return json{ {"tables", tables}, {"links", links}, {"componentTypes", componentTypes} };
			}();
			JsonReply(reply, eHTTPStatusCode::OK, schema);
		});

	Route(eHTTPMethod::GET, "/api/cdclient/tables/:name/rows", Perm("dev_cdclient"),
		"A page of a CDClient table. Query: start, length (max 500), order (column), dir (asc/desc), q (search), filters (JSON [{column, op, value}]; op is = != < <= > >= contains starts null notnull)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto& query = context.queryString;
			RowQuery request;
			request.table = std::string(PathSegment(context.originalPath, 3)); // table names mix cases
			request.start = GeneralUtils::TryParse<uint32_t>(QueryValue(query, "start")).value_or(0);
			request.length = GeneralUtils::TryParse<uint32_t>(QueryValue(query, "length")).value_or(DEFAULT_PAGE);
			request.orderColumn = QueryValue(query, "order");
			request.ascending = QueryValue(query, "dir") != "desc";
			request.search = QueryValue(query, "q");
			const auto filters = QueryValue(query, "filters");
			if (!filters.empty()) {
				const auto parsed = json::parse(filters, nullptr, false);
				if (!parsed.is_array()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "filters must be a JSON array");
				for (const auto& filter : parsed) {
					if (!filter.is_object()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Each filter is {column, op, value}");
					const auto& value = filter.contains("value") ? filter["value"] : json("");
					request.filters.push_back({ Text(filter, "column"), Text(filter, "op", "="), value.is_string() ? value.get<std::string>() : value.dump() });
				}
			}

			std::string error;
			const auto built = BuildRowQuery(GetSchema(), request, error);
			if (!built) return JsonError(reply, error == "Unknown table" ? eHTTPStatusCode::NOT_FOUND : eHTTPStatusCode::BAD_REQUEST, error);
			const auto* table = GetSchema().Find(request.table);
			JsonReply(reply, eHTTPStatusCode::OK, {
				{"table", table->name},
				{"columns", Columns(*table)},
				{"total", Count(built->count, built->countParams)},
				{"start", request.start},
				{"rows", Rows(built->select, built->params, MAX_ROWS)}
			});
		});
}
