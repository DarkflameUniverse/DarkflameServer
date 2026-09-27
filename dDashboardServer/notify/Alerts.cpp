#include "Alerts.h"

#include <chrono>
#include <condition_variable>
#include <ctime>
#include <algorithm>
#include <deque>
#include <mutex>
#include <set>
#include <thread>

#include <curl/curl.h>

#include "Database.h"
#include "EmailService.h"
#include "Logger.h"

namespace {
	constexpr size_t MAX_QUEUE = 200;
	constexpr int MAX_ATTEMPTS = 3;
	constexpr long TIMEOUT_SECONDS = 10;

	struct Job {
		uint32_t webhookId{};
		std::string url;
		std::string body;
		std::vector<std::string> headers;
		std::function<void(std::optional<std::string>)> done;
		int attempt{};
		std::chrono::steady_clock::time_point notBefore{}; // retries wait without holding up other deliveries
	};

	struct Result {
		uint32_t webhookId{};
		int64_t time{};
		int32_t status{};
		std::string error;
		std::function<void(std::optional<std::string>)> done;
	};

	std::thread g_Worker;
	std::mutex g_Mutex;
	std::condition_variable g_Wake;
	std::deque<Job> g_Jobs;
	std::deque<Result> g_Results;
	bool g_Stopping = false;
	bool g_Started = false;

	uint32_t g_LastBugReportId = 0;
	struct LastServerStatus {
		bool auth{};
		bool chat{};
		bool ugcEnabled{};
		bool ugc{};
	};
	std::optional<LastServerStatus> g_LastServerStatus;

	size_t Discard(char*, size_t size, size_t count, void*) { return size * count; }

	size_t ReadHeader(char* data, size_t size, size_t count, void* userData) {
		const std::string_view line(data, size * count);
		auto* retryAfter = static_cast<long*>(userData);
		if (line.size() > 12 && (line.starts_with("retry-after:") || line.starts_with("Retry-After:"))) {
			*retryAfter = std::strtol(std::string(line.substr(12)).c_str(), nullptr, 10);
		}
		return size * count;
	}

	// One POST; returns the HTTP status (0 when the request did not complete)
	int32_t Post(const Job& job, std::string& error, long& retryAfter) {
		CURL* curl = curl_easy_init();
		if (!curl) {
			error = "Could not start an HTTP request";
			return 0;
		}
		curl_slist* headers = curl_slist_append(nullptr, "Content-Type: application/json");
		for (const auto& header : job.headers) headers = curl_slist_append(headers, header.c_str());

		curl_easy_setopt(curl, CURLOPT_URL, job.url.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, job.body.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(job.body.size()));
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT_SECONDS);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "DarkflameServer-Dashboard");
		// Only plain web requests, and no redirects: a webhook URL must not be bounced somewhere else
		curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Discard);
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, ReadHeader);
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, &retryAfter);

		const auto code = curl_easy_perform(curl);
		long status = 0;
		if (code == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
		else error = curl_easy_strerror(code);
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
		if (code == CURLE_OK && (status < 200 || status >= 300)) error = "HTTP " + std::to_string(status);
		return static_cast<int32_t>(status);
	}

	void WorkerLoop() {
		while (true) {
			Job job;
			{
				std::unique_lock lock(g_Mutex);
				while (true) {
					if (g_Stopping && g_Jobs.empty()) return;
					// On shutdown, pending retries are given up rather than waited for
					if (g_Stopping) {
						for (auto& pending : g_Jobs) g_Results.push_back({ pending.webhookId, static_cast<int64_t>(std::time(nullptr)), 0, "Not sent: shutting down", std::move(pending.done) });
						g_Jobs.clear();
						return;
					}
					const auto now = std::chrono::steady_clock::now();
					auto ready = std::ranges::find_if(g_Jobs, [now](const Job& pending) { return pending.notBefore <= now; });
					if (ready != g_Jobs.end()) {
						job = std::move(*ready);
						g_Jobs.erase(ready);
						break;
					}
					if (g_Jobs.empty()) g_Wake.wait(lock);
					else g_Wake.wait_until(lock, std::ranges::min_element(g_Jobs, {}, &Job::notBefore)->notBefore);
				}
			}

			std::string error;
			long retryAfter = 0;
			const auto status = Post(job, error, retryAfter);
			job.attempt++;
			const bool ok = status >= 200 && status < 300;
			// Client errors other than rate limiting will not get better by retrying
			const bool retry = !ok && (status == 0 || status == 429 || status >= 500) && job.attempt < MAX_ATTEMPTS;

			std::lock_guard lock(g_Mutex);
			if (retry) {
				const long wait = status == 429 && retryAfter > 0 ? std::min<long>(retryAfter, 30) : (job.attempt == 1 ? 2 : 10);
				job.notBefore = std::chrono::steady_clock::now() + std::chrono::seconds(wait);
				g_Jobs.push_back(std::move(job));
			} else {
				g_Results.push_back({ job.webhookId, static_cast<int64_t>(std::time(nullptr)), status, error, std::move(job.done) });
			}
		}
	}

	std::string DashboardUrl(const std::string& link) {
		if (link.empty()) return "";
		const auto base = EmailService::BaseUrl();
		return base.empty() ? "" : base + link;
	}

	void Enqueue(const IDashboardAdmin::Webhook& webhook, const WebhookFormat::Alert& alert, std::function<void(std::optional<std::string>)> done = nullptr) {
		Job job;
		job.webhookId = webhook.id;
		job.url = webhook.url;
		job.body = WebhookFormat::BuildBody(webhook.format, alert);
		if (!webhook.secret.empty()) job.headers.push_back("X-DLU-Signature: sha256=" + WebhookFormat::Sign(webhook.secret, job.body));
		job.headers.push_back("X-DLU-Event: " + alert.event);
		job.done = std::move(done);

		{
			std::lock_guard lock(g_Mutex);
			if (!g_Started || g_Jobs.size() >= MAX_QUEUE) {
				LOG("Dropping webhook alert '%s' for %s: %s", alert.title.c_str(), webhook.name.c_str(), g_Started ? "too many queued" : "alerts are not running");
				return;
			}
			g_Jobs.push_back(std::move(job));
		}
		g_Wake.notify_one();
	}

	std::string AccountName(uint32_t accountId) {
		const auto account = Database::Get()->GetAccountById(accountId);
		return account.is_object() ? account.value("name", "account " + std::to_string(accountId)) : "account " + std::to_string(accountId);
	}
}

