#pragma once

#include <memory>
#include <string>
#include <vector>

#include "eHTTPStatusCode.h"

// Content type enum for HTTP responses
enum class eContentType {
	APPLICATION_JSON,
	TEXT_HTML,
	TEXT_CSS,
	TEXT_JAVASCRIPT,
	TEXT_PLAIN,
	TEXT_CSV,
	IMAGE_PNG,
	IMAGE_JPEG,
	APPLICATION_OCTET_STREAM,
	TEXT_PROMETHEUS // the Prometheus text exposition format
};

struct DeferredState;

// For passing HTTP messages between functions
struct HTTPReply {
	eHTTPStatusCode status = eHTTPStatusCode::NOT_FOUND;
	std::string message = "{\"error\":\"Not Found\"}";
	eContentType contentType = eContentType::APPLICATION_JSON;
	std::string location = "";  // For redirect responses (Location header)
	std::vector<std::string> headers{}; // Extra raw headers, e.g. "Set-Cookie: a=b"
	// When set on a 200 reply, this file is streamed from disk as the body (with contentType and headers) instead of
	// message, so large downloads never sit in memory
	std::string file{};
	// Delete `file` once it's being sent (a temporary file made for this reply). Where an open file can't be deleted
	// (Windows) it stays, so whoever makes such files should also clear out old ones.
	bool removeFile{};
	// Set by Web::Defer: the handler answers later, from another thread (DeferredReply), so nothing is sent now
	std::shared_ptr<DeferredState> deferred{};
};
