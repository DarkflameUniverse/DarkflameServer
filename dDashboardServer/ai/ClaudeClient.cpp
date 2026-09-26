#include "ClaudeClient.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <thread>

#include <curl/curl.h>

namespace {
	size_t WriteBody(char* data, size_t size, size_t count, void* userData) {
		static_cast<std::string*>(userData)->append(data, size * count);
		return size * count;
	}

	// Keeps the retry-after header
	size_t WriteHeader(char* data, size_t size, size_t count, void* userData) {
		const std::string line(data, size * count);
		const auto colon = line.find(':');
		if (colon != std::string::npos) {
			std::string name = line.substr(0, colon);
			std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (name == "retry-after") {
				std::string value = line.substr(colon + 1);
				value.erase(0, value.find_first_not_of(" \t"));
				value.erase(value.find_last_not_of(" \t\r\n") + 1);
				*static_cast<std::string*>(userData) = value;
			}
		}
		return size * count;
	}

	std::string Lower(std::string text) {
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return text;
	}

	ClaudeClient::Result Failure(ClaudeClient::eError error, std::string message, long status = 0) {
		ClaudeClient::Result result;
		result.error = error;
		result.message = std::move(message);
		result.status = status;
		return result;
	}
}

namespace ClaudeClient {
	std::optional<std::string> Endpoint(const std::string& base, std::string& error) {
		std::string url = base;
		while (!url.empty() && (url.back() == '/' || url.back() == ' ')) url.pop_back();
		url.erase(0, url.find_first_not_of(' '));
		const auto lower = Lower(url);
		if (lower.starts_with("https://") && lower.size() > 8) return url + "/v1/messages";
		if (lower.starts_with("http://")) {
			// Host part: up to the next '/', without a port
			std::string host = lower.substr(7, lower.find('/', 7) == std::string::npos ? std::string::npos : lower.find('/', 7) - 7);
			if (host.starts_with("[::1]")) host = "[::1]";
			else host = host.substr(0, host.find(':'));
			if (host == "localhost" || host == "127.0.0.1" || host == "[::1]") return url + "/v1/messages";
			error = "claude_api_base must be https:// (plain http:// only for a test server on this machine)";
			return std::nullopt;
		}
		error = "claude_api_base must be an https:// address, like https://api.anthropic.com";
		return std::nullopt;
	}

	nlohmann::json BuildBody(const Config& config, const Request& request) {
		nlohmann::json body{
			{"model", config.model},
			{"max_tokens", config.maxTokens},
			{"system", request.system},
			{"messages", nlohmann::json::array({ { {"role", "user"}, {"content", request.user} } })}
		};
		if (config.structuredOutput && !request.schema.is_null()) {
			body["output_config"] = { {"format", { {"type", "json_schema"}, {"schema", request.schema} }} };
		}
		return body;
	}

	std::vector<std::string> BuildHeaders(const Config& config) {
		return {
			"x-api-key: " + config.apiKey,
			std::string("anthropic-version: ") + API_VERSION,
			"content-type: application/json",
			"accept: application/json"
		};
	}

	bool Retryable(long status) {
		return status == 0 || status == 408 || status == 409 || status == 429 || status >= 500;
	}

	uint32_t RetryDelayMs(uint32_t retry, const std::string& retryAfter) {
		constexpr uint32_t MAX_RETRY_AFTER_MS = 30000;
		constexpr uint32_t MAX_BACKOFF_MS = 16000;
		if (!retryAfter.empty() && std::all_of(retryAfter.begin(), retryAfter.end(), [](unsigned char c) { return std::isdigit(c); }) && retryAfter.size() < 9) {
			return std::min<uint32_t>(static_cast<uint32_t>(std::stoul(retryAfter)) * 1000, MAX_RETRY_AFTER_MS);
		}
		return std::min<uint32_t>(1000u << std::min<uint32_t>(retry, 4), MAX_BACKOFF_MS);
	}

