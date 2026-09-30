#include "UpdateChecker.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <map>
#include <mutex>
#include <optional>
#include <thread>

#include <curl/curl.h>

#include "UpdateCheck.h"
#include "RouteUtils.h"
#include "WSRoutes.h"
#include "MasterPackets.h"
#include "master/UpdateStatus.h"
#include "Game.h"
#include "dConfig.h"
#include "Logger.h"
#include "GeneralUtils.h"
#include "eHTTPMethod.h"
#include "json.hpp"

using namespace RouteUtils;
using Clock = std::chrono::steady_clock;

namespace {
	constexpr auto FIRST_CHECK_DELAY = std::chrono::seconds(15);
	constexpr auto DISABLED_RECHECK = std::chrono::minutes(1); // noticing the setting was turned on
	constexpr auto MANUAL_SPACING = std::chrono::minutes(1);
	constexpr long TIMEOUT_SECONDS = 20;
	constexpr size_t MAX_BODY = 4 * 1024 * 1024;
	const std::string API = "https://api.github.com/repos/";
	const std::string DEFAULT_REPO = "DarkflameUniverse/DarkflameServer";
	const std::string DEFAULT_BRANCH = "main";

	// ---- Settings: read on the main thread only ----

	struct Settings {
		bool enabled{ true };
		std::string repo{ DEFAULT_REPO };
		uint32_t intervalHours{ 6 };
	};