namespace Alerts {
	void Initialize() {
		if (g_Started) return;
		curl_global_init(CURL_GLOBAL_DEFAULT);
		g_Stopping = false;
		g_Started = true;
		g_Worker = std::thread(WorkerLoop);
		try {
			g_LastBugReportId = Database::Get()->GetMaxBugReportId();
		} catch (const std::exception& ex) {
			LOG("Could not read bug reports for alerts: %s", ex.what());
		}
	}

	void Shutdown() {
		if (!g_Started) return;
		{
			std::lock_guard lock(g_Mutex);
			g_Stopping = true;
		}
		g_Wake.notify_all();
		g_Worker.join();
		Update();
		g_Started = false;
		curl_global_cleanup();
	}

	void Update() {
		std::deque<Result> results;
		{
			std::lock_guard lock(g_Mutex);
			results.swap(g_Results);
		}
		for (auto& result : results) {
			try {
				Database::Get()->RecordWebhookResult(result.webhookId, result.time, result.status, result.error);
			} catch (const std::exception& ex) {
				LOG("Could not record webhook result: %s", ex.what());
			}
			if (!result.error.empty()) LOG("Webhook %u failed: %s", result.webhookId, result.error.c_str());
			if (result.done) result.done(result.error.empty() ? std::nullopt : std::optional<std::string>(result.error));
		}
	}

	void Emit(const std::string& event, const std::string& title, const std::string& description,
		std::vector<WebhookFormat::Field> fields, const std::string& link) {
		std::vector<IDashboardAdmin::Webhook> webhooks;
		try {
			webhooks = Database::Get()->GetWebhooks();
		} catch (const std::exception& ex) {
			LOG("Could not load webhooks: %s", ex.what());
			return;
		}
		const WebhookFormat::Alert alert{ event, title, description, std::move(fields), DashboardUrl(link), static_cast<int64_t>(std::time(nullptr)) };
		for (const auto& webhook : webhooks) {
			if (webhook.enabled && WebhookFormat::Matches(webhook.events, event)) Enqueue(webhook, alert);
		}
	}

