#include "RequireAuthMiddleware.h"
#include "HTTPContext.h"
#include "Game.h"
#include "Logger.h"

bool RequireAuthMiddleware::Process(HTTPContext& context, HTTPReply& reply) {
	if (!context.isAuthenticated) {
		LOG_DEBUG("Rejected request to %s: not authenticated", context.path.c_str());
		reply.status = eHTTPStatusCode::UNAUTHORIZED;
		reply.message = R"({"error":"Unauthorized","message":"Authentication required"})";
		reply.contentType = eContentType::APPLICATION_JSON;
		return false; // Stop processing chain
	}
	
	if (context.gmLevel < minGmLevel) {
		LOG("Rejected request to %s: insufficient permissions (gmLevel=%d, required=%d)", 
			context.path.c_str(), context.gmLevel, minGmLevel);
		reply.status = eHTTPStatusCode::FORBIDDEN;
		reply.message = R"({"error":"Forbidden","message":"Insufficient permissions"})";
		reply.contentType = eContentType::APPLICATION_JSON;
		return false; // Stop processing chain
	}
	
	return true; // Continue to next middleware
}
