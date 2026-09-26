#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <chrono>
#include <deque>
#include <map>

#include "json.hpp"
#include "Web.h"
#include "HTTPContext.h"
#include "GeneralUtils.h"
#include "dCommonVars.h"
#include "AccountRules.h"

/**
 * Shared helpers for dashboard routes.
 * The inline functions are pure so they can be unit tested without a database or web server.
 */
namespace RouteUtils {
	// Upper bound on rows a single table request may ask for
	constexpr uint32_t MAX_PAGE_LENGTH = 500;

	struct DataTablesRequest {
		uint32_t draw{1};
		uint32_t start{0};
		uint32_t length{10};
		std::string search{};
		uint32_t orderColumn{0};
		bool orderAsc{true};
	};

	// Parse a DataTables server-side request body. Missing fields fall back to defaults.
	inline std::optional<DataTablesRequest> ParseDataTablesRequest(const std::string& body) {
		const auto json = nlohmann::json::parse(body.empty() ? "{}" : body, nullptr, false);
		if (json.is_discarded() || !json.is_object()) return std::nullopt;

		const auto getUInt = [&json](const char* key, uint32_t fallback) -> uint32_t {
			if (!json.contains(key)) return fallback;
			const auto& value = json[key];
			if (value.is_number_unsigned()) return value.get<uint32_t>();
			if (value.is_number_integer()) return static_cast<uint32_t>(std::max<int64_t>(0, value.get<int64_t>()));
			return fallback;
		};

		DataTablesRequest request;
		request.draw = getUInt("draw", 1);
		request.start = getUInt("start", 0);
		request.length = std::clamp<uint32_t>(getUInt("length", 10), 1, MAX_PAGE_LENGTH);

		if (json.contains("search")) {
			const auto& search = json["search"];
			if (search.is_string()) request.search = search.get<std::string>();
			else if (search.is_object() && search.contains("value") && search["value"].is_string()) request.search = search["value"].get<std::string>();
		}

		if (json.contains("order") && json["order"].is_array() && !json["order"].empty() && json["order"][0].is_object()) {
			const auto& order = json["order"][0];
			if (order.contains("column") && order["column"].is_number_integer()) request.orderColumn = static_cast<uint32_t>(std::max<int64_t>(0, order["column"].get<int64_t>()));
			if (order.contains("dir") && order["dir"].is_string()) request.orderAsc = order["dir"].get<std::string>() != "desc";
		}
		return request;
	}

	// Get the path segment at index, e.g. index 2 of "/api/accounts/42/ban" is "42"
	inline std::string_view PathSegment(std::string_view path, size_t index) {
		size_t pos = 0;
		for (size_t i = 0; i <= index; i++) {
			while (pos < path.size() && path[pos] == '/') pos++;
			if (pos >= path.size()) return {};
			const size_t end = std::min(path.find('/', pos), path.size());
			if (i == index) return path.substr(pos, end - pos);
			pos = end;
		}
		return {};
	}

	// Parse a numeric ID from a path segment; rejects anything that is not entirely digits
	template<typename T>
	std::optional<T> PathId(std::string_view path, size_t index) {
		const auto segment = PathSegment(path, index);
		if (segment.empty() || !std::ranges::all_of(segment, [](char c) { return c >= '0' && c <= '9'; })) return std::nullopt;
		return GeneralUtils::TryParse<T>(segment);
	}

	/**
	 * One CSV cell. Quoted when it contains a comma, quote or line break. Text that a spreadsheet would run as a
	 * formula (starting with = + - @ tab or CR) gets a leading apostrophe, since names and bug text come from players.
	 */
	inline std::string CsvCell(std::string_view value) {
		std::string text(value);
		if (!text.empty() && std::string_view("=+-@\t\r").find(text.front()) != std::string_view::npos) {
			// Plain negative numbers are data, not formulas
			const bool number = text.size() > 1 && text.front() == '-' && text.find_first_not_of("0123456789.", 1) == std::string::npos;
			if (!number) text.insert(text.begin(), '\'');
		}
		if (text.find_first_of(",\"\r\n") == std::string::npos) return text;
		std::string quoted = "\"";
		for (const char c : text) {
			if (c == '"') quoted += '"';
			quoted += c;
		}
		return quoted + "\"";
	}

