#include "RequireAuthMiddleware.h"
#include "HTTPContext.h"
#include "Web.h"
#include "Game.h"
#include "Logger.h"

RequireAuthMiddleware::RequireAuthMiddleware(uint8_t minGmLevel) : minGmLevel(minGmLevel) {}

bool RequireAuthMiddleware::Process(HTTPContext& context, HTTPReply& reply) {
	// Check if user is authenticated
	if (!context.isAuthenticated) {
		LOG_DEBUG("Unauthorized access attempt to %s from %s", context.path.c_str(), context.clientIP.c_str());
		reply.status = eHTTPStatusCode::FOUND;
		reply.message = "";
		reply.location = "/login";
		reply.contentType = eContentType::TEXT_HTML;
		return false;  // Stop middleware chain and send reply
	}

	// Check if user has required GM level
	if (context.gmLevel < minGmLevel) {
		LOG_DEBUG("Forbidden access attempt by user %s (GM level %d < %d required) to %s from %s",
			context.authenticatedUser.c_str(), context.gmLevel, minGmLevel, 
			context.path.c_str(), context.clientIP.c_str());
		reply.status = eHTTPStatusCode::FORBIDDEN;
		reply.message = "{\"error\":\"Forbidden - Insufficient permissions\"}";
		reply.contentType = eContentType::APPLICATION_JSON;
		return false;  // Stop middleware chain and send reply
	}

	// Authentication passed
	LOG_DEBUG("User %s authenticated with GM level %d accessing %s",
		context.authenticatedUser.c_str(), context.gmLevel, context.path.c_str());
	return true;  // Continue to next middleware or route handler
}
