#include "SmtpClient.h"

#include <algorithm>
#include <ctime>
#include <random>

#include <curl/curl.h>

namespace {
	std::string Domain(const std::string& address) {
		const auto at = address.rfind('@');
		return at == std::string::npos ? "localhost" : address.substr(at + 1);
	}

	std::string RfcDate() {
		char date[64];
		const auto now = std::time(nullptr);
		std::tm utc{};
#ifdef _WIN32
		gmtime_s(&utc, &now);
#else
		gmtime_r(&now, &utc);
#endif
		std::strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S +0000", &utc);
		return date;
	}

	struct Upload {
		const std::string& data;
		size_t offset{ 0 };
	};

	size_t ReadUpload(char* buffer, size_t size, size_t count, void* userData) {
		auto* upload = static_cast<Upload*>(userData);
		const size_t length = std::min(size * count, upload->data.size() - upload->offset);
		std::copy_n(upload->data.data() + upload->offset, length, buffer);
		upload->offset += length;
		return length;
	}
}

namespace Smtp {
	bool IsValidAddress(std::string_view address) {
		if (address.size() < 3 || address.size() > 254) return false;
		const auto at = address.find('@');
		if (at == std::string_view::npos || at == 0 || at != address.rfind('@') || at == address.size() - 1) return false;
		if (address.substr(at + 1).find('.') == std::string_view::npos) return false;
		return std::ranges::none_of(address, [](char c) {
			const auto u = static_cast<unsigned char>(c);
			return u <= 0x20 || u == 0x7f || c == '<' || c == '>' || c == '(' || c == ')' || c == ',' || c == ';' || c == ':' || c == '"' || c == '\\' || c == '[' || c == ']';
		});
	}

	std::string Base64(std::string_view data, bool wrapLines) {
		static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string out;
		size_t lineLength = 0;
		for (size_t i = 0; i < data.size(); i += 3) {
			const uint32_t b0 = static_cast<unsigned char>(data[i]);
			const uint32_t b1 = i + 1 < data.size() ? static_cast<unsigned char>(data[i + 1]) : 0;
			const uint32_t b2 = i + 2 < data.size() ? static_cast<unsigned char>(data[i + 2]) : 0;
			const uint32_t triple = (b0 << 16) | (b1 << 8) | b2;
			out += alphabet[(triple >> 18) & 0x3f];
			out += alphabet[(triple >> 12) & 0x3f];
			out += i + 1 < data.size() ? alphabet[(triple >> 6) & 0x3f] : '=';
			out += i + 2 < data.size() ? alphabet[triple & 0x3f] : '=';
			lineLength += 4;
			if (wrapLines && lineLength >= 76 && i + 3 < data.size()) {
				out += "\r\n";
				lineLength = 0;
			}
		}
		return out;
	}

	std::string EncodeHeader(std::string_view value) {
		const bool plain = std::ranges::all_of(value, [](char c) {
			const auto u = static_cast<unsigned char>(c);
			return u >= 0x20 && u < 0x7f;
		});
		if (plain) return std::string(value);
		return "=?UTF-8?B?" + Base64(value, false) + "?=";
	}

	std::string BuildMessage(const Config& config, const Message& message, std::string_view date, std::string_view messageId) {
		// Header values never contain CR/LF: addresses are validated and text is encoded or stripped
		const auto clean = [](std::string_view s) {
			std::string out;
			for (const char c : s) if (c != '\r' && c != '\n') out += c;
			return out;
		};
		const std::string fromName = clean(config.fromName);

		std::string data;
		data += "From: " + (fromName.empty() ? "" : "\"" + EncodeHeader(fromName) + "\" ") + "<" + config.fromAddress + ">\r\n";
		data += "To: <" + message.to + ">\r\n";
		data += "Subject: " + EncodeHeader(clean(message.subject)) + "\r\n";
		data += "Date: " + std::string(date) + "\r\n";
		data += "Message-ID: <" + std::string(messageId) + ">\r\n";
		data += "MIME-Version: 1.0\r\n";
		data += "Content-Type: text/plain; charset=UTF-8\r\n";
		data += "Content-Transfer-Encoding: base64\r\n";
		data += "\r\n";

		// Normalise line endings to CRLF before encoding
		std::string body;
		for (size_t i = 0; i < message.body.size(); i++) {
			if (message.body[i] == '\n' && (i == 0 || message.body[i - 1] != '\r')) body += '\r';
			body += message.body[i];
		}
		data += Base64(body, true) + "\r\n";
		// No dot-stuffing here: libcurl escapes lines starting with '.' when uploading SMTP data
		return data;
	}