	// Rows (JSON objects) as CSV with the given columns: {json field, header}
	inline std::string ToCsv(const nlohmann::json& rows, const std::vector<std::pair<std::string, std::string>>& columns) {
		std::string csv;
		for (size_t i = 0; i < columns.size(); i++) csv += (i ? "," : "") + CsvCell(columns[i].second);
		csv += "\r\n";
		for (const auto& row : rows) {
			for (size_t i = 0; i < columns.size(); i++) {
				if (i) csv += ',';
				if (!row.is_object() || !row.contains(columns[i].first)) continue;
				const auto& value = row[columns[i].first];
				if (value.is_string()) csv += CsvCell(value.get<std::string>());
				else if (value.is_boolean()) csv += value.get<bool>() ? "true" : "false";
				else if (!value.is_null()) csv += CsvCell(value.dump());
			}
			csv += "\r\n";
		}
		return csv;
	}

	// Escape a string for insertion into HTML text or a quoted attribute
	inline std::string EscapeHtml(std::string_view input) {
		std::string out;
		out.reserve(input.size());
		for (const char c : input) {
			switch (c) {
			case '&': out += "&amp;"; break;
			case '<': out += "&lt;"; break;
			case '>': out += "&gt;"; break;
			case '"': out += "&quot;"; break;
			case '\'': out += "&#39;"; break;
			default: out += c; break;
			}
		}
		return out;
	}

	// Recursively escape every string in a JSON value. inja does not escape output,
	// so all template data passes through this before rendering.
	inline void EscapeHtmlStrings(nlohmann::json& value) {
		if (value.is_string()) {
			value = EscapeHtml(value.get_ref<const std::string&>());
		} else if (value.is_structured()) {
			for (auto& child : value) EscapeHtmlStrings(child);
		}
	}

	// The self and rank rules, shared with the in-game slash commands (AccountRules.h)
	using AccountRules::OPERATOR_LEVEL;
	using AccountRules::eAccountAction;
	using AccountRules::SelfPermission;
	using AccountRules::eManageDenial;
	using AccountRules::ManageDenial;
	using AccountRules::CanManageAccount;
	using AccountRules::RemovesLastOperator;
	using AccountRules::CanGrantGmLevel;

	constexpr size_t MIN_PASSWORD_LENGTH = 6;
	constexpr size_t MAX_PASSWORD_LENGTH = 40; // The game client's limit

	// Error message if the password is unacceptable
	inline std::optional<std::string> ValidatePassword(const std::string& password) {
		if (password.size() < MIN_PASSWORD_LENGTH) return "Password must be at least " + std::to_string(MIN_PASSWORD_LENGTH) + " characters";
		if (password.size() > MAX_PASSWORD_LENGTH) return "Password must be at most " + std::to_string(MAX_PASSWORD_LENGTH) + " characters";
		return std::nullopt;
	}

	// Error message if the username is unacceptable for a new account
	inline std::optional<std::string> ValidateUsername(const std::string& username) {
		if (username.size() < 3 || username.size() > 32) return "Username must be 3-32 characters";
		const bool allowed = std::ranges::all_of(username, [](char c) {
			return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
		});
		if (!allowed) return "Username may only contain letters, numbers, '.', '_' and '-'";
		return std::nullopt;
	}

	/**
	 * Sliding-window limiter keyed by client (e.g. IP). Allows maxEvents per window.
	 */
	class RateLimiter {
	public:
		using Clock = std::chrono::steady_clock;

		RateLimiter(size_t maxEvents, std::chrono::seconds window) : m_MaxEvents(maxEvents), m_Window(window) {}

		// Records an attempt and returns whether it is within the limit
		bool Allow(const std::string& key, Clock::time_point now = Clock::now()) {
			auto& events = m_Events[key];
			while (!events.empty() && now - events.front() >= m_Window) events.pop_front();
			if (events.size() >= m_MaxEvents) return false;
			events.push_back(now);
			// Keep memory bounded when many distinct clients show up
			if (m_Events.size() > 10000) Prune(now);
			return true;
		}

	private:
		void Prune(Clock::time_point now) {
			std::erase_if(m_Events, [&](const auto& entry) { return entry.second.empty() || now - entry.second.back() >= m_Window; });
		}

		size_t m_MaxEvents;
		std::chrono::seconds m_Window;
		std::map<std::string, std::deque<Clock::time_point>> m_Events;
	};

	// ---- Helpers below need the web server, database, or config (RouteUtils.cpp) ----

	std::string HashPassword(const std::string& password);

	// Random URL-safe token for emailed links, and the hash stored in the database in its place
	std::string GenerateUrlToken();
	std::string HashToken(const std::string& token);

	// Client address for rate limiting. With behind_proxy=1 the first X-Forwarded-For entry is used,
	// since every request then comes from the proxy itself.
	std::string ClientAddress(const HTTPContext& context);

	// URL-decoded value of a query string parameter ("a=1&b=2"), or empty if absent
	std::string QueryValue(const std::string& query, const std::string& name);

	// Boolean config value of the dashboard config ("1" is true)
	bool ConfigFlag(const std::string& key, bool fallback);

