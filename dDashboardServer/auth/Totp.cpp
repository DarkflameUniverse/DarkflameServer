#include "Totp.h"

#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include "BinaryPathFinder.h"
#include "Game.h"
#include "Logger.h"
#include "dConfig.h"

namespace {
	constexpr std::string_view BASE32_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
	constexpr std::string_view RECOVERY_ALPHABET = "abcdefghjkmnpqrstuvwxyz23456789"; // no look-alikes (i/l/1, o/0)
	constexpr size_t KEY_SIZE = 32;
	constexpr size_t NONCE_SIZE = 12;
	constexpr size_t TAG_SIZE = 16;

	std::vector<uint8_t> g_Key;

	std::string ToHex(const std::vector<uint8_t>& bytes) {
		static constexpr char digits[] = "0123456789abcdef";
		std::string out;
		for (const auto b : bytes) { out += digits[b >> 4]; out += digits[b & 15]; }
		return out;
	}

	std::optional<std::vector<uint8_t>> FromHex(std::string_view hex) {
		if (hex.size() % 2 != 0) return std::nullopt;
		std::vector<uint8_t> out;
		for (size_t i = 0; i < hex.size(); i += 2) {
			const auto nibble = [](char c) -> int {
				if (c >= '0' && c <= '9') return c - '0';
				if (c >= 'a' && c <= 'f') return c - 'a' + 10;
				if (c >= 'A' && c <= 'F') return c - 'A' + 10;
				return -1;
			};
			const int hi = nibble(hex[i]), lo = nibble(hex[i + 1]);
			if (hi < 0 || lo < 0) return std::nullopt;
			out.push_back(static_cast<uint8_t>(hi << 4 | lo));
		}
		return out;
	}

	// Compare without leaking where the first difference is
	bool ConstantTimeEquals(std::string_view a, std::string_view b) {
		if (a.size() != b.size()) return false;
		unsigned char diff = 0;
		for (size_t i = 0; i < a.size(); i++) diff |= static_cast<unsigned char>(a[i] ^ b[i]);
		return diff == 0;
	}

	std::string PercentEncode(std::string_view text) {
		static constexpr char digits[] = "0123456789ABCDEF";
		std::string out;
		for (const unsigned char c : text) {
			if (std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') out += static_cast<char>(c);
			else { out += '%'; out += digits[c >> 4]; out += digits[c & 15]; }
		}
		return out;
	}
}

namespace Totp {
	std::string Base32Encode(const std::vector<uint8_t>& data) {
		std::string out;
		uint32_t buffer = 0;
		int bits = 0;
		for (const auto byte : data) {
			buffer = (buffer << 8) | byte;
			bits += 8;
			while (bits >= 5) {
				out += BASE32_ALPHABET[(buffer >> (bits - 5)) & 31];
				bits -= 5;
			}
		}
		if (bits > 0) out += BASE32_ALPHABET[(buffer << (5 - bits)) & 31];
		return out;
	}

	std::optional<std::vector<uint8_t>> Base32Decode(std::string_view text) {
		std::vector<uint8_t> out;
		uint32_t buffer = 0;
		int bits = 0;
		for (char c : text) {
			if (c == ' ' || c == '-' || c == '=') continue;
			if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
			const auto index = BASE32_ALPHABET.find(c);
			if (index == std::string_view::npos) return std::nullopt;
			buffer = (buffer << 5) | static_cast<uint32_t>(index);
			bits += 5;
			if (bits >= 8) {
				out.push_back(static_cast<uint8_t>((buffer >> (bits - 8)) & 0xFF));
				bits -= 8;
			}
		}
		return out;
	}

	std::string GenerateSecret() {
		std::vector<uint8_t> bytes(20);
		if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) return "";
		return Base32Encode(bytes);
	}

	std::string CodeAt(const std::vector<uint8_t>& secret, int64_t step) {
		std::array<uint8_t, 8> counter{};
		for (int i = 7; i >= 0; i--) {
			counter[static_cast<size_t>(i)] = static_cast<uint8_t>(step & 0xFF);
			step >>= 8;
		}
		unsigned char digest[EVP_MAX_MD_SIZE];
		unsigned int length = 0;
		HMAC(EVP_sha1(), secret.data(), static_cast<int>(secret.size()), counter.data(), counter.size(), digest, &length);
		const int offset = digest[length - 1] & 0x0F;
		const uint32_t binary = (static_cast<uint32_t>(digest[offset] & 0x7F) << 24) | (static_cast<uint32_t>(digest[offset + 1]) << 16) |
			(static_cast<uint32_t>(digest[offset + 2]) << 8) | digest[offset + 3];
		std::string code = std::to_string(binary % 1000000);
		return std::string(DIGITS - code.size(), '0') + code;
	}

	std::optional<int64_t> Verify(const std::string& base32Secret, std::string_view code, int64_t unixTime) {
		std::string digits;
		for (const char c : code) if (c >= '0' && c <= '9') digits += c;
		if (digits.size() != DIGITS) return std::nullopt;
		const auto secret = Base32Decode(base32Secret);
		if (!secret || secret->empty()) return std::nullopt;
		const int64_t current = unixTime / STEP_SECONDS;
		for (int64_t step = current - 1; step <= current + 1; step++) {
			if (ConstantTimeEquals(CodeAt(*secret, step), digits)) return step;
		}
		return std::nullopt;
	}

