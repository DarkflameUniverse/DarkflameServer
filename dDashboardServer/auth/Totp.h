#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * Time-based one-time passwords (RFC 6238, the codes authenticator apps show): 6 digits, 30 second steps,
 * HMAC-SHA1, which is what every common authenticator app expects.
 */
namespace Totp {
	constexpr int64_t STEP_SECONDS = 30;
	constexpr int DIGITS = 6;

	std::string Base32Encode(const std::vector<uint8_t>& data);
	std::optional<std::vector<uint8_t>> Base32Decode(std::string_view text); // ignores spaces and case

	// A new random 160-bit secret, base32 encoded
	std::string GenerateSecret();

	// The code for a time step (RFC 4226 HOTP with the step as counter)
	std::string CodeAt(const std::vector<uint8_t>& secret, int64_t step);

	/**
	 * Check a code against the current step and one step either side, for clock drift.
	 * @return the matching step, so the caller can refuse to accept the same step twice
	 */
	std::optional<int64_t> Verify(const std::string& base32Secret, std::string_view code, int64_t unixTime);

	// otpauth:// link for authenticator apps (usually shown as a QR code)
	std::string ProvisioningUri(const std::string& issuer, const std::string& account, const std::string& base32Secret);

	// Single-use recovery codes like "k7qd-9xw2-mz4p", and the normalized form their hash is taken from
	std::vector<std::string> GenerateRecoveryCodes(size_t count);
	std::string NormalizeRecoveryCode(std::string_view code);

	/**
	 * Secrets are stored encrypted (AES-256-GCM) so a database copy alone cannot generate codes.
	 * The key is `totp_key` from the dashboard config (64 hex characters), or a generated `dashboard_totp_key` file.
	 */
	bool LoadKey();
	std::optional<std::string> EncryptSecret(const std::string& base32Secret);
	std::optional<std::string> DecryptSecret(const std::string& stored);

	// For tests: use this key instead of loading one
	void SetKeyForTesting(const std::vector<uint8_t>& key);
}
