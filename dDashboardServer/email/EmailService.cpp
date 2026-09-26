#include "EmailService.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#include "Game.h"
#include "Logger.h"
#include "dConfig.h"
#include "GeneralUtils.h"
#include "BinaryPathFinder.h"
#include "OAuth2.h"

namespace {
	using Task = std::function<std::optional<std::string>()>;

	struct Job {
		Task task;
		EmailService::Completion onComplete;
	};

	struct Finished {
		EmailService::Completion onComplete;
		std::optional<std::string> error;
	};

	// Written in Initialize before the worker starts, read-only afterwards
	Smtp::Config g_Config;
	std::string g_BaseUrl;
	bool g_Configured = false;
	bool g_UseOAuth2 = false;
	std::string g_OAuth2ConfigError;

	OAuth2::TokenManager g_Tokens; // token operations run on the worker only

	std::thread g_Worker;
	std::mutex g_Mutex;
	std::condition_variable g_Wake;
	std::deque<Job> g_Jobs;
	std::deque<Finished> g_Finished;
	bool g_Stopping = false;

	void WorkerLoop() {
		while (true) {
			Job job;
			{
				std::unique_lock lock(g_Mutex);
				g_Wake.wait(lock, [] { return g_Stopping || !g_Jobs.empty(); });
				if (g_Jobs.empty()) return; // stopping and drained
				job = std::move(g_Jobs.front());
				g_Jobs.pop_front();
			}

			std::optional<std::string> error;
			try {
				error = job.task();
			} catch (const std::exception& ex) {
				error = std::string("Unexpected error: ") + ex.what();
			}

			std::lock_guard lock(g_Mutex);
			g_Finished.push_back({ std::move(job.onComplete), std::move(error) });
		}
	}

	void Enqueue(Task task, EmailService::Completion onComplete) {
		{
			std::lock_guard lock(g_Mutex);
			g_Jobs.push_back({ std::move(task), std::move(onComplete) });
		}
		g_Wake.notify_one();
	}

	// Runs on the worker thread
	std::optional<std::string> SendNow(const Smtp::Message& message) {
		if (!g_UseOAuth2) return Smtp::Send(g_Config, message);

		// Try with the cached token first; if the server rejects it, fetch a fresh one and retry once
		for (int attempt = 0; attempt < 2; attempt++) {
			std::string tokenError;
			const auto token = g_Tokens.GetAccessToken(attempt > 0, tokenError);
			if (!token) return tokenError;

			auto config = g_Config;
			config.oauth2Token = *token;
			bool authFailed = false;
			auto error = Smtp::Send(config, message, &authFailed);
			if (!error || !authFailed) return error;
			if (attempt == 1) return *error + " (check that the connected account is allowed to send as " + g_Config.fromAddress + ")";
		}
		return std::nullopt;
	}

	std::string Setting(const std::string& key, const std::string& fallback = "") {
		const auto value = Game::config->GetValue(key);
		return value.empty() ? fallback : value;
	}

	void ConfigureOAuth2() {
		OAuth2::Config oauth;
		oauth.provider = Setting("smtp_oauth2_provider", "custom");
		oauth.grant = Setting("smtp_oauth2_grant") == "client_credentials" ? OAuth2::eGrant::CLIENT_CREDENTIALS : OAuth2::eGrant::AUTHORIZATION_CODE;
		oauth.clientId = Setting("smtp_oauth2_client_id");
		oauth.clientSecret = Setting("smtp_oauth2_client_secret");
		oauth.authorizeUrl = Setting("smtp_oauth2_authorize_url");
		oauth.tokenUrl = Setting("smtp_oauth2_token_url");
		oauth.scope = Setting("smtp_oauth2_scope");
		oauth.refreshToken = Setting("smtp_oauth2_refresh_token");
		oauth.tokenFile = (BinaryPathFinder::GetBinaryDir() / "dashboard_oauth2_token.json").string();
		oauth.timeoutSeconds = g_Config.timeoutSeconds;

		if (const auto error = OAuth2::ApplyProviderDefaults(oauth, Setting("smtp_oauth2_tenant"))) {
			g_OAuth2ConfigError = *error;
			return;
		}
		g_Tokens.Configure(std::move(oauth));
	}
}

