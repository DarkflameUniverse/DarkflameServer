#pragma once

#include <functional>
#include <string>

#include "json.hpp"

class GameDatabase;

/**
 * Runs slow database work (scans over every character, nightly jobs) on a worker thread with its own database
 * connection, so the dashboard keeps answering while it runs. Tasks must only use the connection they are given:
 * no Database::Get(), no game data (item names) and no config; do that in the completion, which runs on the main
 * thread from Update().
 */
namespace Background {
	using Task = std::function<nlohmann::json(GameDatabase& db)>;
	// error is empty on success
	using Completion = std::function<void(nlohmann::json result, const std::string& error)>;

	void Initialize();
	void Shutdown();

	// Main loop: run completions of finished tasks
	void Update();

	// Queue a task. `name` identifies work that should not run twice at once; returns false if it is already queued or running.
	bool Run(const std::string& name, Task task, Completion done);

	bool IsRunning(const std::string& name);
}
