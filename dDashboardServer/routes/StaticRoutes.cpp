#include "StaticRoutes.h"
#include "Web.h"
#include "HTTPContext.h"
#include "eHTTPMethod.h"
#include "Game.h"
#include "Logger.h"
#include <fstream>
#include <sstream>

namespace {
	std::string ReadFileToString(const std::string& filePath) {
		std::ifstream file(filePath);
		if (!file.is_open()) {
			LOG("Failed to open file: %s", filePath.c_str());
			return "";
		}
		std::stringstream buffer{};
		buffer << file.rdbuf();
		return buffer.str();
	}

	eContentType GetContentType(const std::string& filePath) {
		if (filePath.ends_with(".css")) {
			return eContentType::TEXT_CSS;
		} else if (filePath.ends_with(".js")) {
			return eContentType::TEXT_JAVASCRIPT;
		} else if (filePath.ends_with(".html")) {
			return eContentType::TEXT_HTML;
		} else if (filePath.ends_with(".png")) {
			return eContentType::IMAGE_PNG;
		} else if (filePath.ends_with(".jpg") || filePath.ends_with(".jpeg")) {
			return eContentType::IMAGE_JPEG;
		} else if (filePath.ends_with(".json")) {
			return eContentType::APPLICATION_JSON;
		}
		return eContentType::TEXT_PLAIN;
	}

	void ServeStaticFile(const std::string& urlPath, const std::string& filePath) {
		Game::web.RegisterHTTPRoute({
			.path = urlPath,
			.method = eHTTPMethod::GET,
			.middleware = {},
			.handle = [filePath](HTTPReply& reply, const HTTPContext& context) {
				const std::string content = ReadFileToString(filePath);
				if (content.empty()) {
					reply.status = eHTTPStatusCode::NOT_FOUND;
					reply.message = "{\"error\":\"File not found\"}";
					reply.contentType = eContentType::APPLICATION_JSON;
				} else {
					reply.status = eHTTPStatusCode::OK;
					reply.message = content;
					reply.contentType = GetContentType(filePath);
				}
			}
		});
	}
}

void RegisterStaticRoutes() {
	// Serve CSS files
	ServeStaticFile("/css/dashboard.css", "dDashboardServer/static/css/dashboard.css");
	ServeStaticFile("/css/login.css", "dDashboardServer/static/css/login.css");
	
	// Serve JavaScript files
	ServeStaticFile("/js/dashboard.js", "dDashboardServer/static/js/dashboard.js");
	ServeStaticFile("/js/login.js", "dDashboardServer/static/js/login.js");
}
