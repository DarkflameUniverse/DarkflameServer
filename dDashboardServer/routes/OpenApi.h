#pragma once

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include "json.hpp"

/**
 * The dashboard's API as an OpenAPI 3 document, for Swagger UI on the API page (where each call can be tried out).
 *
 * Built from the registered routes: `:id` path parts become path parameters, and the parameters written in a route's
 * description in the house style become query parameters and a request body:
 *   "Query: ?range=24h|7d|30d, &files= (newest per server)"  or  "Query: after (the last id), limit (max 500)"
 *   "Body: {name, enabled, parent (default root.xml)}"        (also "Body (optional): {...}", "Body adds {...}")
 * Routes described as "(DataTables)" take the DataTables paging body. Pure (routes in, JSON out) so it can be tested.
 */
namespace OpenApi {
	struct Route {
		std::string method;      // GET, POST, ...
		std::string path;        // /api/accounts/:id
		std::string description;
		int minGmLevel{};
		std::string permission;  // empty when the route needs a GM level rather than a named permission
	};

	struct Param {
		std::string name;
		std::string description;
		std::vector<std::string> choices; // from "a|b|c"
		bool isList{};                    // "name: [...]"
	};

	namespace Detail {
		inline std::string Trim(std::string_view text) {
			size_t start = 0, end = text.size();
			while (start < end && std::isspace(static_cast<unsigned char>(text[start]))) start++;
			while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) end--;
			return std::string(text.substr(start, end - start));
		}

		// Split on commas and '&' that aren't inside (), [] or {}
		inline std::vector<std::string> SplitTopLevel(std::string_view text) {
			std::vector<std::string> parts;
			int depth = 0;
			size_t start = 0;
			for (size_t i = 0; i < text.size(); i++) {
				const char c = text[i];
				if (c == '(' || c == '[' || c == '{') depth++;
				else if ((c == ')' || c == ']' || c == '}') && depth > 0) depth--;
				else if ((c == ',' || c == '&') && depth == 0) {
					parts.push_back(Trim(text.substr(start, i - start)));
					start = i + 1;
				}
			}
			parts.push_back(Trim(text.substr(start)));
			return parts;
		}

		// "?range=24h|7d|30d", "&files= (newest per server)", "after (the last id)", "changes: [{...}]"
		inline std::optional<Param> ParseParam(std::string text) {
			while (!text.empty() && (text[0] == '?' || text[0] == '&')) text.erase(0, 1);
			size_t i = 0;
			while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_')) i++;
			if (i == 0) return std::nullopt;
			Param param{ text.substr(0, i) };
			auto rest = Trim(std::string_view(text).substr(i));
			if (rest.starts_with("=")) {
				auto value = rest.substr(1);
				const auto space = value.find_first_of(" (");
				const auto choices = Trim(std::string_view(value).substr(0, space));
				rest = space == std::string::npos ? "" : Trim(std::string_view(value).substr(space));
				if (choices.find('|') != std::string::npos) {
					size_t from = 0;
					for (size_t bar = choices.find('|'); ; bar = choices.find('|', from)) {
						param.choices.push_back(choices.substr(from, bar == std::string::npos ? std::string::npos : bar - from));
						if (bar == std::string::npos) break;
						from = bar + 1;
					}
				}
			} else if (rest.starts_with(":")) {
				param.isList = Trim(std::string_view(rest).substr(1)).starts_with("[");
				rest = "";
			}
			if (rest.starts_with("(") && rest.ends_with(")")) rest = rest.substr(1, rest.size() - 2);
			param.description = rest;
			return param;
		}

		// The text after `marker` up to the end of its sentence (a ". " outside brackets, or the end)
		inline std::optional<std::string> Clause(const std::string& text, const std::string& marker) {
			const auto at = text.find(marker);
			if (at == std::string::npos) return std::nullopt;
			const auto start = at + marker.size();
			int depth = 0;
			for (size_t i = start; i < text.size(); i++) {
				const char c = text[i];
				if (c == '(' || c == '[' || c == '{') depth++;
				else if ((c == ')' || c == ']' || c == '}') && depth > 0) depth--;
				else if (depth == 0 && c == '.' && (i + 1 == text.size() || text[i + 1] == ' ')) return Trim(std::string_view(text).substr(start, i - start));
			}
			return Trim(std::string_view(text).substr(start));
		}

