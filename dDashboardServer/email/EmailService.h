#pragma once

#include <functional>
#include <optional>
#include <string>

#include "SmtpClient.h"
#include "json.hpp"

/**
 * Sends dashboard emails (password resets, address verification) in the background.
 * SMTP runs on a worker thread; completion callbacks run on the main thread from Update().
 */
namespace EmailService {
	// Called with nullopt on success or the error message
	using Completion = std::function<void(const std::optional<std::string>& error)>;

	// Read SMTP settings from the dashboard config and start the worker. Safe to call when unconfigured.
	void Initialize();
	void Shutdown();

	// Whether smtp_host, smtp_from_address and dashboard_url are set
	bool IsConfigured();

	// Public base URL used for links in emails (dashboard_url), without a trailing slash
	std::string BaseUrl();

	// Queue a message. The completion runs on the main thread.
	void Send(Smtp::Message message, Completion onComplete = nullptr);

	// Run blocking work (e.g. talking to an OAuth2 provider) on the email worker thread
	void RunOnWorker(std::function<std::optional<std::string>()> task, Completion onComplete);

	// ---- OAuth2 (smtp_auth=oauth2) ----

	bool UsesOAuth2();

	// Status for the operator UI: provider, grant, whether an account is connected, last error
	nlohmann::json OAuth2Status();

	// Where the provider sends the operator back after signing in
	std::string OAuth2RedirectUri();

	// Authorization URL for connecting a mailbox. Empty if OAuth2 isn't set up for the connect flow.
	std::string OAuth2AuthorizeUrl(const std::string& state, const std::string& codeChallenge);

	// Exchange the code from the provider on the worker thread
	void OAuth2Connect(const std::string& code, const std::string& codeVerifier, Completion onComplete);
	void OAuth2Disconnect(Completion onComplete);

	// Run completions for finished sends. Call once per tick from the main loop.
	void Update();
}
