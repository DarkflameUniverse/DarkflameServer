#ifndef __IACCOUNTEMAILS__H__
#define __IACCOUNTEMAILS__H__

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// Account email addresses, single-use account tokens (password reset, email verification) and session invalidation
class IAccountEmails {
public:
	struct EmailInfo {
		std::string email;
		bool confirmed{};
	};

	struct AccountToken {
		uint32_t accountId{};
		std::string data;
	};

	virtual std::optional<EmailInfo> GetAccountEmail(const uint32_t accountId) = 0;
	virtual void SetAccountEmail(const uint32_t accountId, const std::string_view email, const bool confirmed) = 0;

	// Account with this confirmed address (case-insensitive)
	virtual std::optional<uint32_t> GetAccountIdByConfirmedEmail(const std::string_view email) = 0;

	// Tokens are stored by hash only. Consuming returns and deletes an unexpired token.
	virtual void InsertAccountToken(const std::string_view tokenHash, const uint32_t accountId, const std::string_view purpose, const std::string_view data, const int64_t expiresAt) = 0;
	virtual std::optional<AccountToken> ConsumeAccountToken(const std::string_view tokenHash, const std::string_view purpose) = 0;
	virtual void DeleteAccountTokens(const uint32_t accountId, const std::string_view purpose) = 0;
	virtual void DeleteExpiredAccountTokens() = 0;

	// Login sessions issued before this unix time are no longer accepted
	virtual int64_t GetSessionsValidAfter(const uint32_t accountId) = 0;
	virtual void SetSessionsValidAfter(const uint32_t accountId, const int64_t time) = 0;
};

#endif  //!__IACCOUNTEMAILS__H__
