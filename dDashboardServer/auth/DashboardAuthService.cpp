#include "DashboardAuthService.h"
#include "JWTUtils.h"
#include "Database.h"
#include "Logger.h"
#include "Game.h"
#include "dConfig.h"
#include "GeneralUtils.h"
#include <bcrypt/bcrypt.h>
#include <ctime>

namespace {
	constexpr int64_t LOCKOUT_DURATION = 15 * 60; // 15 minutes in seconds

}

DashboardAuthService::LoginResult DashboardAuthService::Login(
	const std::string& username, 
	const std::string& password, 
	bool rememberMe) {

	LoginResult result;

	if (username.empty() || password.empty()) {
		result.message = "Username and password are required";
		return result;
	}

	if (password.length() > 40) {
		result.message = "Password exceeds maximum length (40 characters)";
		return result;
	}

	try {
		// Get account info
		auto accountInfo = Database::Get()->GetAccountInfo(username);
		if (!accountInfo) {
			result.message = "Invalid username or password";
			LOG_DEBUG("Login attempt for non-existent user: %s", username.c_str());
			return result;
		}

		uint32_t accountId = accountInfo->id;

		// Check if account is locked
		bool isLockedOut = Database::Get()->IsLockedOut(accountId);

		if (isLockedOut) {
			// Record failed attempt even without checking password
			Database::Get()->RecordFailedAttempt(accountId);
			uint8_t failedAttempts = Database::Get()->GetFailedAttempts(accountId);
			
			result.message = "Account is locked due to too many failed attempts";
			result.accountLocked = true;
			LOG("Login attempt on locked account: %s (failed attempts: %d)", username.c_str(), failedAttempts);
			return result;
		}

		// Check password
		if (::bcrypt_checkpw(password.c_str(), accountInfo->bcryptPassword.c_str()) != 0) {
			// Record failed attempt
			Database::Get()->RecordFailedAttempt(accountId);
			uint8_t newFailedAttempts = Database::Get()->GetFailedAttempts(accountId);

			// Lock account after 3 failed attempts
			if (newFailedAttempts >= 3) {
				int64_t lockoutUntil = std::time(nullptr) + LOCKOUT_DURATION;
				Database::Get()->SetLockout(accountId, lockoutUntil);
				result.message = "Account locked due to too many failed attempts";
				result.accountLocked = true;
				LOG("Account locked after failed attempts: %s", username.c_str());
			} else {
				result.message = "Invalid username or password";
				LOG_DEBUG("Failed login attempt for user: %s (attempt %d/3)", 
					username.c_str(), newFailedAttempts);
			}
			return result;
		}

		// Check GM level
		if (!HasDashboardAccess(static_cast<uint8_t>(accountInfo->maxGmLevel))) {
			result.message = "Access denied: insufficient permissions";
			LOG("Access denied for non-admin user: %s", username.c_str());
			return result;
		}

		// Successful login
		Database::Get()->ClearFailedAttempts(accountId);
		result.success = true;
		result.gmLevel = static_cast<uint8_t>(accountInfo->maxGmLevel);
		result.token = JWTUtils::GenerateToken(username, result.gmLevel, rememberMe);
		result.message = "Login successful";

		LOG("Successful login: %s (GM Level: %d)", username.c_str(), result.gmLevel);
		return result;

	} catch (const std::exception& ex) {
		result.message = "An error occurred during login";
		LOG("Error during login process: %s", ex.what());
		return result;
	}
}

bool DashboardAuthService::VerifyToken(const std::string& token, std::string& username, uint8_t& gmLevel) {
	JWTUtils::JWTPayload payload;
	if (!JWTUtils::ValidateToken(token, payload)) {
		LOG_DEBUG("Token validation failed: invalid or expired JWT");
		return false;
	}

	username = payload.username;
	gmLevel = payload.gmLevel;

	// Optionally verify user still exists and has access
	try {
		auto accountInfo = Database::Get()->GetAccountInfo(username);
		if (!accountInfo || !HasDashboardAccess(static_cast<uint8_t>(accountInfo->maxGmLevel))) {
			LOG_DEBUG("Token verification failed: user no longer has access");
			return false;
		}
	} catch (const std::exception& ex) {
		LOG_DEBUG("Error verifying user during token validation: %s", ex.what());
		return false;
	}

	LOG_DEBUG("Token verified successfully for user: %s (GM Level: %d)", username.c_str(), gmLevel);
	return true;
}

bool DashboardAuthService::HasDashboardAccess(uint8_t gmLevel) {
	// Get minimum GM level from config (default 0 = any user)
	uint8_t minGmLevel = 0;
	
	if (Game::config) {
		const std::string& minGmLevelStr = Game::config->GetValue("min_dashboard_gm_level");
		if (!minGmLevelStr.empty()) {
			const auto parsed = GeneralUtils::TryParse<uint8_t>(minGmLevelStr);
			if (parsed) {
				minGmLevel = parsed.value();
			}
		}
	}
	
	return gmLevel >= minGmLevel;
}
