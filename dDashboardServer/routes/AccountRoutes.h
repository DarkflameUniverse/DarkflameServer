#pragma once

#include <cstdint>
#include <string>

// Email-based account features: password reset, email verification, admin email tools
void RegisterAccountRoutes();

// Email a confirmation link for an address already stored (unconfirmed) on the account. Returns a request id.
// requester is the account that sees the outcome (0: nobody, e.g. self-registration before signing in).
uint32_t SendVerificationEmail(uint32_t accountId, const std::string& username, const std::string& email, uint32_t requester);
