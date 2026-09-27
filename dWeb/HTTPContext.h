#pragma once

#include <string>
#include <map>
#include <memory>
#include <algorithm>
#include "eHTTPStatusCode.h"
#include "json.hpp"
#include "ApiKeyScope.h"

/**
 * HTTP Request Context
 * 
 * Carries all request metadata through the middleware chain.
 * Populated by the Web framework before middleware/handlers are called.
 */
struct HTTPContext {
	// Request metadata
	std::string method{};
	std::string path{};         // lowercased: routes match without regard to case
	std::string originalPath{}; // as sent, for parameters whose case matters (file names)
	std::string queryString{};
	std::string body{};
	
	// Request headers (header name -> value)
	// Header names are lowercase for case-insensitive lookup
	std::map<std::string, std::string> headers{};
	
	// Client information
	std::string clientIP{};
	unsigned long connectionId = 0; // the web server's id of the connection (for Web::Defer)
	
	// Authentication information (populated by auth middleware)
	bool isAuthenticated = false;
	std::string authenticatedUser{};
	uint32_t accountId = 0;
	uint8_t gmLevel = 0;
	// Set when an API key authenticated the request: the key's scope, on top of what the account may do (gmLevel).
	// Every permission check must honour it (RouteUtils::Can and friends do).
	std::shared_ptr<const ApiKeys::Scope> apiKey{};
	
	// Custom data for middleware to communicate
	std::map<std::string, std::string> userData{};
	
	/**
	 * Get header value (case-insensitive)
	 */
	const std::string& GetHeader(const std::string& headerName) const {
		static const std::string empty{};
		
		// Convert to lowercase for comparison
		std::string lowerName = headerName;
		std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
		
		const auto it = headers.find(lowerName);
		return it != headers.end() ? it->second : empty;
	}
	
	/**
	 * Set header value (automatically lowercased)
	 */
	void SetHeader(const std::string& headerName, const std::string& value) {
		std::string lowerName = headerName;
		std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
		headers[lowerName] = value;
	}
	
	/**
	 * Get user data as JSON for template rendering
	 */
	nlohmann::json GetUserDataJson() const {
		nlohmann::json userData;
		userData["username"] = authenticatedUser;
		userData["gmLevel"] = gmLevel;
		userData["accountId"] = accountId;
		return userData;
	}
};
