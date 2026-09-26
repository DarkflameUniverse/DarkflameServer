#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

/**
 * Periodic dashboard tasks (economy checks, ledger compaction, log pruning, ...). Each task has a built-in schedule
 * that GM 9 can change or switch off on the Tasks page, and can be run by hand. Every run is recorded with a log.
 * Schedules are cron expressions in UTC (see Cron.h). A schedule missed while the dashboard was down runs once on start.
 */
namespace Scheduler {
	// One run of a task. Tasks may finish later (after background work); log and finish from the main thread.
	class Run {
	public:
		Run(std::string task, std::string trigger, std::string actor, int64_t startedAt)
			: m_Task(std::move(task)), m_Trigger(std::move(trigger)), m_Actor(std::move(actor)), m_StartedAt(startedAt) {}

		void Log(const std::string& line);
		// Only the first call counts; later ones (after a time out) are ignored
		void Finish(bool success, const std::string& summary);

		bool Manual() const { return m_Trigger == "manual"; }
		bool Finished() const { return m_Finished; }
		const std::string& Task() const { return m_Task; }
		const std::string& Trigger() const { return m_Trigger; }
		const std::string& Actor() const { return m_Actor; }
		int64_t StartedAt() const { return m_StartedAt; }
		const std::string& Text() const { return m_Log; }

	private:
		friend void Update();
		void End(uint8_t status, const std::string& summary);

		std::string m_Task;
		std::string m_Trigger;
		std::string m_Actor;
		int64_t m_StartedAt;
		std::string m_Log;
		bool m_Finished{};
		bool m_LogFull{};
	};
	using RunPtr = std::shared_ptr<Run>;

	struct Task {
		std::string name;            // stable id, lower_snake_case
		std::string title;
		std::string description;
		std::string defaultSchedule; // cron; empty means manual only unless a schedule is set
		std::function<void(RunPtr)> start;
		int64_t timeoutSeconds{ 2 * 60 * 60 };
		bool defaultEnabled{ true }; // off: only runs by hand until someone switches it on
	};

	// Before Initialize
	void Register(Task task);

	// Load schedules and switches from the database
	void Initialize();

	// Main loop: start due tasks, time out stuck ones
	void Update();

	// Start a task now; false if it is unknown or already running
	bool RunNow(const std::string& name, const std::string& actor);

	void RegisterRoutes();
}
