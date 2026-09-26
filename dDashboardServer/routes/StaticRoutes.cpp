#include "StaticRoutes.h"
#include "RouteUtils.h"
#include "Web.h"
#include "HTTPContext.h"
#include "eHTTPMethod.h"
#include "Game.h"
#include "Logger.h"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace {
	constexpr const char* STATIC_DIR = "dDashboardServer/static/";

	// Only plain file names with the expected extension, so a request can never leave the static directory
	bool IsSafeFileName(std::string_view name, std::string_view extension) {
		if (name.empty() || name.size() > 64 || !name.ends_with(extension) || name.starts_with('.')) return false;
		return std::ranges::all_of(name, [](char c) {
			return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
		});
	}

	void ServeDirectory(const std::string& urlPrefix, const std::string& directory, const std::string& extension, eContentType contentType) {
		RouteUtils::Route(eHTTPMethod::GET, urlPrefix + ":file", RouteUtils::PUBLIC, "Static " + extension + " files",
			[directory, extension, contentType](HTTPReply& reply, const HTTPContext& context) {
				const auto name = RouteUtils::PathSegment(context.path, 1);
				reply.status = eHTTPStatusCode::NOT_FOUND;
				reply.message = "";
				reply.contentType = eContentType::TEXT_PLAIN;
				if (!IsSafeFileName(name, extension)) return;

				std::ifstream file(std::string(STATIC_DIR) + directory + std::string(name), std::ios::binary);
				if (!file) return;
				std::stringstream buffer;
				buffer << file.rdbuf();
				reply.status = eHTTPStatusCode::OK;
				reply.message = buffer.str();
				reply.contentType = contentType;
				// Revalidate every time so updated scripts are picked up after a server update
				reply.headers.push_back("Cache-Control: no-cache");
			});
	}
}

void RegisterStaticRoutes() {
	// Browsers request this on their own; answer with nothing rather than a 404
	RouteUtils::Route(eHTTPMethod::GET, "/favicon.ico", RouteUtils::PUBLIC, "Empty favicon", [](HTTPReply& reply, const HTTPContext&) {
		reply.status = eHTTPStatusCode::NO_CONTENT;
		reply.message = "";
		reply.contentType = eContentType::TEXT_PLAIN;
	});

	ServeDirectory("/css/", "css/", ".css", eContentType::TEXT_CSS);
	ServeDirectory("/js/", "js/", ".js", eContentType::TEXT_JAVASCRIPT);
}
