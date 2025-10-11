#include "DashboardWeb.h"

// thanks bill gates
#ifdef _WIN32
#undef min
#undef max
#endif
#include "inja.hpp"

#include "eHTTPMethod.h"


// simple home page with inja
void HandleHTTPHomeRequest(HTTPReply& reply, std::string body) {
	try {
		inja::Environment env;
		env.set_trim_blocks(true);
		env.set_lstrip_blocks(true);

		nlohmann::json data;
		data["title"] = "Darkflame Universe Dashboard";
		data["header"] = "Welcome to the Darkflame Universe Dashboard";
		data["message"] = "This is a simple dashboard page served using Inja templating engine.";

		const std::string template_str = R"(
		<!DOCTYPE html>
		<html lang="en">
		<head>
			<title>{{ title }}</title>
			<meta charset="UTF-8">
			<meta name="viewport" content="width=device-width, initial-scale=1.0">
		</head>
		<body>
			<h1>{{ header }}</h1>
			<p>{{ message }}</p>
		</body>
		</html>
		)";

		std::string rendered = env.render(template_str, data);
		reply.message = rendered;
		reply.status = eHTTPStatusCode::OK;
		reply.contentType = ContentType::HTML;
	} catch (const std::exception& e) {
		reply.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
		reply.message = "Internal Server Error";
		reply.contentType = ContentType::PLAIN;
	}
}

namespace DashboardWeb {
	void RegisterRoutes() {
		Game::web.RegisterHTTPRoute({
			.path = "/",
			.method = eHTTPMethod::GET,
			.handle = HandleHTTPHomeRequest
		});
	}
}