namespace EmailService {
	void Initialize() {
		g_UseOAuth2 = Setting("smtp_auth", "password") == "oauth2";
		const auto provider = Setting("smtp_oauth2_provider");

		// Known providers have fixed SMTP servers
		std::string defaultHost;
		if (g_UseOAuth2 && provider == "microsoft") defaultHost = "smtp.office365.com";
		else if (g_UseOAuth2 && provider == "google") defaultHost = "smtp.gmail.com";

		g_Config.host = Setting("smtp_host", defaultHost);
		g_Config.port = GeneralUtils::TryParse<uint16_t>(Setting("smtp_port")).value_or(587);
		const auto security = Setting("smtp_security", "starttls");
		g_Config.security = security == "tls" ? Smtp::eSecurity::TLS : security == "none" ? Smtp::eSecurity::NONE : Smtp::eSecurity::STARTTLS;
		g_Config.password = Setting("smtp_password");
		g_Config.fromAddress = Setting("smtp_from_address");
		// With OAuth2 the login is the mailbox address, which is normally the sender
		g_Config.username = Setting("smtp_username", g_UseOAuth2 ? g_Config.fromAddress : "");
		g_Config.fromName = Setting("smtp_from_name", "DarkflameServer");
		g_Config.caFile = Setting("smtp_ca_file");
		g_Config.verifyCertificate = Setting("smtp_verify_certificate", "1") != "0";
		g_Config.timeoutSeconds = GeneralUtils::TryParse<uint32_t>(Setting("smtp_timeout")).value_or(20);

		g_BaseUrl = Setting("dashboard_url");
		while (!g_BaseUrl.empty() && g_BaseUrl.back() == '/') g_BaseUrl.pop_back();

		g_Configured = !g_Config.host.empty() && !g_Config.fromAddress.empty() && !g_BaseUrl.empty();
		if (!g_Configured) {
			LOG("Email is disabled; set smtp_host, smtp_from_address and dashboard_url in dashboardconfig.ini to enable it");
			return;
		}
		if (!Smtp::IsValidAddress(g_Config.fromAddress)) {
			LOG("smtp_from_address '%s' is not a valid address; email is disabled", g_Config.fromAddress.c_str());
			g_Configured = false;
			return;
		}
		if (g_UseOAuth2) {
			ConfigureOAuth2();
			if (!g_OAuth2ConfigError.empty()) {
				LOG("Email OAuth2 settings are incomplete: %s; email is disabled", g_OAuth2ConfigError.c_str());
				g_Configured = false;
				return;
			}
		}
		if (!g_Config.verifyCertificate) LOG("WARNING: SMTP certificate verification is disabled (smtp_verify_certificate=0)");
		if (g_Config.security == Smtp::eSecurity::NONE) LOG("WARNING: SMTP is configured without TLS (smtp_security=none)");

		Smtp::GlobalInit();
		g_Stopping = false;
		g_Worker = std::thread(WorkerLoop);
		LOG("Email enabled via %s:%u%s", g_Config.host.c_str(), g_Config.port, g_UseOAuth2 ? " using OAuth2" : "");
	}

	void Shutdown() {
		if (!g_Worker.joinable()) return;
		{
			std::lock_guard lock(g_Mutex);
			g_Stopping = true;
		}
		g_Wake.notify_all();
		g_Worker.join();
		Update();
		Smtp::GlobalCleanup();
	}

	bool IsConfigured() {
		return g_Configured;
	}

	std::string BaseUrl() {
		return g_BaseUrl;
	}

	void Send(Smtp::Message message, Completion onComplete) {
		if (!g_Configured) {
			if (onComplete) onComplete("Email is not configured on this server");
			return;
		}
		Enqueue([message = std::move(message)]() { return SendNow(message); }, std::move(onComplete));
	}

	void RunOnWorker(std::function<std::optional<std::string>()> task, Completion onComplete) {
		if (!g_Configured) {
			if (onComplete) onComplete("Email is not configured on this server");
			return;
		}
		Enqueue(std::move(task), std::move(onComplete));
	}

	void Update() {
		std::deque<Finished> finished;
		{
			std::lock_guard lock(g_Mutex);
			finished.swap(g_Finished);
		}
		for (auto& item : finished) {
			if (item.error) LOG("Email task failed: %s", item.error->c_str());
			if (item.onComplete) item.onComplete(item.error);
		}
	}

	bool UsesOAuth2() {
		return g_UseOAuth2;
	}

	nlohmann::json OAuth2Status() {
		nlohmann::json status{
			{"configured", g_Configured},
			{"auth", g_UseOAuth2 ? "oauth2" : "password"},
			{"host", g_Config.host},
			{"from", g_Config.fromAddress}
		};
		if (!g_UseOAuth2) return status;
		if (!g_OAuth2ConfigError.empty()) {
			status["error"] = g_OAuth2ConfigError;
			return status;
		}
		const auto& oauth = g_Tokens.GetConfig();
		status["provider"] = oauth.provider;
		status["grant"] = oauth.grant == OAuth2::eGrant::CLIENT_CREDENTIALS ? "client_credentials" : "authorization_code";
		status["account"] = g_Config.username;
		status["connected"] = g_Tokens.HasCredentials();
		status["canConnect"] = oauth.grant == OAuth2::eGrant::AUTHORIZATION_CODE && !oauth.authorizeUrl.empty();
		status["redirectUri"] = OAuth2RedirectUri();
		const auto lastError = g_Tokens.LastError();
		if (!lastError.empty()) status["error"] = lastError;
		return status;
	}

	std::string OAuth2RedirectUri() {
		return g_BaseUrl + "/oauth2/callback";
	}

	std::string OAuth2AuthorizeUrl(const std::string& state, const std::string& codeChallenge) {
		if (!g_Configured || !g_UseOAuth2) return "";
		const auto& oauth = g_Tokens.GetConfig();
		if (oauth.grant != OAuth2::eGrant::AUTHORIZATION_CODE || oauth.authorizeUrl.empty()) return "";
		return OAuth2::BuildAuthorizeUrl(oauth, OAuth2RedirectUri(), state, codeChallenge);
	}

	void OAuth2Connect(const std::string& code, const std::string& codeVerifier, Completion onComplete) {
		RunOnWorker([code, codeVerifier]() { return g_Tokens.ExchangeCode(code, codeVerifier, OAuth2RedirectUri()); }, std::move(onComplete));
	}

	void OAuth2Disconnect(Completion onComplete) {
		RunOnWorker([]() -> std::optional<std::string> { g_Tokens.Disconnect(); return std::nullopt; }, std::move(onComplete));
	}
}