	std::string ProvisioningUri(const std::string& issuer, const std::string& account, const std::string& base32Secret) {
		return "otpauth://totp/" + PercentEncode(issuer) + ":" + PercentEncode(account) + "?secret=" + base32Secret +
			"&issuer=" + PercentEncode(issuer) + "&algorithm=SHA1&digits=6&period=30";
	}

	std::vector<std::string> GenerateRecoveryCodes(size_t count) {
		std::vector<std::string> codes;
		for (size_t n = 0; n < count; n++) {
			std::array<uint8_t, 12> bytes{};
			if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) return {};
			std::string code;
			for (size_t i = 0; i < bytes.size(); i++) {
				if (i > 0 && i % 4 == 0) code += '-';
				code += RECOVERY_ALPHABET[bytes[i] % RECOVERY_ALPHABET.size()];
			}
			codes.push_back(code);
		}
		return codes;
	}

	std::string NormalizeRecoveryCode(std::string_view code) {
		std::string out;
		for (char c : code) {
			if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
			if (RECOVERY_ALPHABET.find(c) != std::string_view::npos) out += c;
		}
		return out;
	}

	bool LoadKey() {
		const auto configured = Game::config ? Game::config->GetValue("totp_key") : "";
		if (!configured.empty()) {
			const auto key = FromHex(configured);
			if (!key || key->size() != KEY_SIZE) {
				LOG("totp_key must be 64 hex characters; two-factor login is unavailable");
				return false;
			}
			g_Key = *key;
			return true;
		}

		const auto keyPath = BinaryPathFinder::GetBinaryDir() / "dashboard_totp_key";
		std::string stored;
		if (std::ifstream in(keyPath); in) std::getline(in, stored);
		if (const auto key = FromHex(stored); key && key->size() == KEY_SIZE) {
			g_Key = *key;
			return true;
		}

		std::vector<uint8_t> key(KEY_SIZE);
		if (RAND_bytes(key.data(), static_cast<int>(key.size())) != 1) return false;
		std::ofstream out(keyPath, std::ios::trunc);
		out << ToHex(key);
		out.close();
		std::error_code ec;
		std::filesystem::permissions(keyPath, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace, ec);
		LOG("Generated a new two-factor login key at %s (back it up with the database, or everyone's 2FA stops working)", keyPath.string().c_str());
		g_Key = key;
		return true;
	}

	void SetKeyForTesting(const std::vector<uint8_t>& key) {
		g_Key = key;
	}

	// Stored as hex(nonce | ciphertext | tag)
	std::optional<std::string> EncryptSecret(const std::string& base32Secret) {
		if (g_Key.size() != KEY_SIZE) return std::nullopt;
		std::vector<uint8_t> nonce(NONCE_SIZE), ciphertext(base32Secret.size()), tag(TAG_SIZE);
		if (RAND_bytes(nonce.data(), static_cast<int>(nonce.size())) != 1) return std::nullopt;

		auto* ctx = EVP_CIPHER_CTX_new();
		int length = 0;
		bool ok = ctx && EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, g_Key.data(), nonce.data()) == 1 &&
			EVP_EncryptUpdate(ctx, ciphertext.data(), &length, reinterpret_cast<const unsigned char*>(base32Secret.data()), static_cast<int>(base32Secret.size())) == 1 &&
			EVP_EncryptFinal_ex(ctx, ciphertext.data() + length, &length) == 1 &&
			EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(TAG_SIZE), tag.data()) == 1;
		EVP_CIPHER_CTX_free(ctx);
		if (!ok) return std::nullopt;

		std::vector<uint8_t> out(nonce);
		out.insert(out.end(), ciphertext.begin(), ciphertext.end());
		out.insert(out.end(), tag.begin(), tag.end());
		return ToHex(out);
	}

	std::optional<std::string> DecryptSecret(const std::string& stored) {
		if (g_Key.size() != KEY_SIZE) return std::nullopt;
		const auto bytes = FromHex(stored);
		if (!bytes || bytes->size() < NONCE_SIZE + TAG_SIZE) return std::nullopt;
		const std::vector<uint8_t> nonce(bytes->begin(), bytes->begin() + NONCE_SIZE);
		const std::vector<uint8_t> ciphertext(bytes->begin() + NONCE_SIZE, bytes->end() - TAG_SIZE);
		std::vector<uint8_t> tag(bytes->end() - TAG_SIZE, bytes->end());
		std::vector<uint8_t> plain(ciphertext.size() + 1);

		auto* ctx = EVP_CIPHER_CTX_new();
		int length = 0, total = 0;
		bool ok = ctx && EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, g_Key.data(), nonce.data()) == 1 &&
			EVP_DecryptUpdate(ctx, plain.data(), &length, ciphertext.data(), static_cast<int>(ciphertext.size())) == 1;
		total = length;
		ok = ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(TAG_SIZE), tag.data()) == 1 &&
			EVP_DecryptFinal_ex(ctx, plain.data() + total, &length) == 1; // fails if the data or key is wrong
		EVP_CIPHER_CTX_free(ctx);
		if (!ok) return std::nullopt;
		total += length;
		return std::string(reinterpret_cast<const char*>(plain.data()), static_cast<size_t>(total));
	}
}
