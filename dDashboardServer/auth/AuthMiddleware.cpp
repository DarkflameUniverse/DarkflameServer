#include "AuthMiddleware.h"
#include "AuthTokenHandler.h"

bool AuthMiddleware::Process(HTTPContext& context, HTTPReply& reply) {
	return AuthTokenHandler::ProcessHTTPContext(context, reply);
}