	using Handler = std::function<void(HTTPReply&, const HTTPContext&)>;

	// Minimum level meaning the route needs no authentication at all
	constexpr int16_t PUBLIC = -1;

	struct RouteDoc {
		std::string method;
		std::string path;
		int16_t minGmLevel;
		std::string description;
		std::string permission; // set for routes guarded by a permission; minGmLevel is then its level at the time
	};

	// A named permission (Permissions.h) guarding a route; its GM level can be changed while the server runs
	struct Perm {
		std::string key;
		explicit Perm(const char* key) : key(key) {}
	};

	/**
	 * Register a route with authentication, documentation and error handling.
	 * Exceptions escaping the handler become a 500 (JSON for /api paths, an error page otherwise).
	 * @param minGmLevel Required GM level, or PUBLIC for no authentication
	 */
	void Route(eHTTPMethod method, const std::string& path, int16_t minGmLevel, const std::string& description, Handler handler);
	void Route(eHTTPMethod method, const std::string& path, const Perm& permission, const std::string& description, Handler handler);

	// Whether the signed-in user has a permission
	bool Can(const HTTPContext& context, const std::string& permission);

	// A character typed by a person: a number is its ID, anything else its name. nullopt: no such character.
	std::optional<LWOOBJID> ResolveCharacter(std::string_view text);

	// Their own characters, or anyone's with characters_view
	bool CanViewCharacter(const HTTPContext& context, uint32_t ownerAccountId);

	// Every route registered through Route, for the API documentation endpoint
	const std::vector<RouteDoc>& GetRouteDocs();

	void JsonReply(HTTPReply& reply, eHTTPStatusCode status, const nlohmann::json& body);
	void JsonError(HTTPReply& reply, eHTTPStatusCode status, const std::string& message);
	void JsonSuccess(HTTPReply& reply, nlohmann::json extra = nlohmann::json::object());

	// Send CSV as a download named `filename` (letters, digits, - _ . only)
	void CsvReply(HTTPReply& reply, std::string filename, const std::string& csv);

	// Parse the request body as JSON. An empty body parses as an empty object.
	std::optional<nlohmann::json> ParseBody(const HTTPContext& context);

	// What an audit log entry is about, so an account's page can list the staff actions about it
	struct AuditTarget {
		uint32_t accountId{};
		LWOOBJID characterId{};

		static AuditTarget Account(uint32_t accountId) { return { accountId, 0 }; }
		// Looks up the character's account
		static AuditTarget Character(LWOOBJID characterId);
	};

	// Context for changes the dashboard makes by itself (scheduled jobs, timers): audited as "[system]", account 0
	HTTPContext SystemContext();

	// Write an entry to the audit log for the acting user
	void Audit(const HTTPContext& context, const std::string& action, const std::string& description, const AuditTarget& target = {});

	// Whether session cookies should carry the Secure attribute (config: secure_cookies=1)
	bool UseSecureCookies();

	// Whether the signed-in user may do this kind of action on the account (self_*, manage_equal_rank; see ManageDenial)
	bool CanManageAccount(const HTTPContext& context, uint8_t targetLevel, uint32_t targetAccountId, eAccountAction action);

	// {tools, items, moderation}: which kinds of action the signed-in user may take on the account, for templates
	nlohmann::json ManageJson(const HTTPContext& context, uint8_t targetLevel, uint32_t targetAccountId);

	/**
	 * Check the actor may do this kind of action on the target account, writing a 403/404 reply if not.
	 * @return The target's GM level if allowed
	 */
	std::optional<uint8_t> AuthorizeAccountAction(const HTTPContext& context, uint32_t targetAccountId, HTTPReply& reply, eAccountAction action);

	/**
	 * The last-operator safety rail (RemovesLastOperator) for an action that takes a GM 9 account out of use:
	 * writes a 409 reply and returns true if it must be refused.
	 */
	bool RefuseLastOperator(uint32_t targetAccountId, uint8_t targetLevel, HTTPReply& reply, const std::string& what);

	// " (on their own account)" when the actor acts on their own account, for audit descriptions; otherwise empty
	std::string OwnAccountNote(uint32_t actorAccountId, uint32_t targetAccountId);

	/**
	 * Render a dashboard page. All string data is HTML-escaped before rendering.
	 * @param page The navigation key used to highlight the sidebar entry
	 */
	void RenderPage(HTTPReply& reply, const HTTPContext& context, const std::string& templateName, const std::string& page, nlohmann::json data = nlohmann::json::object());

	// Render a simple error page
	void RenderError(HTTPReply& reply, const HTTPContext& context, eHTTPStatusCode status, const std::string& message);
}