		// The first {...} (balanced) at or after `from`
		inline std::optional<std::string> Braces(const std::string& text, size_t from) {
			const auto open = text.find('{', from);
			if (open == std::string::npos) return std::nullopt;
			int depth = 0;
			for (size_t i = open; i < text.size(); i++) {
				if (text[i] == '{') depth++;
				else if (text[i] == '}' && --depth == 0) return text.substr(open + 1, i - open - 1);
			}
			return std::nullopt;
		}
	}

	inline std::vector<Param> QueryParams(const std::string& description) {
		std::vector<Param> params;
		const auto clause = Detail::Clause(description, "Query:");
		if (!clause) return params;
		for (const auto& part : Detail::SplitTopLevel(*clause)) {
			if (auto param = Detail::ParseParam(part)) params.push_back(std::move(*param));
		}
		return params;
	}

	inline std::vector<Param> BodyParams(const std::string& description) {
		std::vector<Param> params;
		const auto at = description.find("Body");
		if (at == std::string::npos) return params;
		const auto braces = Detail::Braces(description, at);
		if (!braces) return params;
		for (const auto& part : Detail::SplitTopLevel(*braces)) {
			if (auto param = Detail::ParseParam(part)) params.push_back(std::move(*param));
		}
		return params;
	}

	// The first sentence, for the one-line summary
	inline std::string Summary(const std::string& description) {
		const auto clause = Detail::Clause(" " + description, " ");
		auto summary = clause ? *clause : description;
		return summary.size() > 120 ? summary.substr(0, 117) + "..." : summary;
	}

	inline nlohmann::json Build(const std::vector<Route>& routes, const std::string& title) {
		nlohmann::json paths = nlohmann::json::object();
		for (const auto& route : routes) {
			std::string path;
			nlohmann::json parameters = nlohmann::json::array();
			std::string tag;
			size_t segment = 0;
			for (size_t i = 0; i < route.path.size();) {
				const auto next = route.path.find('/', i + 1);
				auto part = route.path.substr(i + 1, (next == std::string::npos ? route.path.size() : next) - i - 1);
				if (part.starts_with(":")) {
					part = part.substr(1);
					parameters.push_back({ {"name", part}, {"in", "path"}, {"required", true}, {"schema", { {"type", "string"} }} });
					path += "/{" + part + "}";
				} else {
					path += "/" + part;
					if (segment == 1) tag = part; // /api/<tag>/...
				}
				segment++;
				if (next == std::string::npos) break;
				i = next;
			}
			for (const auto& param : QueryParams(route.description)) {
				nlohmann::json schema{ {"type", "string"} };
				if (!param.choices.empty()) schema["enum"] = param.choices;
				parameters.push_back({ {"name", param.name}, {"in", "query"}, {"required", false}, {"description", param.description}, {"schema", schema} });
			}

			auto method = route.method;
			for (auto& c : method) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			std::string access = route.permission.empty()
				? (route.minGmLevel <= 0 ? "Any signed-in account" : "GM " + std::to_string(route.minGmLevel) + "+")
				: "Permission `" + route.permission + "` (GM " + std::to_string(route.minGmLevel) + "+ now)";
			nlohmann::json operation{
				{"summary", Summary(route.description)},
				{"description", route.description + "\n\n**Who:** " + access},
				{"tags", { tag.empty() ? "pages" : tag }},
				{"parameters", parameters},
				{"responses", { {"200", { {"description", "OK"} }}, {"400", { {"description", "Bad request: {error}"} }},
					{"401", { {"description", "Not signed in"} }}, {"403", { {"description", "Not allowed at your GM level"} }}, {"404", { {"description", "Not found"} }} }}
			};
			if (!route.permission.empty()) operation["x-permission"] = route.permission;

			if (method != "get") {
				nlohmann::json properties = nlohmann::json::object(), example = nlohmann::json::object();
				if (route.description.find("(DataTables)") != std::string::npos) {
					for (const auto& [name, value] : { std::pair<const char*, nlohmann::json>{ "draw", 1 }, { "start", 0 }, { "length", 25 }, { "search", "" } }) {
						properties[name] = { {"type", value.is_string() ? "string" : "integer"} };
						example[name] = value;
					}
				}
				for (const auto& param : BodyParams(route.description)) {
					nlohmann::json schema = param.isList ? nlohmann::json{ {"type", "array"}, {"items", nlohmann::json::object()} } : nlohmann::json::object();
					if (!param.choices.empty()) schema["enum"] = param.choices;
					if (!param.description.empty()) schema["description"] = param.description;
					properties[param.name] = schema;
					example[param.name] = param.isList ? nlohmann::json::array() : nlohmann::json(param.choices.empty() ? "" : param.choices.front());
				}
				operation["requestBody"] = { {"required", false}, {"content", { {"application/json", { {"schema", { {"type", "object"}, {"properties", properties} }}, {"example", example} }} }} };
			}
			paths[path][method] = operation;
		}
		return {
			{"openapi", "3.0.3"},
			{"info", { {"title", title}, {"version", "1"},
				{"description", "Every endpoint your account may use. Sign in on the dashboard (your session is used here) or send "
					"`Authorization: Bearer <token>` with a token from your account page. Requests signed in with the session cookie must also "
					"send `X-Requested-With` (this page does)."} }},
			{"components", { {"securitySchemes", {
				{"bearer", { {"type", "http"}, {"scheme", "bearer"}, {"description", "An API token from your account page"} }},
				{"session", { {"type", "apiKey"}, {"in", "cookie"}, {"name", "dashboardToken"}, {"description", "Your dashboard sign-in"} }} }} }},
			{"security", { { {"session", nlohmann::json::array()} }, { {"bearer", nlohmann::json::array()} } }},
			{"paths", paths}
		};
	}
}
