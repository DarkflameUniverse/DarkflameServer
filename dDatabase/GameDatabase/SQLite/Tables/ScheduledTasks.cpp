#include "SQLiteDatabase.h"

#include <ctime>

namespace {
	std::string Text(CppSQLite3Query& result, const char* field) {
		return result.fieldIsNull(field) ? "" : result.getStringField(field);
	}

	IScheduledTasks::TaskRun RunRow(CppSQLite3Query& result, bool withLog) {
		IScheduledTasks::TaskRun run;
		run.id = static_cast<uint64_t>(result.getInt64Field("id"));
		run.task = Text(result, "task");
		run.trigger = Text(result, "task_trigger");
		run.actor = Text(result, "actor");
		run.startedAt = result.getInt64Field("started_at");
		run.finishedAt = result.getInt64Field("finished_at");
		run.status = static_cast<IScheduledTasks::eRunStatus>(result.getIntField("status"));
		run.summary = Text(result, "summary");
		if (withLog) run.log = Text(result, "log");
		return run;
	}
}

std::vector<IScheduledTasks::TaskSettings> SQLiteDatabase::GetScheduledTasks() {
	std::vector<TaskSettings> tasks;
	auto [_, result] = ExecuteSelect("SELECT * FROM scheduled_tasks ORDER BY name;");
	while (!result.eof()) {
		TaskSettings task;
		task.name = Text(result, "name");
		if (!result.fieldIsNull("schedule")) task.schedule = result.getStringField("schedule");
		task.enabled = result.getIntField("enabled") != 0;
		task.lastScheduledAt = result.getInt64Field("last_scheduled_at");
		task.updatedAt = result.getInt64Field("updated_at");
		task.updatedBy = Text(result, "updated_by");
		tasks.push_back(std::move(task));
		result.nextRow();
	}
	return tasks;
}

void SQLiteDatabase::SetScheduledTask(const std::string& name, const std::optional<std::string>& schedule, bool enabled, const std::string& by) {
	ExecuteInsert(
		"INSERT INTO scheduled_tasks (name, schedule, enabled, updated_at, updated_by) VALUES (?, ?, ?, ?, ?) "
		"ON CONFLICT(name) DO UPDATE SET schedule = excluded.schedule, enabled = excluded.enabled, updated_at = excluded.updated_at, updated_by = excluded.updated_by;",
		name, schedule, enabled, static_cast<int64_t>(std::time(nullptr)), by);
}

void SQLiteDatabase::SetTaskLastScheduled(const std::string& name, int64_t time) {
	ExecuteInsert("INSERT INTO scheduled_tasks (name, last_scheduled_at) VALUES (?, ?) ON CONFLICT(name) DO UPDATE SET last_scheduled_at = excluded.last_scheduled_at;",
		name, time);
}

void SQLiteDatabase::InsertTaskRun(const TaskRun& run) {
	ExecuteInsert("INSERT INTO scheduled_task_runs (task, task_trigger, actor, started_at, finished_at, status, summary, log) VALUES (?, ?, ?, ?, ?, ?, ?, ?);",
		run.task, run.trigger, run.actor, run.startedAt, run.finishedAt, static_cast<uint32_t>(run.status), run.summary, run.log);
}

nlohmann::json SQLiteDatabase::GetTaskRunsTable(const std::string& task, uint32_t start, uint32_t length) {
	const std::string where = task.empty() ? "" : " WHERE task = ?";
	const auto count = [&]() {
		auto [_, result] = task.empty() ? ExecuteSelect("SELECT COUNT(*) AS count FROM scheduled_task_runs;")
			: ExecuteSelect("SELECT COUNT(*) AS count FROM scheduled_task_runs" + where + ";", task);
		return result.eof() ? 0 : result.getIntField("count");
	}();
	const std::string query = "SELECT id, task, task_trigger, actor, started_at, finished_at, status, summary FROM scheduled_task_runs" + where + " ORDER BY id DESC LIMIT ? OFFSET ?;";
	auto [_, result] = task.empty() ? ExecuteSelect(query, length, start) : ExecuteSelect(query, task, length, start);
	nlohmann::json data = nlohmann::json::array();
	while (!result.eof()) {
		const auto run = RunRow(result, false);
		data.push_back({ {"id", run.id}, {"task", run.task}, {"trigger", run.trigger}, {"actor", run.actor}, {"started_at", run.startedAt},
			{"finished_at", run.finishedAt}, {"status", static_cast<uint32_t>(run.status)}, {"summary", run.summary} });
		result.nextRow();
	}
	return { {"draw", 0}, {"recordsTotal", count}, {"recordsFiltered", count}, {"data", data} };
}

std::optional<IScheduledTasks::TaskRun> SQLiteDatabase::GetTaskRun(uint64_t id) {
	auto [_, result] = ExecuteSelect("SELECT * FROM scheduled_task_runs WHERE id = ?;", static_cast<int64_t>(id));
	if (result.eof()) return std::nullopt;
	return RunRow(result, true);
}

std::vector<IScheduledTasks::TaskRun> SQLiteDatabase::GetLatestTaskRuns() {
	std::vector<TaskRun> runs;
	auto [_, result] = ExecuteSelect(
		"SELECT r.id, r.task, r.task_trigger, r.actor, r.started_at, r.finished_at, r.status, r.summary FROM scheduled_task_runs r "
		"JOIN (SELECT task, MAX(id) AS id FROM scheduled_task_runs GROUP BY task) latest ON latest.id = r.id ORDER BY r.task;");
	while (!result.eof()) {
		runs.push_back(RunRow(result, false));
		result.nextRow();
	}
	return runs;
}

uint32_t SQLiteDatabase::PruneTaskRuns(int64_t beforeTime) {
	return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM scheduled_task_runs WHERE finished_at < ?;", beforeTime));
}
