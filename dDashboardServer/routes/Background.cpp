#include "Background.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <set>
#include <thread>

#include "Database.h"
#include "Logger.h"

namespace {
	struct Job {
		std::string name;
		Background::Task task;
		Background::Completion done;
	};

	struct Finished {
		std::string name;
		Background::Completion done;
		nlohmann::json result;
		std::string error;
	};

	std::thread g_Worker;
	std::mutex g_Mutex;
	std::condition_variable g_Wake;
	std::deque<Job> g_Jobs;
	std::deque<Finished> g_Finished;
	std::set<std::string> g_Active; // queued or running, main thread only
	bool g_Stopping = false;
	std::unique_ptr<GameDatabase> g_Connection;

	void WorkerLoop() {
		while (true) {
			Job job;
			{
				std::unique_lock lock(g_Mutex);
				g_Wake.wait(lock, [] { return g_Stopping || !g_Jobs.empty(); });
				if (g_Stopping) return;
				job = std::move(g_Jobs.front());
				g_Jobs.pop_front();
			}

			Finished finished{ job.name, std::move(job.done), nullptr, "" };
			try {
				finished.result = job.task(*g_Connection);
			} catch (const std::exception& ex) {
				finished.error = ex.what();
			}

			std::lock_guard lock(g_Mutex);
			g_Finished.push_back(std::move(finished));
		}
	}
}

namespace Background {
	void Initialize() {
		if (g_Worker.joinable()) return;
		try {
			// Connecting reads the config, so do it here on the main thread
			g_Connection = Database::CreateConnection();
		} catch (const std::exception& ex) {
			LOG("Could not open a second database connection for background work: %s", ex.what());
			return;
		}
		g_Stopping = false;
		g_Worker = std::thread(WorkerLoop);
	}

	void Shutdown() {
		if (!g_Worker.joinable()) return;
		{
			std::lock_guard lock(g_Mutex);
			g_Stopping = true;
		}
		g_Wake.notify_all();
		// A task in progress finishes first; queued ones are dropped
		g_Worker.join();
		g_Jobs.clear();
		if (g_Connection) g_Connection->Destroy("DashboardServer background");
		g_Connection.reset();
	}

	void Update() {
		std::deque<Finished> finished;
		{
			std::lock_guard lock(g_Mutex);
			finished.swap(g_Finished);
		}
		for (auto& item : finished) {
			g_Active.erase(item.name);
			if (!item.error.empty()) LOG("Background task %s failed: %s", item.name.c_str(), item.error.c_str());
			try {
				if (item.done) item.done(std::move(item.result), item.error);
			} catch (const std::exception& ex) {
				LOG("Background task %s completion failed: %s", item.name.c_str(), ex.what());
			}
		}
	}

	bool Run(const std::string& name, Task task, Completion done) {
		if (g_Active.contains(name)) return false;
		if (!g_Worker.joinable()) {
			// No worker (second connection failed): run inline so the feature still works, just slower
			nlohmann::json result;
			std::string error;
			try {
				result = task(*Database::Get());
			} catch (const std::exception& ex) {
				error = ex.what();
			}
			if (done) done(std::move(result), error);
			return true;
		}
		g_Active.insert(name);
		{
			std::lock_guard lock(g_Mutex);
			g_Jobs.push_back({ name, std::move(task), std::move(done) });
		}
		g_Wake.notify_one();
		return true;
	}

	bool IsRunning(const std::string& name) {
		return g_Active.contains(name);
	}
}
