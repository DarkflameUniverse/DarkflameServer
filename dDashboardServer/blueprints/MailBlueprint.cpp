#include "MailBlueprint.h"
#include "Database.h"
#include "eGameMasterLevel.h"
#include "MailInfo.h"
#include "Logger.h"
#include <ctime>

namespace MailBlueprint {

	// Helper function to get current user's account info from session
	std::optional<IAccounts::Info> GetCurrentUser(const crow::request& req, DashboardApp& app) {
		auto& session = app.get_context<Session>(const_cast<crow::request&>(req));
		std::string username = session.template get<std::string>("username");

		if (username.empty()) {
			return std::nullopt;
		}

		return Database::Get()->GetAccountInfo(username);
	}

	// Helper function to get user's GM level
	eGameMasterLevel GetUserGMLevel(const crow::request& req, DashboardApp& app) {
		auto user = GetCurrentUser(req, app);
		if (!user) {
			return eGameMasterLevel::CIVILIAN;
		}
		return user->maxGmLevel;
	}

	// Helper function to check if user has minimum GM level
	bool HasMinimumGMLevel(const crow::request& req, DashboardApp& app, eGameMasterLevel required) {
		auto level = GetUserGMLevel(req, app);
		return static_cast<uint8_t>(level) >= static_cast<uint8_t>(required);
	}

	void Setup(DashboardApp& app) {
		// Send mail to a character or all characters
		CROW_ROUTE(app, "/api/mail/send")
			.methods("POST"_method)
			([&](const crow::request& req) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				auto body = crow::json::load(req.body);
				if (!body) {
					return crow::response(400, "{\"error\": \"Invalid JSON\"}");
				}

				crow::json::wvalue response;

				try {
					auto user = GetCurrentUser(req, app);
					if (!user) {
						response["success"] = false;
						response["error"] = "Not authenticated";
						return crow::response(401, response);
					}

					auto& session = app.get_context<Session>(const_cast<crow::request&>(req));
					std::string username = session.template get<std::string>("username");

					// Get mail parameters
					std::string subject;
					if (body.has("subject"))
						subject = std::string(body["subject"].s());
					else
						subject = "";

					std::string message;
					if (body.has("body"))
						message = std::string(body["body"].s());
					else
						message = "";
					int64_t recipientId = body.has("recipient_id") ? body["recipient_id"].i() : 0;
					bool sendToAll = body.has("send_to_all") ? body["send_to_all"].b() : false;

					// Item attachment (optional)
					int32_t itemLot = body.has("attachment_lot") ? body["attachment_lot"].i() : 0;
					int32_t itemCount = body.has("attachment_count") ? body["attachment_count"].i() : 0;

					if (subject.empty() || message.empty()) {
						response["success"] = false;
						response["error"] = "Subject and body are required";
						return crow::response(response);
					}

					// Prefix sender name with [GM]
					std::string senderName = "[GM] " + username;

					std::vector<LWOOBJID> recipients;

					if (sendToAll) {
						// Get all accounts and their characters
						auto allAccounts = Database::Get()->GetAllAccounts();
						for (const auto& acct : allAccounts) {
							auto chars = Database::Get()->GetAccountCharacterIds(acct.id);
							for (const auto& charId : chars) {
								recipients.push_back(charId);
							}
						}
					} else if (recipientId > 0) {
						recipients.push_back(recipientId);
					} else {
						response["success"] = false;
						response["error"] = "No recipients specified";
						return crow::response(response);
					}

					// Send mail to all recipients
					uint64_t currentTime = static_cast<uint64_t>(std::time(nullptr));
					int mailSent = 0;

					for (const auto& recipId : recipients) {
						// Get recipient character name
						auto charInfo = Database::Get()->GetCharacterInfo(recipId);
						if (!charInfo) continue;

						MailInfo mail;
						mail.senderUsername = senderName;
						mail.recipient = charInfo->name;
						mail.receiverId = recipId;
						mail.subject = subject;
						mail.body = message;
						mail.itemID = itemLot > 0 ? 1 : 0; // If there's an item, set ID to 1
						mail.itemLOT = itemLot;
						mail.itemCount = itemCount > 0 ? itemCount : 1;
						mail.timeSent = currentTime;
						mail.wasRead = false;

						Database::Get()->InsertNewMail(mail);
						mailSent++;
					}

					response["success"] = true;
					response["message"] = "Mail sent successfully";
					response["recipients"] = mailSent;

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Get mail by ID (for viewing)
		CROW_ROUTE(app, "/api/mail/<uint>")
			.methods("GET"_method)
			([&](const crow::request& req, uint64_t mail_id) {
				// Any authenticated user can view mail
				auto user = GetCurrentUser(req, app);
				if (!user) {
					return crow::response(401, "{\"error\": \"Not authenticated\"}");
				}

				crow::json::wvalue response;

				try {
					auto mail = Database::Get()->GetMail(mail_id);
					if (!mail) {
						response["success"] = false;
						response["error"] = "Mail not found";
						return crow::response(404, response);
					}

					// Check if user can access this mail (owns the character or is GM)
					auto charInfo = Database::Get()->GetCharacterInfo(mail->receiverId);
					bool canAccess = false;

					if (charInfo && charInfo->accountId == user->id) {
						canAccess = true;
					}

					if (static_cast<uint8_t>(user->maxGmLevel) >= static_cast<uint8_t>(eGameMasterLevel::MODERATOR)) {
						canAccess = true;
					}

					if (!canAccess) {
						response["success"] = false;
						response["error"] = "Access denied";
						return crow::response(403, response);
					}

					response["success"] = true;
					response["id"] = mail->id;
					response["sender_name"] = mail->senderUsername;
					response["receiver_name"] = mail->recipient;
					response["receiver_id"] = static_cast<uint64_t>(mail->receiverId);
					response["subject"] = mail->subject;
					response["body"] = mail->body;
					response["attachment_lot"] = mail->itemLOT;
					response["attachment_count"] = mail->itemCount;
					response["time_sent"] = mail->timeSent;
					response["was_read"] = mail->wasRead;

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});
	}

} // namespace MailBlueprint
