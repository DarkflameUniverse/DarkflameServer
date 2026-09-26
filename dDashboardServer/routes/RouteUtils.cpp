#include "RouteUtils.h"
#include "Permissions.h"

#include "Database.h"
#include "GameLabels.h"
#include "Game.h"
#include "Logger.h"
#include "dConfig.h"
#include "inja.hpp"
#include <bcrypt/BCrypt.hpp>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include "eHTTPMethod.h"
#include "RequireAuthMiddleware.h"
#include "magic_enum.hpp"
#include "Alerts.h"
#include "mongoose.h"

namespace {
	constexpr const char* TEMPLATE_DIR = "dDashboardServer/templates/";

	inja::Environment& GetEnvironment() {
		static inja::Environment env = [] {
			// No trim_blocks/lstrip_blocks: inja's versions also eat the spaces around a tag on the same line
			// ('class="a{% if x %} b{% endif %}"' would render "ab"), unlike Jinja2's
			return inja::Environment{ TEMPLATE_DIR };
		}();
		return env;
	}
}

namespace RouteUtils {
	namespace {
		std::vector<RouteDoc> g_RouteDocs;
	}

	void Register(eHTTPMethod method, const std::string& path, std::vector<MiddlewarePtr> middleware, Handler handler) {
		Game::web.RegisterHTTPRoute({
			.path = path,
			.method = method,
			.middleware = std::move(middleware),
			.handle = [path, handler = std::move(handler)](HTTPReply& reply, const HTTPContext& context) {
				try {
					handler(reply, context);
				} catch (const std::exception& ex) {
					LOG("Error handling %s %s: %s", context.method.c_str(), context.path.c_str(), ex.what());
					if (path.starts_with("/api/")) JsonError(reply, eHTTPStatusCode::INTERNAL_SERVER_ERROR, "Internal server error");
					else RenderError(reply, context, eHTTPStatusCode::INTERNAL_SERVER_ERROR, "Something went wrong loading this page.");
				}
			}
		});
	}

	void Route(eHTTPMethod method, const std::string& path, int16_t minGmLevel, const std::string& description, Handler handler) {
		std::vector<MiddlewarePtr> middleware;
		if (minGmLevel >= 0) middleware.push_back(std::make_shared<RequireAuthMiddleware>(static_cast<uint8_t>(minGmLevel)));
		g_RouteDocs.push_back({ std::string(magic_enum::enum_name(method)), path, minGmLevel, description, "" });
		Register(method, path, std::move(middleware), std::move(handler));
	}

	void Route(eHTTPMethod method, const std::string& path, const Perm& permission, const std::string& description, Handler handler) {
		if (!Permissions::Find(permission.key)) LOG("Route %s uses unknown permission %s; nobody can use it", path.c_str(), permission.key.c_str());
		std::vector<MiddlewarePtr> middleware;
		middleware.push_back(std::make_shared<RequireAuthMiddleware>(std::function<uint8_t()>([key = permission.key] { return Permissions::Level(key); })));
		g_RouteDocs.push_back({ std::string(magic_enum::enum_name(method)), path, Permissions::Level(permission.key), description, permission.key });
		Register(method, path, std::move(middleware), std::move(handler));
	}

	bool Can(const HTTPContext& context, const std::string& permission) {
		return context.isAuthenticated && Permissions::Allowed(context.gmLevel, permission);
	}

	std::optional<LWOOBJID> ResolveCharacter(std::string_view text) {
		std::string trimmed(text);
		trimmed.erase(0, trimmed.find_first_not_of(" \t"));
		trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
		if (trimmed.empty()) return std::nullopt;
		if (const auto id = GeneralUtils::TryParse<LWOOBJID>(trimmed)) return id;
		const auto info = Database::Get()->GetCharacterInfo(trimmed);
		return info ? std::optional<LWOOBJID>(info->id) : std::nullopt;
	}

	bool CanViewCharacter(const HTTPContext& context, uint32_t ownerAccountId) {
		return context.isAuthenticated && Permissions::CanViewCharacter(context.gmLevel, context.accountId, ownerAccountId);
	}

	const std::vector<RouteDoc>& GetRouteDocs() {
		return g_RouteDocs;
	}

	void JsonReply(HTTPReply& reply, eHTTPStatusCode status, const nlohmann::json& body) {
		reply.status = status;
		reply.message = body.dump();
		reply.contentType = eContentType::APPLICATION_JSON;
	}

	void JsonError(HTTPReply& reply, eHTTPStatusCode status, const std::string& message) {
		JsonReply(reply, status, { {"success", false}, {"error", message} });
	}

