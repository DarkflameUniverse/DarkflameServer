#include "Scheduler.h"

#include <ctime>
#include <map>

#include "Cron.h"
#include "RouteUtils.h"
#include "WSRoutes.h"
#include "Database.h"
#include "Game.h"
#include "Logger.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	constexpr size_t MAX_LOG_BYTES = 256 * 1024;
	constexpr size_t MAX_SCHEDULE_LENGTH = 128;
	using eRunStatus = IScheduledTasks::eRunStatus;

	struct State {
		Scheduler::Task task;
		std::optional<std::string> customSchedule;
		bool enabled{ true };
		std::optional<Cron::Schedule> schedule; // parsed; nullopt when manual only or invalid
		std::optional<int64_t> nextAt;
		int64_t lastScheduledAt{};
		std::string updatedBy;
		int64_t updatedAt{};
		Scheduler::RunPtr running;
	};

	std::map<std::string, State> g_Tasks; // sorted by name for a stable page
	bool g_Initialized = false;

	int64_t Now() { return static_cast<int64_t>(std::time(nullptr)); }

	const std::string& EffectiveSchedule(const State& state) {
		return state.customSchedule ? *state.customSchedule : state.task.defaultSchedule;
	}

	// Work out the next run from the effective schedule; `from` is the last time it fired (or now)
	void Reschedule(State& state, int64_t from) {
		state.schedule.reset();
		state.nextAt.reset();
		const auto& text = EffectiveSchedule(state);
		if (text.empty()) return;
		std::string error;
		state.schedule = Cron::Parse(text, error);
		if (!state.schedule) {
			LOG("Task %s has an invalid schedule '%s' (%s); it only runs by hand until fixed", state.task.name.c_str(), text.c_str(), error.c_str());
			return;
		}
		if (state.enabled) state.nextAt = Cron::Next(*state.schedule, from);
	}

	void Start(State& state, const std::string& trigger, const std::string& actor) {
		auto run = std::make_shared<Scheduler::Run>(state.task.name, trigger, actor, Now());
		state.running = run;
		LOG("Task %s started (%s%s)", state.task.name.c_str(), trigger.c_str(), actor.empty() ? "" : (" by " + actor).c_str());
		BroadcastTableChanged("scheduled_tasks", state.task.name);
		try {
			state.task.start(run);
		} catch (const std::exception& ex) {
			run->Log(std::string("Error: ") + ex.what());
			run->Finish(false, ex.what());
		}
	}

	nlohmann::json RunJson(const IScheduledTasks::TaskRun& run) {
		return { {"id", run.id}, {"task", run.task}, {"trigger", run.trigger}, {"actor", run.actor}, {"started_at", run.startedAt},
			{"finished_at", run.finishedAt}, {"status", static_cast<uint32_t>(run.status)}, {"summary", run.summary} };
	}

	nlohmann::json TaskJson(const State& state, const std::map<std::string, IScheduledTasks::TaskRun>& latest) {
		nlohmann::json json{
			{"name", state.task.name}, {"title", state.task.title}, {"description", state.task.description},
			{"default_schedule", state.task.defaultSchedule}, {"schedule", EffectiveSchedule(state)}, {"custom", state.customSchedule.has_value()},
			{"valid", EffectiveSchedule(state).empty() || state.schedule.has_value()},
			{"enabled", state.enabled}, {"default_enabled", state.task.defaultEnabled}, {"next_at", state.nextAt ? nlohmann::json(*state.nextAt) : nlohmann::json(nullptr)},
			{"updated_by", state.updatedBy}, {"updated_at", state.updatedAt}, {"running", nullptr}, {"last_run", nullptr}
		};
		if (state.running) {
			json["running"] = { {"trigger", state.running->Trigger()}, {"actor", state.running->Actor()}, {"started_at", state.running->StartedAt()} };
		}
		if (const auto it = latest.find(state.task.name); it != latest.end()) json["last_run"] = RunJson(it->second);
		return json;
	}

	std::optional<std::string> ValidateSchedule(const std::string& text) {
		if (text.size() > MAX_SCHEDULE_LENGTH) return "The schedule is too long";
		std::string error;
		if (!Cron::Parse(text, error)) return error;
		return std::nullopt;
	}
}

namespace Scheduler {
	void Run::Log(const std::string& line) {
		if (m_Finished || m_LogFull) return;
		const auto now = Now();
		char stamp[16];
		std::strftime(stamp, sizeof(stamp), "%H:%M:%S ", std::gmtime(&now));
		if (m_Log.size() + line.size() > MAX_LOG_BYTES) {
			m_Log += stamp + std::string("(log cut off: too long)\n");
			m_LogFull = true;
			return;
		}
		m_Log += stamp + line + "\n";
	}

