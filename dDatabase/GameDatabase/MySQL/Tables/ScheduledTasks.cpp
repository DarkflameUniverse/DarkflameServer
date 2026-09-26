#include "MySQLDatabase.h"

#include <ctime>

namespace {
	std::string Text(PreparedStmtResultSet& result, const char* field) {
		return result->isNull(field) ? "" : std::string(result->getString(field).c_str());
	}

	IScheduledTasks::TaskRun RunRow(PreparedStmtResultSet& result, bool withLog) {
		IScheduledTasks::TaskRun run;
		run.id = result->getUInt64("id");
		run.task = Text(result, "task");
		run.trigger = Text(result, "task_trigger");
		run.actor = Text(result, "actor");
		run.startedAt = result->getInt64("started_at");
		run.finishedAt = result->getInt64("finished_at");
		run.status = static_cast<IScheduledTasks::eRunStatus>(result->getInt("status"));
		run.summary = Text(result, "summary");
		if (withLog) run.log = Text(result, "log");
		return run;
	}
}

std::vector<IScheduledTasks::TaskSettings> MySQLDatabase::GetScheduledTasks() {
	std::vector<TaskSettings> tasks;
	auto result = ExecuteSelect("SELECT * FROM scheduled_tasks ORDER BY name;");
	while (result->next()) {
		TaskSettings task;
		task.name = Text(result, "name");
		if (!result->isNull("schedule")) task.schedule = Text(result, "schedule");
		task.enabled = result->getInt("enabled") != 0;
		task.lastScheduledAt = result->getInt64("last_scheduled_at");
		task.updatedAt = result->getInt64("updated_at");
		task.updatedBy = Text(result, "updated_by");
		tasks.push_back(std::move(task));
	}
	return tasks;
}

void MySQLDatabase::SetScheduledTask(const std::string& name, const std::optional<std::string>& schedule, bool enabled, const std::string& by) {
	ExecuteInsert(
		"INSERT INTO scheduled_tasks (name, schedule, enabled, updated_at, updated_by) VALUES (?, ?, ?, ?, ?) "
		"ON DUPLICATE KEY UPDATE schedule = VALUES(schedule), enabled = VALUES(enabled), updated_at = VALUES(updated_at), updated_by = VALUES(updated_by);",
		name, schedule, enabled, static_cast<int64_t>(std::time(nullptr)), by);
}

void MySQLDatabase::SetTaskLastScheduled(const std::string& name, int64_t time) {
	ExecuteInsert("INSERT INTO scheduled_tasks (name, last_scheduled_at) VALUES (?, ?) ON DUPLICATE KEY UPDATE last_scheduled_at = VALUES(last_scheduled_at);",
		name, time);
}

void MySQLDatabase::InsertTaskRun(const TaskRun& run) {
	ExecuteInsert("INSERT INTO scheduled_task_runs (task, task_trigger, actor, started_at, finished_at, status, summary, log) VALUES (?, ?, ?, ?, ?, ?, ?, ?);",
		run.task, run.trigger, run.actor, run.startedAt, run.finishedAt, static_cast<uint32_t>(run.status), run.summary, run.log);
}

nlohmann::json MySQLDatabase::GetTaskRunsTable(const std::string& task, uint32_t start, uint32_t length) {
	const std::string where = task.empty() ? "" : " WHERE task = ?";
	uint32_t count = 0;
	{
		auto result = task.empty() ? ExecuteSelect("SELECT COUNT(*) AS count FROM scheduled_task_runs;")
			: ExecuteSelect("SELECT COUNT(*) AS count FROM scheduled_task_runs" + where + ";", task);
		if (result->next()) count = result->getUInt("count");
	}
	const std::string query = "SELECT id, task, task_trigger, actor, started_at, finished_at, status, summary FROM scheduled_task_runs" + where + " ORDER BY id DESC LIMIT ? OFFSET ?;";
	auto result = task.empty() ? ExecuteSelect(query, length, start) : ExecuteSelect(query, task, length, start);
	nlohmann::json data = nlohmann::json::array();
	while (result->next()) {
		const auto run = RunRow(result, false);
		data.push_back({ {"id", run.id}, {"task", run.task}, {"trigger", run.trigger}, {"actor", run.actor}, {"started_at", run.startedAt},
			{"finished_at", run.finishedAt}, {"status", static_cast<uint32_t>(run.status)}, {"summary", run.summary} });
	}
	return { {"draw", 0}, {"recordsTotal", count}, {"recordsFiltered", count}, {"data", data} };
}

std::optional<IScheduledTasks::TaskRun> MySQLDatabase::GetTaskRun(uint64_t id) {
	auto result = ExecuteSelect("SELECT * FROM scheduled_task_runs WHERE id = ?;", static_cast<int64_t>(id));
	if (!result->next()) return std::nullopt;
	return RunRow(result, true);
}

std::vector<IScheduledTasks::TaskRun> MySQLDatabase::GetLatestTaskRuns() {
	std::vector<TaskRun> runs;
	auto result = ExecuteSelect(
		"SELECT r.id, r.task, r.task_trigger, r.actor, r.started_at, r.finished_at, r.status, r.summary FROM scheduled_task_runs r "
		"JOIN (SELECT task, MAX(id) AS id FROM scheduled_task_runs GROUP BY task) latest ON latest.id = r.id ORDER BY r.task;");
	while (result->next()) runs.push_back(RunRow(result, false));
	return runs;
}

uint32_t MySQLDatabase::PruneTaskRuns(int64_t beforeTime) {
	return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM scheduled_task_runs WHERE finished_at < ?;", beforeTime));
}
