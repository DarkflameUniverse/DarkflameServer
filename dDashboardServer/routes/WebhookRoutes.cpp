#include "WebhookRoutes.h"

#include "RouteUtils.h"
#include "Alerts.h"
#include "Database.h"
#include "WSRoutes.h"
#include "WebhookFormat.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	constexpr size_t MAX_WEBHOOKS = 25;

	// Webhook URLs usually contain a token (Discord, Slack), so only a masked form leaves the server
	nlohmann::json WebhookJson(const IDashboardAdmin::Webhook& webhook) {
		return {
			{"id", webhook.id}, {"name", webhook.name}, {"url", WebhookFormat::MaskUrl(webhook.url)}, {"format", webhook.format},
			{"events", webhook.events}, {"hasSecret", !webhook.secret.empty()}, {"enabled", webhook.enabled},
			{"createdAt", webhook.createdAt}, {"lastSentAt", webhook.lastSentAt}, {"lastStatus", webhook.lastStatus}, {"lastError", webhook.lastError}
		};
	}

	// Fill a webhook from a request body; fields not in the body keep their current value
	std::optional<std::string> Apply(IDashboardAdmin::Webhook& webhook, const nlohmann::json& body) {
		if (body.contains("name")) webhook.name = body.value("name", "");
		if (webhook.name.empty() || webhook.name.size() > 64) return "Give it a name of up to 64 characters";
		if (body.contains("url") && !body.value("url", "").empty()) webhook.url = body.value("url", "");
		if (const auto error = WebhookFormat::ValidateUrl(webhook.url)) return *error;
		if (body.contains("format")) webhook.format = body.value("format", "");
		if (!WebhookFormat::IsKnownFormat(webhook.format)) return "Format must be discord, slack or json";
		if (body.contains("events")) {
			std::string events;
			if (body["events"].is_array()) {
				for (const auto& event : body["events"]) if (event.is_string()) events += (events.empty() ? "" : ",") + event.get<std::string>();
			} else {
				events = body.value("events", "");
			}
			webhook.events = WebhookFormat::NormalizeEvents(events);
		}
		if (webhook.events.empty()) return "Pick at least one event";
		// A null secret keeps the current one; an empty string clears it
		if (body.contains("secret") && body["secret"].is_string()) webhook.secret = body["secret"].get<std::string>();
		if (webhook.secret.size() > 128) return "The secret can be at most 128 characters";
		if (body.contains("enabled")) webhook.enabled = body.value("enabled", true);
		return std::nullopt;
	}
}

void RegisterWebhookRoutes() {
	Route(eHTTPMethod::GET, "/api/webhooks", Perm("webhooks"), "Outgoing webhooks (URLs masked)",
		[](HTTPReply& reply, const HTTPContext&) {
			nlohmann::json list = nlohmann::json::array();
			for (const auto& webhook : Database::Get()->GetWebhooks()) list.push_back(WebhookJson(webhook));
			nlohmann::json events = nlohmann::json::array();
			for (const auto& event : WebhookFormat::Events()) {
				if (std::string(event.name) != "test") events.push_back({ {"name", event.name}, {"description", event.description} });
			}
			JsonReply(reply, eHTTPStatusCode::OK, { {"webhooks", list}, {"events", events} });
		});

	Route(eHTTPMethod::POST, "/api/webhooks", Perm("webhooks"), "Add a webhook. Body: {name, url, format (discord|slack|json), events: [..] or \"*\", secret}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto body = ParseBody(context);
			if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
			if (Database::Get()->GetWebhooks().size() >= MAX_WEBHOOKS) return JsonError(reply, eHTTPStatusCode::CONFLICT, "Too many webhooks");
			IDashboardAdmin::Webhook webhook;
			webhook.format = "discord";
			if (const auto error = Apply(webhook, *body)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, *error);
			Database::Get()->InsertWebhook(webhook);
			Audit(context, "manage_webhook", "Added webhook " + webhook.name + " to " + WebhookFormat::MaskUrl(webhook.url) + " for " + webhook.events);
			BroadcastTableChanged("webhooks");
			JsonSuccess(reply, { {"message", "Webhook added"} });
		});

	Route(eHTTPMethod::POST, "/api/webhooks/:id", Perm("webhooks"), "Change a webhook. Body: any of {name, url, format, events, secret, enabled}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto id = PathId<uint32_t>(context.path, 2);
			const auto body = ParseBody(context);
			if (!id || !body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid request");
			auto webhook = Database::Get()->GetWebhook(*id);
			if (!webhook) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Webhook not found");
			if (const auto error = Apply(*webhook, *body)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, *error);
			Database::Get()->UpdateWebhook(*webhook);
			Audit(context, "manage_webhook", "Changed webhook " + webhook->name + (webhook->enabled ? "" : " (disabled)"));
			BroadcastTableChanged("webhooks", std::to_string(*id));
			JsonSuccess(reply, { {"message", "Webhook saved"} });
		});

	Route(eHTTPMethod::POST, "/api/webhooks/:id/delete", Perm("webhooks"), "Remove a webhook",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto id = PathId<uint32_t>(context.path, 2);
			if (!id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid ID");
			const auto webhook = Database::Get()->GetWebhook(*id);
			if (!webhook) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Webhook not found");
			Database::Get()->DeleteWebhook(*id);
			Audit(context, "manage_webhook", "Removed webhook " + webhook->name);
			BroadcastTableChanged("webhooks");
			JsonSuccess(reply, { {"message", "Webhook removed"} });
		});

	Route(eHTTPMethod::POST, "/api/webhooks/:id/test", Perm("webhooks"), "Send a test message; the result appears on the webhook shortly",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto id = PathId<uint32_t>(context.path, 2);
			if (!id || !Database::Get()->GetWebhook(*id)) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Webhook not found");
			const auto webhookId = *id;
			Alerts::SendTest(webhookId, [webhookId](std::optional<std::string>) { BroadcastTableChanged("webhooks", std::to_string(webhookId)); });
			JsonSuccess(reply, { {"message", "Test sent"} });
		});
}
