#pragma once

#include <memory>
#include "HTTPContext.h"

// Forward declaration
struct HTTPReply;

/**
 * Middleware Interface
 * 
 * All middleware implements this interface and is called in order during request processing.
 * Middleware can:
 * - Inspect and modify the request (HTTPContext)
 * - Populate authentication/authorization info
 * - Short-circuit the chain by setting a reply and returning false
 * - Pass to the next middleware by returning true
 */
class IHTTPMiddleware {
public:
	virtual ~IHTTPMiddleware() = default;
	
	/**
	 * Process the request through this middleware
	 * 
	 * @param context The HTTP request context
	 * @param reply The HTTP reply (can be populated to short-circuit)
	 * @return true to continue to next middleware, false to stop processing
	 */
	virtual bool Process(HTTPContext& context, HTTPReply& reply) = 0;
	
	/**
	 * Get a friendly name for this middleware
	 */
	virtual std::string GetName() const = 0;
};

using MiddlewarePtr = std::shared_ptr<IHTTPMiddleware>;
