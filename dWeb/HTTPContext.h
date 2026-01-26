#pragma once

#include <string>
#include <map>
#include <memory>
#include <algorithm>
#include "eHTTPStatusCode.h"

/**
 * HTTP Request Context
 * 
 * Carries all request metadata through the middleware chain.
 * Populated by the Web framework before middleware/handlers are called.
 */
struct HTTPContext {
	// Request metadata
	std::string method{};
	std::string path{};
	std::string queryString{};
	std::string body{};
	
	// Request headers (header name -> value)
	// Header names are lowercase for case-insensitive lookup
	std::map<std::string, std::string> headers{};
	
	// Client information
	std::string clientIP{};
	
	// Authentication information (populated by auth middleware)
	bool isAuthenticated = false;
	std::string authenticatedUser{};
	uint8_t gmLevel = 0;
	
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
};