	void JsonSuccess(HTTPReply& reply, nlohmann::json extra) {
		extra["success"] = true;
		JsonReply(reply, eHTTPStatusCode::OK, extra);
	}

	std::optional<nlohmann::json> ParseBody(const HTTPContext& context) {
		if (context.body.empty()) return nlohmann::json::object();
		auto json = nlohmann::json::parse(context.body, nullptr, false);
		if (json.is_discarded() || !json.is_object()) return std::nullopt;
		return json;
	}

	AuditTarget AuditTarget::Character(LWOOBJID characterId) {
		const auto info = Database::Get()->GetCharacterInfo(characterId);
		return { info ? info->accountId : 0, characterId };
	}

	HTTPContext SystemContext() {
		HTTPContext context;
		context.isAuthenticated = true;
		context.authenticatedUser = "[system]";
		return context;
	}

	void Audit(const HTTPContext& context, const std::string& action, const std::string& description, const AuditTarget& target) {
		// Staff acting on their own account or characters is called out, so it stands out in the log and in alerts
		const auto text = description + OwnAccountNote(context.accountId, target.accountId);
		try {
			Database::Get()->InsertAuditLog(context.accountId, context.authenticatedUser, action, text, target.accountId, target.characterId);
		} catch (const std::exception& ex) {
			LOG("Failed to write audit log entry %s: %s", action.c_str(), ex.what());
		}
		LOG("[audit] %s: %s %s", context.authenticatedUser.c_str(), action.c_str(), text.c_str());
		Alerts::FromAudit(context.authenticatedUser, action, text);
	}

	std::string HashPassword(const std::string& password) {
		char salt[BCRYPT_HASHSIZE];
		char hash[BCRYPT_HASHSIZE];
		bcrypt_gensalt(12, salt);
		bcrypt_hashpw(password.c_str(), salt, hash);
		return hash;
	}

	std::string ClientAddress(const HTTPContext& context) {
		if (ConfigFlag("behind_proxy", false)) {
			// Use the last address: the one our proxy appended. Earlier ones come from the client and can be forged,
			// which would let anyone dodge the per-address rate limits.
			const auto& forwarded = context.GetHeader("X-Forwarded-For");
			if (!forwarded.empty()) {
				const auto comma = forwarded.rfind(',');
				auto last = comma == std::string::npos ? forwarded : forwarded.substr(comma + 1);
				last.erase(0, last.find_first_not_of(' '));
				last.erase(last.find_last_not_of(' ') + 1);
				if (!last.empty()) return last;
			}
		}
		return context.clientIP;
	}

	namespace {
		std::string Hex(const unsigned char* data, size_t size) {
			static constexpr char digits[] = "0123456789abcdef";
			std::string out;
			out.reserve(size * 2);
			for (size_t i = 0; i < size; i++) {
				out += digits[data[i] >> 4];
				out += digits[data[i] & 0xf];
			}
			return out;
		}
	}

	std::string GenerateUrlToken() {
		unsigned char bytes[32];
		if (RAND_bytes(bytes, sizeof(bytes)) != 1) throw std::runtime_error("RAND_bytes failed");
		return Hex(bytes, sizeof(bytes));
	}

	std::string HashToken(const std::string& token) {
		unsigned char digest[SHA256_DIGEST_LENGTH];
		SHA256(reinterpret_cast<const unsigned char*>(token.data()), token.size(), digest);
		return Hex(digest, sizeof(digest));
	}

