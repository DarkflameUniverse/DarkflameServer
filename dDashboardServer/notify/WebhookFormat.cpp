#include "WebhookFormat.h"

#include <algorithm>
#include <ctime>

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include "GeneralUtils.h"

namespace {
	// Discord embed limits; longer text is cut
	constexpr size_t MAX_TITLE = 256;
	constexpr size_t MAX_DESCRIPTION = 4000;
	constexpr size_t MAX_FIELD_NAME = 256;
	constexpr size_t MAX_FIELD_VALUE = 1000;
	constexpr size_t MAX_FIELDS = 25;

	std::string Cut(const std::string& text, size_t max) {
		if (text.size() <= max) return text;
		// Don't cut through a UTF-8 sequence
		size_t end = max - 3;
		while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) end--;
		return text.substr(0, end) + "...";
	}

	// Colors by event so a channel reads at a glance
	int Color(const std::string& event) {
		if (event == "security" || event == "economy_flag") return 0xD03B3B;
		if (event == "moderation") return 0xEC835A;
		if (event == "server") return 0xFAB219;
		if (event == "bug_report") return 0x3987E5;
		return 0x0CA30C;
	}

	std::string IsoTime(int64_t time) {
		const auto t = static_cast<std::time_t>(time);
		std::tm tm{};
#ifdef _WIN32
		gmtime_s(&tm, &t);
#else
		gmtime_r(&t, &tm);
#endif
		char buffer[32];
		std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
		return buffer;
	}
}

namespace WebhookFormat {
	const std::vector<EventInfo>& Events() {
		static const std::vector<EventInfo> events{
			{ "bug_report", "A player submitted a bug report" },
			{ "pending_name", "A character name is waiting for approval" },
			{ "moderation", "Bans, locks, mutes, kicks and restrictions, from the dashboard or in game" },
			{ "security", "GM level changes, two-factor login changes, API tokens, recovery code use" },
			{ "economy_flag", "The nightly checks flagged unusual income, an item spike or a duplicated item" },
			{ "server", "Auth, chat or the UGC server went offline or came back, restarts scheduled or cancelled" },
			{ "test", "Test messages sent from the webhooks page" },
		};
		return events;
	}

	bool IsKnownEvent(const std::string& event) {
		return std::ranges::any_of(Events(), [&](const EventInfo& info) { return event == info.name; });
	}

	bool Matches(const std::string& events, const std::string& event) {
		if (event == "test") return true;
		for (auto name : GeneralUtils::SplitString(events, ',')) {
			name.erase(0, name.find_first_not_of(' '));
			name.erase(name.find_last_not_of(' ') + 1);
			if (name == "*" || name == event) return true;
		}
		return false;
	}

	std::string NormalizeEvents(const std::string& events) {
		std::string out;
		for (auto name : GeneralUtils::SplitString(events, ',')) {
			name.erase(0, name.find_first_not_of(' '));
			name.erase(name.find_last_not_of(' ') + 1);
			if (name == "*") return "*";
			if (!IsKnownEvent(name) || name == "test") continue;
			if (!out.empty()) out += ',';
			out += name;
		}
		return out;
	}

	bool IsKnownFormat(const std::string& format) {
		return format == "discord" || format == "slack" || format == "json";
	}

	std::string BuildBody(const std::string& format, const Alert& alert) {
		if (format == "discord") {
			nlohmann::json fields = nlohmann::json::array();
			for (const auto& field : alert.fields) {
				if (fields.size() >= MAX_FIELDS) break;
				fields.push_back({ {"name", Cut(field.name, MAX_FIELD_NAME)}, {"value", Cut(field.value.empty() ? "-" : field.value, MAX_FIELD_VALUE)}, {"inline", field.value.size() < 40} });
			}
			nlohmann::json embed{
				{"title", Cut(alert.title, MAX_TITLE)}, {"description", Cut(alert.description, MAX_DESCRIPTION)},
				{"color", Color(alert.event)}, {"fields", fields}, {"timestamp", IsoTime(alert.time)},
				{"footer", { {"text", "DarkflameServer · " + alert.event} }}
			};
			if (!alert.url.empty()) embed["url"] = alert.url;
			// No mentions from player-written text (bug reports, names)
			return nlohmann::json{ {"embeds", nlohmann::json::array({ embed })}, {"allowed_mentions", { {"parse", nlohmann::json::array()} }} }.dump();
		}
		if (format == "slack") {
			std::string text = "*" + Cut(alert.title, MAX_TITLE) + "*";
			if (!alert.description.empty()) text += "\n" + Cut(alert.description, MAX_DESCRIPTION);
			for (const auto& field : alert.fields) text += "\n• *" + field.name + ":* " + Cut(field.value, MAX_FIELD_VALUE);
			if (!alert.url.empty()) text += "\n<" + alert.url + "|Open in dashboard>";
			return nlohmann::json{ {"text", text} }.dump();
		}
		nlohmann::json fields = nlohmann::json::object();
		for (const auto& field : alert.fields) fields[field.name] = field.value;
		return nlohmann::json{
			{"event", alert.event}, {"title", alert.title}, {"description", alert.description},
			{"fields", fields}, {"url", alert.url}, {"time", alert.time}
		}.dump();
	}

	std::string Sign(const std::string& secret, const std::string& body) {
		unsigned char digest[EVP_MAX_MD_SIZE];
		unsigned int length = 0;
		HMAC(EVP_sha256(), secret.data(), static_cast<int>(secret.size()), reinterpret_cast<const unsigned char*>(body.data()), body.size(), digest, &length);
		static constexpr char digits[] = "0123456789abcdef";
		std::string out;
		for (unsigned int i = 0; i < length; i++) { out += digits[digest[i] >> 4]; out += digits[digest[i] & 15]; }
		return out;
	}

	std::optional<std::string> ValidateUrl(const std::string& url) {
		if (url.size() > 2000) return "That URL is too long";
		const bool https = url.starts_with("https://"), http = url.starts_with("http://");
		if (!https && !http) return "Use an http:// or https:// URL";
		const auto hostStart = url.find("://") + 3;
		const auto hostEnd = std::min(url.find_first_of("/?#", hostStart), url.size());
		const auto authority = url.substr(hostStart, hostEnd - hostStart);
		if (authority.empty()) return "The URL has no host";
		if (authority.find('@') != std::string::npos) return "Put credentials in the secret, not the URL";
		if (std::ranges::any_of(url, [](char c) { return static_cast<unsigned char>(c) <= ' ' || c == '"' || c == '\\'; })) return "The URL contains spaces or control characters";
		return std::nullopt;
	}

	std::string MaskUrl(const std::string& url) {
		const auto schemeEnd = url.find("://");
		if (schemeEnd == std::string::npos) return "…";
		// Any part of the path can be the secret (Discord and Slack put tokens there), so only the host is shown
		const auto pathStart = url.find_first_of("/?#", schemeEnd + 3);
		if (pathStart == std::string::npos) return url;
		const bool hasPath = url.size() > pathStart + 1;
		return url.substr(0, pathStart) + (hasPath ? "/…" : "");
	}
}