	void SendTest(uint32_t webhookId, std::function<void(std::optional<std::string>)> done) {
		const auto webhook = Database::Get()->GetWebhook(webhookId);
		if (!webhook) {
			done("Webhook not found");
			return;
		}
		const WebhookFormat::Alert alert{ "test", "Test from the DarkflameServer dashboard", "If you can read this, the webhook works.",
			{ { "Webhook", webhook->name } }, DashboardUrl("/webhooks"), static_cast<int64_t>(std::time(nullptr)) };
		Enqueue(*webhook, alert, std::move(done));
	}

	void CheckNewBugReports() {
		const auto reports = Database::Get()->GetBugReportsAfter(g_LastBugReportId, 10);
		for (const auto& report : reports) {
			g_LastBugReportId = std::max(g_LastBugReportId, report["id"].get<uint32_t>());
			const auto reporter = report.value("reporter_name", "");
			Emit("bug_report", "Bug report #" + std::to_string(report["id"].get<uint32_t>()), report.value("body", ""),
				{ { "Reporter", reporter.empty() ? report.value("reporter_id", "") : reporter }, { "Client", report.value("client_version", "") },
				  { "Selection", report.value("selection", "") } },
				"/bug_reports/" + std::to_string(report["id"].get<uint32_t>()));
		}
	}

	void PendingName(LWOOBJID characterId) {
		const auto info = Database::Get()->GetCharacterInfo(characterId);
		if (!info || info->pendingName.empty()) return;
		Emit("pending_name", "Name waiting for approval", info->name + " wants to be called " + info->pendingName + ".",
			{ { "Current name", info->name }, { "Requested", info->pendingName } }, "/moderation");
	}

	void FromAudit(const std::string& actor, const std::string& action, const std::string& description) {
		static const std::set<std::string> moderation{
			"ban_account", "unban_account", "lock_account", "unlock_account", "mute_account", "unmute_account",
			"kick_account", "restrict_character", "broadcast_mail", "delete_account", "give_strike", "revoke_strike",
			"warn_account", "action_player_report", "chat_filter_allow", "chat_filter_block", "chat_filter_remove"
		};
		static const std::set<std::string> security{
			"set_gm_level", "create_api_token", "create_api_key", "rotate_api_key", "revoke_api_key", "reset_password", "create_account", "enable_2fa", "disable_2fa",
			"reset_2fa", "regenerate_recovery_codes", "manage_webhook", "change_setting"
		};
		std::string event;
		if (moderation.contains(action)) event = "moderation";
		else if (security.contains(action)) event = "security";
		else return;

		std::string title = action;
		std::replace(title.begin(), title.end(), '_', ' ');
		if (!title.empty()) title[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(title[0])));
		Emit(event, title, description, { { "By", actor } }, "/audit_log");
	}

	void InGameAccountAction(const std::string& action, uint32_t accountId) {
		const auto name = AccountName(accountId);
		Emit("moderation", action == "account_banned" ? "Account banned in game" : "Account muted in game",
			name + (action == "account_banned" ? " was banned" : " was muted") + " with a GM command.", { { "Account", name } },
			"/accounts/" + std::to_string(accountId));
	}

	void ServerStatus(bool authOnline, bool chatOnline, bool ugcEnabled, bool ugcOnline) {
		const LastServerStatus current{ authOnline, chatOnline, ugcEnabled, ugcOnline };
		if (!g_LastServerStatus) {
			g_LastServerStatus = current;
			return;
		}
		const auto previous = *g_LastServerStatus;
		g_LastServerStatus = current;
		if (previous.auth != authOnline) {
			Emit("server", authOnline ? "Auth server is back" : "Auth server went offline",
				authOnline ? "Players can log in again." : "Players cannot log in until it is back.", {}, "/");
		}
		if (previous.chat != chatOnline) {
			Emit("server", chatOnline ? "Chat server is back" : "Chat server went offline",
				chatOnline ? "Chat, friends and teams work again." : "Chat, friends and teams are unavailable.", {}, "/");
		}
		// Turning enable_ugc_server on or off is not an outage
		if (previous.ugcEnabled && ugcEnabled && previous.ugc != ugcOnline) {
			Emit("server", ugcOnline ? "UGC server is back" : "UGC server went offline",
				ugcOnline ? "Player models' meshes and icons are made and downloaded again." : "Game clients cannot download player models' meshes and icons until it is back.", {}, "/ugc");
		}
	}
}