	void CsvReply(HTTPReply& reply, std::string filename, const std::string& csv) {
		std::erase_if(filename, [](char c) { return !(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.'); });
		if (filename.empty()) filename = "export.csv";
		reply.status = eHTTPStatusCode::OK;
		reply.contentType = eContentType::TEXT_CSV;
		// A byte order mark so Excel reads UTF-8 names correctly
		reply.message = "\xEF\xBB\xBF" + csv;
		reply.headers.push_back("Content-Disposition: attachment; filename=\"" + filename + "\"");
	}

	std::string QueryValue(const std::string& query, const std::string& name) {
		const auto key = name + "=";
		size_t pos = 0;
		while ((pos = query.find(key, pos)) != std::string::npos) {
			if (pos == 0 || query[pos - 1] == '&' || query[pos - 1] == '?') {
				const auto end = query.find('&', pos);
				std::string raw = query.substr(pos + key.size(), end == std::string::npos ? std::string::npos : end - pos - key.size());
				std::replace(raw.begin(), raw.end(), '+', ' ');
				std::string decoded(raw.size() + 1, '\0');
				const int length = mg_url_decode(raw.c_str(), raw.size(), decoded.data(), decoded.size(), 1);
				decoded.resize(length > 0 ? static_cast<size_t>(length) : 0);
				return decoded;
			}
			pos += key.size();
		}
		return "";
	}

	bool ConfigFlag(const std::string& key, bool fallback) {
		if (!Game::config) return fallback;
		const auto value = Game::config->GetValue(key);
		return value.empty() ? fallback : value == "1";
	}

	bool UseSecureCookies() {
		return Game::config && Game::config->GetValue("secure_cookies") == "1";
	}

	bool CanManageAccount(const HTTPContext& context, uint8_t targetLevel, uint32_t targetAccountId, eAccountAction action) {
		return context.isAuthenticated && AccountRules::ManageDenialNow(context.gmLevel, context.accountId, targetLevel, targetAccountId, action) == eManageDenial::NONE;
	}

	nlohmann::json ManageJson(const HTTPContext& context, uint8_t targetLevel, uint32_t targetAccountId) {
		return {
			{"tools", CanManageAccount(context, targetLevel, targetAccountId, eAccountAction::TOOLS)},
			{"items", CanManageAccount(context, targetLevel, targetAccountId, eAccountAction::ITEMS)},
			{"moderation", CanManageAccount(context, targetLevel, targetAccountId, eAccountAction::MODERATION)},
		};
	}

	std::optional<uint8_t> AuthorizeAccountAction(const HTTPContext& context, uint32_t targetAccountId, HTTPReply& reply, eAccountAction action) {
		const auto target = Database::Get()->GetAccountById(targetAccountId);
		if (target.contains("error")) {
			JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Account not found");
			return std::nullopt;
		}
		const uint8_t targetLevel = target.value("gm_level", 0);
		const auto denial = AccountRules::ManageDenialNow(context.gmLevel, context.accountId, targetLevel, targetAccountId, action);
		if (denial == eManageDenial::NONE) return targetLevel;
		JsonError(reply, eHTTPStatusCode::FORBIDDEN, AccountRules::DenialMessage(denial, action));
		return std::nullopt;
	}

	bool RefuseLastOperator(uint32_t targetAccountId, uint8_t targetLevel, HTTPReply& reply, const std::string& what) {
		if (targetLevel < OPERATOR_LEVEL) return false;
		if (!RemovesLastOperator(targetLevel, Database::Get()->CountActiveAccountsAtGmLevel(OPERATOR_LEVEL, targetAccountId))) return false;
		JsonError(reply, eHTTPStatusCode::CONFLICT, AccountRules::LastOperatorMessage(what));
		return true;
	}

	std::string OwnAccountNote(uint32_t actorAccountId, uint32_t targetAccountId) {
		return actorAccountId != 0 && actorAccountId == targetAccountId ? " (on their own account)" : "";
	}

	void RenderPage(HTTPReply& reply, const HTTPContext& context, const std::string& templateName, const std::string& page, nlohmann::json data) {
		try {
			data.merge_patch(context.GetUserDataJson());
			data["current_page"] = page;
			data["can"] = Permissions::ForLevel(context.isAuthenticated ? context.gmLevel : 0);
			// The account's view choices, on <body> so each page's toggles start as they were left (static/js/common.js)
			// Names for the game's numbered values, from the server's enums (GameLabels.h)
			data["labels"] = GameLabels::Json();
			data["labelsJson"] = GameLabels::Json().dump();
			data["prefs"] = context.isAuthenticated && context.accountId ? Database::Get()->GetDashboardPreferences(context.accountId) : "{}";
			EscapeHtmlStrings(data);

			reply.status = eHTTPStatusCode::OK;
			reply.message = GetEnvironment().render_file(templateName, data);
			reply.contentType = eContentType::TEXT_HTML;
		} catch (const std::exception& ex) {
			LOG("Error rendering template %s: %s", templateName.c_str(), ex.what());
			reply.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
			reply.message = "<h1>500 - Server Error</h1>";
			reply.contentType = eContentType::TEXT_HTML;
		}
	}

	void RenderError(HTTPReply& reply, const HTTPContext& context, eHTTPStatusCode status, const std::string& message) {
		RenderPage(reply, context, "error.jinja2", "", { {"status_code", static_cast<int>(status)}, {"error_message", message} });
		if (reply.status == eHTTPStatusCode::OK) reply.status = status;
	}
}
