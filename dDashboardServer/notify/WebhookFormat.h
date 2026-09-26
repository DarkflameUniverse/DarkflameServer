#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "json.hpp"

/**
 * Pure parts of outgoing webhooks: which events a webhook wants, the request body for each supported format and
 * the signature for generic JSON receivers. No network or database, so they can be unit tested.
 */
namespace WebhookFormat {
	struct Field {
		std::string name;
		std::string value;
	};

	struct Alert {
		std::string event;        // e.g. "bug_report"
		std::string title;
		std::string description;
		std::vector<Field> fields;
		std::string url;          // absolute link into the dashboard, may be empty
		int64_t time{};           // unix seconds
	};

	struct EventInfo {
		const char* name;
		const char* description;
	};

	// Every event a webhook can subscribe to
	const std::vector<EventInfo>& Events();
	bool IsKnownEvent(const std::string& event);

	// events is "*" or a comma separated list
	bool Matches(const std::string& events, const std::string& event);

	// Keep only known event names ("*" stays as is); returns the cleaned list
	std::string NormalizeEvents(const std::string& events);

	bool IsKnownFormat(const std::string& format);

	// Request body for "discord", "slack" or "json"
	std::string BuildBody(const std::string& format, const Alert& alert);

	// Hex HMAC-SHA256 of the body, sent as X-DLU-Signature: sha256=<hex> when the webhook has a secret
	std::string Sign(const std::string& secret, const std::string& body);

	// http(s) URL without credentials; returns an error message or nothing when fine
	std::optional<std::string> ValidateUrl(const std::string& url);

	// "https://discord.com/…": the host only, since tokens live in the path
	std::string MaskUrl(const std::string& url);
}