	void Run::Finish(bool success, const std::string& summary) {
		End(static_cast<uint8_t>(success ? eRunStatus::SUCCEEDED : eRunStatus::FAILED), summary);
	}

	void Run::End(uint8_t status, const std::string& summary) {
		if (m_Finished) return;
		IScheduledTasks::TaskRun record{ 0, m_Task, m_Trigger, m_Actor, m_StartedAt, Now(), static_cast<eRunStatus>(status), summary, "" };
		if (!summary.empty()) Log(summary);
		m_Finished = true;
		record.log = m_Log;
		LOG("Task %s %s: %s", m_Task.c_str(), status == static_cast<uint8_t>(eRunStatus::SUCCEEDED) ? "finished" : "failed", summary.c_str());
		try {
			Database::Get()->InsertTaskRun(record);
		} catch (const std::exception& ex) {
			LOG("Could not record the run of task %s: %s", m_Task.c_str(), ex.what());
		}
		const auto task = m_Task;
		BroadcastTableChanged("scheduled_tasks", task);
		// Last: this may drop the final reference to the run
		if (const auto it = g_Tasks.find(task); it != g_Tasks.end() && it->second.running.get() == this) it->second.running.reset();
	}

	void Register(Task task) {
		auto name = task.name;
		const bool enabled = task.defaultEnabled;
		g_Tasks[name] = State{ std::move(task) };
		g_Tasks[name].enabled = enabled;
	}

	void Initialize() {
		std::vector<IScheduledTasks::TaskSettings> saved;
		try {
			saved = Database::Get()->GetScheduledTasks();
		} catch (const std::exception& ex) {
			LOG("Could not load task schedules, using defaults: %s", ex.what());
		}
		const auto now = Now();
		for (auto& [name, state] : g_Tasks) {
			const auto row = std::ranges::find(saved, name, &IScheduledTasks::TaskSettings::name);
			if (row != saved.end()) {
				state.customSchedule = row->schedule;
				state.enabled = row->enabled;
				state.lastScheduledAt = row->lastScheduledAt;
				state.updatedBy = row->updatedBy;
				state.updatedAt = row->updatedAt;
			}
			// A run missed while the dashboard was down is due straight away (once)
			Reschedule(state, state.lastScheduledAt > 0 ? state.lastScheduledAt : now);
		}
		g_Initialized = true;
	}

	void Update() {
		if (!g_Initialized) return;
		const auto now = Now();
		for (auto& [name, state] : g_Tasks) {
			if (state.running && now - state.running->StartedAt() > state.task.timeoutSeconds) {
				state.running->Log("Timed out after " + std::to_string(state.task.timeoutSeconds / 60) + " minutes; a late result is ignored");
				state.running->End(static_cast<uint8_t>(eRunStatus::TIMED_OUT), "Timed out");
			}
			if (!state.nextAt || *state.nextAt > now) continue;

			const auto due = *state.nextAt;
			state.lastScheduledAt = due;
			// From now, so a long outage runs the task once rather than once per missed slot
			state.nextAt = state.schedule ? Cron::Next(*state.schedule, std::max(due, now)) : std::nullopt;
			try {
				Database::Get()->SetTaskLastScheduled(name, due);
			} catch (const std::exception& ex) {
				LOG("Could not save when task %s ran: %s", name.c_str(), ex.what());
			}
			if (state.running) {
				LOG("Task %s is still running; skipping this scheduled run", name.c_str());
				continue;
			}
			Start(state, "schedule", "");
		}
	}

	bool RunNow(const std::string& name, const std::string& actor) {
		const auto it = g_Tasks.find(name);
		if (it == g_Tasks.end() || it->second.running) return false;
		Start(it->second, "manual", actor);
		return true;
	}

