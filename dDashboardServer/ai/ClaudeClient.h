#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "json.hpp"

/**
 * A small client for the Anthropic Messages API (POST {base}/v1/messages), used only by the dashboard's staff-side
 * AI moderator helper. Nothing it returns is ever shown to players or sent into the game.
 *
 * Send() blocks (HTTPS, with retries), so it must run on a worker thread. The API key is only put in the request
 * header: it is never logged and never part of an error message.
 */
namespace ClaudeClient {
	constexpr const char* API_VERSION = "2023-06-01";
	constexpr uint32_t MAX_ATTEMPTS = 4; // the first try and up to three retries

	struct Config {
		std::string apiKey;
		std::string base{ "https://api.anthropic.com" };
		std::string model{ "claude-sonnet-5" };
		uint32_t timeoutSeconds{ 60 };
		uint32_t maxTokens{ 2000 };
		// Ask the API to hold the answer to a JSON schema (output_config.format); the answer is still validated here
		bool structuredOutput{ true };
	};

	struct Request {
		std::string system;
		std::string user;
		nlohmann::json schema; // null: no output_config.format
	};

	enum class eError : uint8_t {
		NONE,
		DISABLED,     // the helper is switched off
		NO_KEY,       // no API key set
		BAD_CONFIG,   // e.g. an http:// base that isn't this machine
		BUDGET,       // a request limit was reached
		API,          // the API refused the request (4xx)
		UNAVAILABLE,  // the API kept failing (429/5xx/network) after retries
		BAD_RESPONSE, // the API answered with something unreadable, cut off or refused
	};

	struct Result {
		bool ok{};
		eError error{ eError::NONE };
		std::string message;      // what went wrong, safe to show staff (never contains the key)
		std::string text;         // the model's text blocks, joined
		std::string model;        // the model that answered
		std::string stopReason;
		uint32_t inputTokens{};
		uint32_t outputTokens{};
		uint32_t attempts{};
		long status{};            // last HTTP status (0: none)
	};

	struct HttpResponse {
		long status{};            // 0: the request never got an answer (see error)
		std::string body;
		std::string retryAfter;   // the retry-after header, if any
		std::string error;        // transport error
	};

	// One HTTP POST. Replaced in tests; the default uses libcurl.
	using Transport = std::function<HttpResponse(const std::string& url, const std::vector<std::string>& headers, const std::string& body, uint32_t timeoutSeconds)>;
	using Sleep = std::function<void(uint32_t milliseconds)>;

	// ---- Pure helpers, exposed for testing ----

	// The messages endpoint for a base URL, or nullopt with `error` when the base may not be used: https:// anywhere,
	// http:// only for this machine (a local test server), so the key never travels unencrypted.
	std::optional<std::string> Endpoint(const std::string& base, std::string& error);

	nlohmann::json BuildBody(const Config& config, const Request& request);
	std::vector<std::string> BuildHeaders(const Config& config);

	// Whether a status is worth retrying: 408, 409, 429 and 5xx, and no answer at all (0)
	bool Retryable(long status);

	// How long to wait before retry number `retry` (0-based): the server's retry-after (capped at 30 s) when it gave
	// one, else 1 s, 2 s, 4 s... capped at 16 s
	uint32_t RetryDelayMs(uint32_t retry, const std::string& retryAfter);

	// Read a Messages API response body (HTTP 200)
	Result ParseResponse(const std::string& body);

	// Staff-readable message for an error response (4xx/5xx): the API's own error type and message
	std::string DescribeError(long status, const std::string& body);

	// ---- Sending ----

	HttpResponse CurlTransport(const std::string& url, const std::vector<std::string>& headers, const std::string& body, uint32_t timeoutSeconds);

	// Blocking: send with retries on 429/5xx/network errors
	Result Send(const Config& config, const Request& request, const Transport& transport = CurlTransport, const Sleep& sleep = {});
}