	Settings ReadSettings() {
		Settings s;
		s.enabled = Game::config->GetValue("update_check_enabled") != "0";
		const auto& repo = Game::config->GetValue("update_check_repo");
		if (!repo.empty()) s.repo = repo;
		s.intervalHours = std::clamp(GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("update_check_interval_hours")).value_or(6), 1u, 168u);
		return s;
	}

	// ---- The worker ----

	struct Job {
		std::string repo;
		UpdateCheck::Build build;
	};

	struct Outcome {
		std::string repo;
		UpdateCheck::Status status;
		bool releaseFailed{}; // couldn't ask (offline, rate limited): keep the last answer
		bool comparisonFailed{};
		int64_t retryAfter{}; // unix time GitHub's rate limit resets, when it ran out
	};

	struct Response {
		long status{}; // 0: no answer
		std::string body;
		std::string error; // transport error
		std::string etag;
		std::optional<int64_t> rateRemaining;
		int64_t rateReset{};
	};

	struct Cached {
		std::string etag;
		std::string body;
	};

	std::thread g_Worker;
	std::mutex g_Mutex;
	std::condition_variable g_Wake;
	std::optional<Job> g_Job;
	std::optional<Outcome> g_Done;
	bool g_Stopping = false;
	std::atomic<bool> g_Abort = false;
	bool g_Started = false;
	std::map<std::string, Cached> g_Cache; // url -> last 200 answer; the worker's alone

	size_t WriteBody(char* data, size_t size, size_t count, void* userData) {
		auto* body = static_cast<std::string*>(userData);
		if (body->size() + size * count > MAX_BODY) return 0;
		body->append(data, size * count);
		return size * count;
	}

	std::string_view Trim(std::string_view text) {
		while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
		while (!text.empty() && (text.back() == '\r' || text.back() == '\n' || text.back() == ' ')) text.remove_suffix(1);
		return text;
	}

	size_t ReadHeader(char* data, size_t size, size_t count, void* userData) {
		auto* response = static_cast<Response*>(userData);
		const std::string_view line(data, size * count);
		const auto colon = line.find(':');
		if (colon == std::string_view::npos) return size * count;
		std::string name(line.substr(0, colon));
		std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		const auto value = std::string(Trim(line.substr(colon + 1)));
		if (name == "etag") response->etag = value;
		else if (name == "x-ratelimit-remaining") response->rateRemaining = GeneralUtils::TryParse<int64_t>(value);
		else if (name == "x-ratelimit-reset") response->rateReset = GeneralUtils::TryParse<int64_t>(value).value_or(0);
		return size * count;
	}

	int Progress(void*, curl_off_t, curl_off_t, curl_off_t, curl_off_t) { return g_Abort ? 1 : 0; }

	// GET with the cached ETag; a 304 comes back as the cached 200
	Response Get(const std::string& url) {
		Response response;
		CURL* curl = curl_easy_init();
		if (!curl) {
			response.error = "Could not start an HTTP request";
			return response;
		}
		const auto cached = g_Cache.find(url);
		curl_slist* headers = curl_slist_append(nullptr, "Accept: application/vnd.github+json");
		headers = curl_slist_append(headers, "X-GitHub-Api-Version: 2022-11-28");
		if (cached != g_Cache.end() && !cached->second.etag.empty()) headers = curl_slist_append(headers, ("If-None-Match: " + cached->second.etag).c_str());

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT_SECONDS);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "DarkflameServer-Dashboard");
		curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
		curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, ""); // whatever curl can decompress
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteBody);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, ReadHeader);
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, Progress);

		const auto code = curl_easy_perform(curl);
		if (code == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
		else response.error = curl_easy_strerror(code);
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);

		if (response.status == 304 && cached != g_Cache.end()) {
			response.status = 200;
			response.body = cached->second.body;
		} else if (response.status == 200) {
			if (!response.etag.empty()) g_Cache[url] = { response.etag, response.body };
			else g_Cache.erase(url);
		}
		return response;
	}

	bool RateLimited(const Response& response) {
		return response.status == 429 || (response.status == 403 && response.rateRemaining == 0);
	}

	// Why a request didn't give an answer, for people
	std::string Problem(const Response& response, const std::string& what) {
		if (response.status == 0) return "Couldn't reach GitHub for " + what + ": " + response.error;
		if (RateLimited(response)) return "GitHub's rate limit for this address is used up; checking again after it resets";
		const auto message = UpdateCheck::ErrorMessage(response.body);
		return "GitHub answered HTTP " + std::to_string(response.status) + " for " + what + (message.empty() ? "" : ": " + message);
	}

	Outcome Run(const Job& job) {
		Outcome outcome;
		outcome.repo = job.repo;
		auto& status = outcome.status;
		std::vector<std::string> errors;
		const auto note = [&](const Response& response, const std::string& what) {
			errors.push_back(Problem(response, what));
			if (RateLimited(response)) outcome.retryAfter = response.rateReset;
		};

		const auto releases = Get(API + job.repo + "/releases/latest");
		if (releases.status == 200) {
			std::string error;
			status.release = UpdateCheck::ParseRelease(releases.body, error);
			if (!status.release) errors.push_back(error);
		} else if (releases.status == 404) {
			errors.push_back("No releases found in " + job.repo);
		} else {
			note(releases, "the latest release");
			outcome.releaseFailed = true;
		}

		// Development builds: how far behind their branch (or main) they are, when the commit is on GitHub
		const bool development = job.build.kind != BuildInfo::eBuildKind::RELEASE;
		if (development && !job.build.commit.empty() && !g_Abort && outcome.retryAfter == 0) {
			std::vector<std::string> branches;
			if (UpdateCheck::IsSafeBranch(job.build.branch)) branches.push_back(job.build.branch);
			if (job.build.branch != DEFAULT_BRANCH) branches.push_back(DEFAULT_BRANCH);
			bool answered = false;
			for (const auto& branch : branches) {
				// per_page=1: only the counts are wanted, not the list of commits
				const auto compare = Get(API + job.repo + "/compare/" + job.build.commit + "..." + branch + "?per_page=1");
				if (compare.status == 404 || compare.status == 422) continue; // no such branch here, or the commit isn't in it
				answered = true;
				if (compare.status == 200) {
					std::string error;
					status.comparison = UpdateCheck::ParseComparison(compare.body, error);
					if (status.comparison) status.comparedBranch = branch;
					else errors.push_back(error);
				} else {
					note(compare, "the comparison with " + branch);
					outcome.comparisonFailed = true;
				}
				break;
			}
			if (!answered) errors.push_back("Commit " + job.build.ShortCommit() + " isn't on GitHub in " + job.repo + ", so how far behind it is isn't known");
		}

		for (const auto& error : errors) status.error += (status.error.empty() ? "" : ". ") + error;
		return outcome;
	}

	void WorkerLoop() {
		while (true) {
			Job job;
			{
				std::unique_lock lock(g_Mutex);
				g_Wake.wait(lock, [] { return g_Stopping || g_Job; });
				if (g_Stopping) return;
				job = std::move(*g_Job);
				g_Job.reset();
			}
			auto outcome = Run(job);
			std::lock_guard lock(g_Mutex);
			g_Done = std::move(outcome);
		}
	}

	// ---- Main thread state ----

	const UpdateCheck::Build g_Build = UpdateCheck::CurrentBuild();
	UpdateCheck::Status g_Status;
	std::string g_StatusRepo; // the repository g_Status is about
	bool g_InFlight = false;
	bool g_WasConnected = false;
	Clock::time_point g_NextCheck{};
	Clock::time_point g_LastManual{};
	std::string g_LastLogged;

	const char* StateName(UpdateCheck::eState state) {
		switch (state) {
			case UpdateCheck::eState::UP_TO_DATE: return "up_to_date";
			case UpdateCheck::eState::UPDATE_AVAILABLE: return "update_available";
			case UpdateCheck::eState::FAILED: return "failed";
			default: return "unknown";
		}
	}

	void SendToMaster() {
		UpdateStatus message;
		message.state = static_cast<uint8_t>(UpdateCheck::State(g_Build, g_Status));
		message.summary = UpdateCheck::Summary(g_Build, g_Status);
		MasterPackets::SendToMaster(message);
	}

	void TakeResult(Outcome outcome, bool masterConnected) {
		auto next = std::move(outcome.status);
		// Offline or rate limited: the last answer about the same repository stays, next to the error
		if (outcome.repo == g_StatusRepo) {
			if (outcome.releaseFailed && !next.release) next.release = g_Status.release;
			if (outcome.comparisonFailed && !next.comparison) {
				next.comparison = g_Status.comparison;
				next.comparedBranch = g_Status.comparedBranch;
			}
		}
		next.checkedAt = static_cast<int64_t>(std::time(nullptr));
		g_Status = std::move(next);
		g_StatusRepo = outcome.repo;

		if (outcome.retryAfter > g_Status.checkedAt) {
			g_NextCheck = std::max(g_NextCheck, Clock::now() + std::chrono::seconds(outcome.retryAfter - g_Status.checkedAt + 5));
		}

		const auto summary = UpdateCheck::Summary(g_Build, g_Status);
		if (summary != g_LastLogged) {
			LOG("%s", summary.c_str());
			g_LastLogged = summary;
		}
		if (masterConnected) SendToMaster();
		BroadcastTableChanged("update_check");
	}
}