	void RegisterRoutes() {
		Route(eHTTPMethod::GET, "/api/tasks", Perm("tasks_view"), "Periodic tasks: schedule, next run, whether running, and the last run",
			[](HTTPReply& reply, const HTTPContext&) {
				std::map<std::string, IScheduledTasks::TaskRun> latest;
				for (auto& run : Database::Get()->GetLatestTaskRuns()) latest[run.task] = std::move(run);
				nlohmann::json tasks = nlohmann::json::array();
				for (const auto& [name, state] : g_Tasks) tasks.push_back(TaskJson(state, latest));
				JsonReply(reply, eHTTPStatusCode::OK, { {"success", true}, {"tasks", tasks}, {"now", Now()} });
			});

		Route(eHTTPMethod::POST, "/api/tasks/preview", Perm("tasks_view"), "Check a schedule and list its next runs. Body: {schedule}. Returns {valid, error, next: [unix times]}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const std::string text = body->value("schedule", "");
				if (const auto error = ValidateSchedule(text)) return JsonSuccess(reply, { {"valid", false}, {"error", *error} });
				const auto schedule = Cron::Parse(text);
				nlohmann::json next = nlohmann::json::array();
				auto at = Now();
				for (int i = 0; i < 5; i++) {
					const auto time = Cron::Next(*schedule, at);
					if (!time) break;
					next.push_back(*time);
					at = *time;
				}
				JsonSuccess(reply, { {"valid", true}, {"next", next} });
			});

		Route(eHTTPMethod::POST, "/api/tasks/runs", Perm("tasks_view"), "Task runs, newest first (DataTables). Body adds {task} to show one task",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto request = ParseDataTablesRequest(context.body);
				const auto body = ParseBody(context);
				if (!request || !body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				auto response = Database::Get()->GetTaskRunsTable(body->value("task", ""), request->start, std::min<uint32_t>(request->length, 200));
				response["draw"] = request->draw;
				JsonReply(reply, eHTTPStatusCode::OK, response);
			});

		Route(eHTTPMethod::GET, "/api/tasks/runs/:id", Perm("tasks_view"), "One finished run with its log",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto id = PathId<uint64_t>(context.path, 3);
				if (!id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid run id");
				const auto run = Database::Get()->GetTaskRun(*id);
				if (!run) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Run not found");
				auto json = RunJson(*run);
				json["log"] = run->log;
				JsonSuccess(reply, { {"run", json} });
			});

		Route(eHTTPMethod::GET, "/api/tasks/:name/log", Perm("tasks_view"), "The log so far of a task that is running now",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto it = g_Tasks.find(std::string(PathSegment(context.path, 2)));
				if (it == g_Tasks.end()) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Unknown task");
				if (!it->second.running) return JsonSuccess(reply, { {"running", false} });
				const auto& run = *it->second.running;
				JsonSuccess(reply, { {"running", true}, {"trigger", run.Trigger()}, {"actor", run.Actor()}, {"started_at", run.StartedAt()}, {"log", run.Text()} });
			});

		Route(eHTTPMethod::POST, "/api/tasks/:name", Perm("tasks_manage"), "Change a task. Body: {schedule (empty or missing: the default), enabled}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto it = g_Tasks.find(std::string(PathSegment(context.path, 2)));
				const auto body = ParseBody(context);
				if (it == g_Tasks.end()) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Unknown task");
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				auto& state = it->second;
				std::string text = body->value("schedule", "");
				text.erase(0, text.find_first_not_of(" \t"));
				text.erase(text.find_last_not_of(" \t") + 1);
				if (text == state.task.defaultSchedule) text.clear();
				if (!text.empty()) {
					if (const auto error = ValidateSchedule(text)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, *error);
				}
				const bool enabled = body->value("enabled", state.enabled);
				const auto custom = text.empty() ? std::nullopt : std::optional<std::string>(text);
				Database::Get()->SetScheduledTask(state.task.name, custom, enabled, context.authenticatedUser);
				state.customSchedule = custom;
				state.enabled = enabled;
				state.updatedBy = context.authenticatedUser;
				state.updatedAt = Now();
				Reschedule(state, Now());
				Audit(context, "update_task", state.task.name + ": " + (enabled ? "on" : "off") + ", schedule " +
					(custom ? *custom : "default (" + (state.task.defaultSchedule.empty() ? std::string("manual only") : state.task.defaultSchedule) + ")"));
				BroadcastTableChanged("scheduled_tasks", state.task.name);
				JsonSuccess(reply, { {"message", "Saved"} });
			});

		Route(eHTTPMethod::POST, "/api/tasks/:name/run", Perm("tasks_manage"), "Run a task now, even when it is switched off",
			[](HTTPReply& reply, const HTTPContext& context) {
				const std::string name(PathSegment(context.path, 2));
				if (!g_Tasks.contains(name)) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Unknown task");
				if (!RunNow(name, context.authenticatedUser)) return JsonError(reply, eHTTPStatusCode::CONFLICT, "It is already running");
				Audit(context, "run_task", name);
				JsonSuccess(reply, { {"message", "Started"} });
			});
	}
}