	Result ParseResponse(const std::string& body) {
		const auto json = nlohmann::json::parse(body, nullptr, false);
		if (json.is_discarded() || !json.is_object()) return Failure(eError::BAD_RESPONSE, "The API answered with something that isn't JSON");
		if (json.value("type", "") == "error") return Failure(eError::API, DescribeError(200, body));
		if (!json.contains("content") || !json["content"].is_array()) return Failure(eError::BAD_RESPONSE, "The API's answer has no content");

		Result result;
		result.model = json.value("model", "");
		result.stopReason = json.contains("stop_reason") && json["stop_reason"].is_string() ? json["stop_reason"].get<std::string>() : "";
		if (json.contains("usage") && json["usage"].is_object()) {
			const auto& usage = json["usage"];
			if (usage.contains("input_tokens") && usage["input_tokens"].is_number_unsigned()) result.inputTokens = usage["input_tokens"].get<uint32_t>();
			if (usage.contains("output_tokens") && usage["output_tokens"].is_number_unsigned()) result.outputTokens = usage["output_tokens"].get<uint32_t>();
		}
		// Only text blocks; thinking blocks (empty by default) and anything else are skipped
		for (const auto& block : json["content"]) {
			if (block.is_object() && block.value("type", "") == "text" && block.contains("text") && block["text"].is_string()) result.text += block["text"].get<std::string>();
		}

		if (result.stopReason == "refusal") {
			result.error = eError::BAD_RESPONSE;
			result.message = "The model declined to answer this one";
			return result;
		}
		if (result.stopReason == "max_tokens") {
			result.error = eError::BAD_RESPONSE;
			result.message = "The answer was cut off at the token limit (ai_helper_max_tokens); raise it or try again";
			return result;
		}
		if (result.text.empty()) {
			result.error = eError::BAD_RESPONSE;
			result.message = "The API's answer has no text";
			return result;
		}
		result.ok = true;
		return result;
	}

	std::string DescribeError(long status, const std::string& body) {
		const auto json = nlohmann::json::parse(body, nullptr, false);
		std::string type, message;
		if (json.is_object() && json.contains("error") && json["error"].is_object()) {
			type = json["error"].value("type", "");
			message = json["error"].value("message", "");
		}
		// Messages are the API's own; keep them short
		if (message.size() > 300) message = message.substr(0, 300) + "...";
		std::string text = "Claude API error " + std::to_string(status);
		if (!type.empty()) text += " (" + type + ")";
		if (status == 401) text += ": the API key was refused; check claude_api_key";
		else if (status == 403) text += ": this key may not use that model or feature";
		else if (status == 404) text += ": check claude_model and claude_api_base";
		else if (!message.empty()) text += ": " + message;
		return text;
	}

	HttpResponse CurlTransport(const std::string& url, const std::vector<std::string>& headers, const std::string& body, uint32_t timeoutSeconds) {
		HttpResponse response;
		CURL* curl = curl_easy_init();
		if (!curl) {
			response.error = "Could not initialise libcurl";
			return response;
		}
		char errorBuffer[CURL_ERROR_SIZE]{};
		curl_slist* list = nullptr;
		for (const auto& header : headers) list = curl_slist_append(list, header.c_str());
		// No "Expect: 100-continue" round trip before the body
		list = curl_slist_append(list, "Expect:");

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteBody);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, WriteHeader);
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.retryAfter);
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeoutSeconds));
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
		// Never follow redirects: the key header would go along
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
		curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https,http");

		const CURLcode result = curl_easy_perform(curl);
		if (result == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
		else response.error = errorBuffer[0] ? errorBuffer : curl_easy_strerror(result);
		curl_slist_free_all(list);
		curl_easy_cleanup(curl);
		return response;
	}

	Result Send(const Config& config, const Request& request, const Transport& transport, const Sleep& sleep) {
		if (config.apiKey.empty()) return Failure(eError::NO_KEY, "No Claude API key is set (claude_api_key)");
		std::string error;
		const auto endpoint = Endpoint(config.base, error);
		if (!endpoint) return Failure(eError::BAD_CONFIG, error);

		const auto headers = BuildHeaders(config);
		const auto body = BuildBody(config, request).dump();
		const Sleep wait = sleep ? sleep : [](uint32_t ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); };

		HttpResponse last;
		uint32_t attempts = 0;
		for (uint32_t attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
			if (attempt > 0) wait(RetryDelayMs(attempt - 1, last.retryAfter));
			last = transport(*endpoint, headers, body, config.timeoutSeconds);
			attempts++;
			if (last.status == 200) {
				auto result = ParseResponse(last.body);
				result.attempts = attempts;
				result.status = 200;
				return result;
			}
			if (!Retryable(last.status)) {
				auto result = Failure(eError::API, DescribeError(last.status, last.body), last.status);
				result.attempts = attempts;
				return result;
			}
		}
		auto result = Failure(eError::UNAVAILABLE, last.status == 0
			? "Could not reach the Claude API after " + std::to_string(attempts) + " tries: " + last.error
			: DescribeError(last.status, last.body) + " (gave up after " + std::to_string(attempts) + " tries)", last.status);
		result.attempts = attempts;
		return result;
	}
}