namespace UpdateChecker {
	void Initialize() {
		if (g_Started) return;
		curl_global_init(CURL_GLOBAL_DEFAULT);
		g_Stopping = false;
		g_Abort = false;
		g_Started = true;
		g_NextCheck = Clock::now() + FIRST_CHECK_DELAY;
		g_Worker = std::thread(WorkerLoop);
	}

	void Shutdown() {
		if (!g_Started) return;
		{
			std::lock_guard lock(g_Mutex);
			g_Stopping = true;
		}
		g_Abort = true; // a request in flight gives up instead of running to its timeout
		g_Wake.notify_all();
		g_Worker.join();
		g_Started = false;
	}

	void Update(bool masterConnected) {
		if (!g_Started) return;
		std::optional<Outcome> done;
		{
			std::lock_guard lock(g_Mutex);
			done.swap(g_Done);
		}
		if (done) {
			g_InFlight = false;
			TakeResult(std::move(*done), masterConnected);
		}

		// A master that (re)connected hears the last result, so its log has it after a restart
		if (masterConnected && !g_WasConnected && g_Status.checkedAt != 0) SendToMaster();
		g_WasConnected = masterConnected;

		const auto now = Clock::now();
		if (g_InFlight || now < g_NextCheck) return;
		const auto settings = ReadSettings();
		if (!settings.enabled) {
			g_NextCheck = now + DISABLED_RECHECK;
			return;
		}
		if (!UpdateCheck::IsValidRepo(settings.repo)) {
			LOG("Not checking for updates: update_check_repo (%s) isn't owner/name", settings.repo.c_str());
			g_NextCheck = now + std::chrono::hours(settings.intervalHours);
			return;
		}
		g_NextCheck = now + std::chrono::hours(settings.intervalHours);
		g_InFlight = true;
		{
			std::lock_guard lock(g_Mutex);
			g_Job = Job{ settings.repo, g_Build };
		}
		g_Wake.notify_one();
	}