	void GlobalInit() {
		curl_global_init(CURL_GLOBAL_DEFAULT);
	}

	void GlobalCleanup() {
		curl_global_cleanup();
	}

	std::optional<std::string> Send(const Config& config, const Message& message, bool* authFailed) {
		if (authFailed) *authFailed = false;
		if (config.host.empty()) return "SMTP is not configured (smtp_host)";
		if (!IsValidAddress(config.fromAddress)) return "Invalid sender address (smtp_from_address)";
		if (!IsValidAddress(message.to)) return "Invalid recipient address";
		// Never send credentials in plain text
		if ((!config.username.empty() || !config.oauth2Token.empty()) && config.security == eSecurity::NONE) {
			return "Refusing to send SMTP credentials without TLS; set smtp_security to starttls or tls";
		}

		CURL* curl = curl_easy_init();
		if (!curl) return "Could not initialise libcurl";

		std::random_device rd;
		const std::string messageId = std::to_string(std::time(nullptr)) + "." + std::to_string(rd()) + std::to_string(rd()) + "@" + Domain(config.fromAddress);
		const std::string payload = BuildMessage(config, message, RfcDate(), messageId);
		Upload upload{ payload };

		const std::string scheme = config.security == eSecurity::TLS ? "smtps://" : "smtp://";
		const std::string url = scheme + config.host + ":" + std::to_string(config.port) + "/" + Domain(config.fromAddress);
		const std::string from = "<" + config.fromAddress + ">";
		curl_slist* recipients = curl_slist_append(nullptr, ("<" + message.to + ">").c_str());
		char errorBuffer[CURL_ERROR_SIZE]{};

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L); // we run on a worker thread
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, static_cast<long>(config.timeoutSeconds));
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(config.timeoutSeconds) * 3);
		if (config.security == eSecurity::STARTTLS) curl_easy_setopt(curl, CURLOPT_USE_SSL, static_cast<long>(CURLUSESSL_ALL));
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, config.verifyCertificate ? 1L : 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, config.verifyCertificate ? 2L : 0L);
		if (!config.caFile.empty()) curl_easy_setopt(curl, CURLOPT_CAINFO, config.caFile.c_str());
		if (!config.oauth2Token.empty()) {
			// libcurl picks OAUTHBEARER or XOAUTH2 from what the server offers
			curl_easy_setopt(curl, CURLOPT_USERNAME, config.username.c_str());
			curl_easy_setopt(curl, CURLOPT_XOAUTH2_BEARER, config.oauth2Token.c_str());
		} else if (!config.username.empty()) {
			curl_easy_setopt(curl, CURLOPT_USERNAME, config.username.c_str());
			curl_easy_setopt(curl, CURLOPT_PASSWORD, config.password.c_str());
		}
		curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from.c_str());
		curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, ReadUpload);
		curl_easy_setopt(curl, CURLOPT_READDATA, &upload);
		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

		const CURLcode result = curl_easy_perform(curl);
		curl_slist_free_all(recipients);
		curl_easy_cleanup(curl);

		if (result == CURLE_OK) return std::nullopt;
		std::string error = errorBuffer[0] ? errorBuffer : curl_easy_strerror(result);
		if (result == CURLE_LOGIN_DENIED) {
			if (authFailed) *authFailed = true;
			error = "SMTP login failed: " + error;
		}
		else if (result == CURLE_PEER_FAILED_VERIFICATION) error = "Certificate verification failed: " + error;
		return error;
	}
}
