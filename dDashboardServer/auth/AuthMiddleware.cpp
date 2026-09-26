#include "AuthMiddleware.h"
#include "AuthTokenHandler.h"
#include "Game.h"
#include "Logger.h"
#include "HTTPContext.h"
#include "Web.h"

bool AuthMiddleware::Process(HTTPContext& context, HTTPReply& reply) {
	// Any state-changing request that doesn't carry its own Authorization header could have been sent by another
	// site through the visitor's browser (a form or a text/plain fetch). Those can't set custom headers, so require
	// one. This covers requests before sign-in too (login, registration, password reset); signed-in cookie requests
	// are checked again in RequireAuthMiddleware.
	const bool safeMethod = context.method == "GET" || context.method == "HEAD" || context.method == "OPTIONS";
	if (!safeMethod && context.GetHeader("Authorization").empty() && context.GetHeader("X-Requested-With").empty()) {
		LOG("Rejected %s %s without X-Requested-With (possible CSRF) from %s", context.method.c_str(), context.path.c_str(), context.clientIP.c_str());
		reply.status = eHTTPStatusCode::FORBIDDEN;
		reply.message = "{\"success\":false,\"error\":\"Missing X-Requested-With header\"}";
		reply.contentType = eContentType::APPLICATION_JSON;
		return false;
	}
	return AuthTokenHandler::ProcessHTTPContext(context, reply);
}