	bool CheckNow(std::string& error) {
		const auto settings = ReadSettings();
		if (!settings.enabled) {
			error = "Update checks are turned off (update_check_enabled)";
			return false;
		}
		if (!UpdateCheck::IsValidRepo(settings.repo)) {
			error = "The repository setting (update_check_repo) must look like owner/name";
			return false;
		}
		if (g_InFlight) return true;
		const auto now = Clock::now();
		if (g_LastManual != Clock::time_point{} && now - g_LastManual < MANUAL_SPACING) {
			error = "Checked less than a minute ago; try again in a moment";
			return false;
		}
		g_LastManual = now;
		g_NextCheck = now;
		return true;
	}

	nlohmann::json Json() {
		const auto settings = ReadSettings();
		const auto state = UpdateCheck::State(g_Build, g_Status);
		nlohmann::json json = {
			{"build", {
				{"kind", std::string(BuildInfo::BuildKindName())},
				{"release", g_Build.kind == BuildInfo::eBuildKind::RELEASE},
				{"label", UpdateCheck::BuildLabel(g_Build)},
				{"description", UpdateCheck::DescribeBuild(g_Build)},
				{"version", g_Build.version.ToString()},
				{"commit", g_Build.commit},
				{"shortCommit", g_Build.ShortCommit()},
				{"branch", g_Build.branch},
				{"dirty", g_Build.dirty},
			}},
			{"enabled", settings.enabled},
			{"repo", settings.repo},
			{"repoUrl", UpdateCheck::IsValidRepo(settings.repo) ? "https://github.com/" + settings.repo : ""},
			{"checking", g_InFlight || (settings.enabled && g_Status.checkedAt == 0)},
			{"state", StateName(state)},
			{"updateAvailable", state == UpdateCheck::eState::UPDATE_AVAILABLE},
			{"summary", UpdateCheck::Summary(g_Build, g_Status)},
			{"error", g_Status.error},
			{"checkedAt", g_Status.checkedAt},
			{"intervalHours", settings.intervalHours},
		};
		if (!g_Build.commit.empty() && UpdateCheck::IsValidRepo(settings.repo)) json["build"]["commitUrl"] = "https://github.com/" + settings.repo + "/commit/" + g_Build.commit;
		if (g_Status.release) {
			const auto& release = *g_Status.release;
			json["release"] = { {"tag", release.tag}, {"name", release.name}, {"url", release.url}, {"publishedAt", release.publishedAt},
				{"version", release.version ? release.version->ToString() : ""}, {"newer", UpdateCheck::ReleaseIsNewer(g_Build, g_Status)} };
		}
		if (g_Status.comparison) {
			const auto& comparison = *g_Status.comparison;
			json["comparison"] = { {"branch", g_Status.comparedBranch}, {"status", comparison.status}, {"behind", comparison.commitsBehind},
				{"ahead", comparison.commitsAhead}, {"url", comparison.url} };
		}
		return json;
	}

	void RegisterRoutes() {
		Route(eHTTPMethod::GET, "/api/update_check", 0, "The running build and what the last check for a newer release or newer commits on GitHub found",
			[](HTTPReply& reply, const HTTPContext&) { JsonSuccess(reply, { {"update", Json()} }); });

		Route(eHTTPMethod::POST, "/api/update_check/check", Perm("settings"), "Check GitHub for a newer release now (at most once a minute). Nothing is installed.",
			[](HTTPReply& reply, const HTTPContext&) {
				std::string error;
				if (!CheckNow(error)) return JsonError(reply, eHTTPStatusCode::TOO_MANY_REQUESTS, error);
				BroadcastTableChanged("update_check"); // pages show it's checking
				JsonSuccess(reply, { {"message", "Checking GitHub; the result shows here in a moment"} });
			});
	}
}
