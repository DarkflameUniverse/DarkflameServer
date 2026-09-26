#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

/**
 * SMTP sending for the dashboard's account emails, built on libcurl.
 * Supports implicit TLS, STARTTLS and plain connections with the authentication methods the server offers,
 * and verifies the server certificate and host name (OS certificate store unless a CA file is given).
 */
namespace Smtp {
	enum class eSecurity : uint8_t {
		NONE,      // Plain text; only for a relay on the same machine or private network
		STARTTLS,  // Upgrade with STARTTLS (usually port 587)
		TLS        // TLS from the first byte (usually port 465)
	};

	struct Config {
		std::string host;
		uint16_t port{ 587 };
		eSecurity security{ eSecurity::STARTTLS };
		std::string username;
		std::string password;
		std::string fromAddress;
		std::string fromName;
		// PEM bundle to verify the server against. Empty uses the system trust store.
		std::string caFile;
		bool verifyCertificate{ true };
		uint32_t timeoutSeconds{ 20 };
		// OAuth2 access token; when set, authenticates with XOAUTH2 as `username` instead of a password
		std::string oauth2Token;
	};

	struct Message {
		std::string to;
		std::string subject;
		std::string body; // Plain text, UTF-8
	};

	// libcurl process-wide setup; call once before any Send and before starting threads
	void GlobalInit();
	void GlobalCleanup();

	// Send a message. Blocks until done; returns an error description on failure.
	// authFailed is set when the server rejected the credentials (e.g. an expired OAuth2 token).
	std::optional<std::string> Send(const Config& config, const Message& message, bool* authFailed = nullptr);

	// ---- Message formatting, exposed for testing ----

	// Loose address check: one '@', no whitespace/control characters or characters that could break headers
	bool IsValidAddress(std::string_view address);

	// Encode a header value as an RFC 2047 encoded word if it is not plain ASCII
	std::string EncodeHeader(std::string_view value);

	std::string Base64(std::string_view data, bool wrapLines);

	// Full RFC 5322 message (headers + base64 body) for upload; libcurl performs dot-stuffing
	std::string BuildMessage(const Config& config, const Message& message, std::string_view date, std::string_view messageId);
}
