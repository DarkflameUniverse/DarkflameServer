#pragma once

#include <string>
#include <cstdint>

/**
 * Dashboard authentication service
 * Handles user login, password verification, and account lockout
 */
class DashboardAuthService {
public:
	/**
	 * Login result structure
	 */
	struct LoginResult {
		bool success{false};
		std::string message{};
		std::string token{}; // JWT token if successful
		uint8_t gmLevel{0}; // GM level if successful
		bool accountLocked{false}; // Account is locked out
	};

	/**
	 * Attempt to log in with username and password
	 * @param username The username
	 * @param password The plaintext password (max 40 characters)
	 * @param rememberMe If true, extends token expiration to 30 days
	 * @return LoginResult with success status and JWT token if successful
	 */
	static LoginResult Login(const std::string& username, const std::string& password, bool rememberMe = false);

	/**
	 * Verify that a token is valid and get the username
	 * @param token The JWT token
	 * @param username Output parameter for the username
	 * @param gmLevel Output parameter for the GM level
	 * @return true if token is valid
	 */
	static bool VerifyToken(const std::string& token, std::string& username, uint8_t& gmLevel);

	/**
	 * Check if user has required GM level for dashboard access
	 * @param gmLevel The user's GM level
	 * @return true if user can access dashboard (GM level > 0)
	 */
	static bool HasDashboardAccess(uint8_t gmLevel);
};
