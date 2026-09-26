#ifndef __ISCHEDULEDTASKS__H__
#define __ISCHEDULEDTASKS__H__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "json.hpp"

/**
 * Periodic dashboard tasks (economy checks, pruning, ...): their schedule and on/off switch as changed on the dashboard,
 * and a history of runs with their logs. Tasks without a row use their built-in schedule.
 */
class IScheduledTasks {
public:
	struct TaskSettings {
		std::string name;
		std::optional<std::string> schedule; // nullopt: the task's default
		bool enabled{ true };
		int64_t lastScheduledAt{};           // the last time the schedule fired (for catching up after downtime)
		int64_t updatedAt{};
		std::string updatedBy;
	};

	enum class eRunStatus : uint8_t { SUCCEEDED = 1, FAILED = 2, TIMED_OUT = 3, INTERRUPTED = 4 };

	struct TaskRun {
		uint64_t id{};
		std::string task;
		std::string trigger; // "schedule" or "manual"
		std::string actor;   // who started a manual run
		int64_t startedAt{};
		int64_t finishedAt{};
		eRunStatus status{ eRunStatus::SUCCEEDED };
		std::string summary;
		std::string log;
	};

	virtual std::vector<TaskSettings> GetScheduledTasks() = 0;

	// Change the schedule (nullopt: back to the default) and on/off switch
	virtual void SetScheduledTask(const std::string& name, const std::optional<std::string>& schedule, bool enabled, const std::string& by) = 0;

	virtual void SetTaskLastScheduled(const std::string& name, int64_t time) = 0;

	// Runs are written once they finish
	virtual void InsertTaskRun(const TaskRun& run) = 0;

	// Newest first, without logs; empty task means every task. DataTables shape.
	virtual nlohmann::json GetTaskRunsTable(const std::string& task, uint32_t start, uint32_t length) = 0;

	virtual std::optional<TaskRun> GetTaskRun(uint64_t id) = 0;

	// The latest finished run of every task
	virtual std::vector<TaskRun> GetLatestTaskRuns() = 0;

	virtual uint32_t PruneTaskRuns(int64_t beforeTime) = 0;
};

#endif  //!__ISCHEDULEDTASKS__H__
