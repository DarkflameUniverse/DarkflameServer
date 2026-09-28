#pragma once

#include <optional>
#include <string>

// The UGC page: what the UGC server has made of players' models and modular builds, and making them again
namespace UgcRoutes {
	void RegisterRoutes();

	// Reads the client data the UGC page needs (modules, build types) once, on the main thread at startup, so the web
	// threads never query the CDClient
	void Preload();

	// Web thread: a ugcconfig.ini setting's value as the UGC server sees it (dashboard value, file value, default)
	std::optional<std::string> Setting(const std::string& name);
}
