#include "ModerationBlueprint.h"
#include "Database.h"
#include "eGameMasterLevel.h"
#include "Logger.h"

namespace ModerationBlueprint {

	// Helper function to get current user's account info from session
	std::optional<IAccounts::Info> GetCurrentUser(const crow::request& req, DashboardApp& app) {
		auto& session = app.get_context<Session>(const_cast<crow::request&>(req));
		std::string username = session.template get<std::string>("username");

		if (username.empty()) {
			return std::nullopt;
		}

		return Database::Get()->GetAccountInfo(username);
	}

	// Helper function to check if user has minimum GM level
	bool HasMinimumGMLevel(const crow::request& req, DashboardApp& app, eGameMasterLevel required) {
		auto user = GetCurrentUser(req, app);
		if (!user) {
			return false;
		}
		return static_cast<uint8_t>(user->maxGmLevel) >= static_cast<uint8_t>(required);
	}

	void Setup(DashboardApp& app) {
		// Get pet names by status
		CROW_ROUTE(app, "/api/moderation/pets")
			.methods("GET"_method)
			([&](const crow::request& req) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;
				crow::json::wvalue::list data;

				try {
					auto statusParam = req.url_params.get("status");
					std::string status = statusParam ? statusParam : "all";

					std::vector<IPetNames::DetailedInfo> pets;

					if (status == "approved") {
						pets = Database::Get()->GetPetNamesByStatus(2);
					} else if (status == "unapproved") {
						pets = Database::Get()->GetPetNamesByStatus(1);
					} else {
						pets = Database::Get()->GetAllPetNames();
					}

					for (const auto& pet : pets) {
						crow::json::wvalue item;
						item["id"] = static_cast<uint64_t>(pet.id);
						item["pet_name"] = pet.petName;
						item["approval_status"] = pet.approvalStatus;
						item["owner_id"] = static_cast<uint64_t>(pet.ownerId);

						// Get owner character name
						if (pet.ownerId > 0) {
							auto charInfo = Database::Get()->GetCharacterInfo(pet.ownerId);
							if (charInfo) {
								item["owner_name"] = charInfo->name;
							} else {
								item["owner_name"] = "Unknown";
							}
						} else {
							item["owner_name"] = "None";
						}

						data.push_back(std::move(item));
					}

					response["data"] = std::move(data);

				} catch (std::exception& ex) {
					response["error"] = ex.what();
					return crow::response(500, response);
				}

				return crow::response(response);
			});

		// Approve a pet name
		CROW_ROUTE(app, "/api/moderation/pets/<uint>/approve")
			.methods("POST"_method)
			([&](const crow::request& req, uint64_t pet_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					Database::Get()->SetPetApprovalStatus(pet_id, 2); // 2 = approved

					response["success"] = true;
					response["message"] = "Pet name approved";

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Reject a pet name
		CROW_ROUTE(app, "/api/moderation/pets/<uint>/reject")
			.methods("POST"_method)
			([&](const crow::request& req, uint64_t pet_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					Database::Get()->SetPetApprovalStatus(pet_id, 0); // 0 = rejected

					response["success"] = true;
					response["message"] = "Pet name rejected";

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Get properties by approval status
		CROW_ROUTE(app, "/api/moderation/properties")
			.methods("GET"_method)
			([&](const crow::request& req) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;
				crow::json::wvalue::list data;

				try {
					auto statusParam = req.url_params.get("status");
					std::string status = statusParam ? statusParam : "all";

					std::vector<IProperty::Info> properties;

					if (status == "approved") {
						properties = Database::Get()->GetPropertiesByApprovalStatus(1);
					} else if (status == "unapproved") {
						properties = Database::Get()->GetPropertiesByApprovalStatus(0);
					} else {
						properties = Database::Get()->GetAllProperties();
					}

					for (const auto& prop : properties) {
						crow::json::wvalue item;
						item["id"] = static_cast<uint64_t>(prop.id);
						item["name"] = prop.name;
						item["description"] = prop.description;
						item["owner_id"] = static_cast<uint64_t>(prop.ownerId);
						item["clone_id"] = static_cast<uint64_t>(prop.cloneId);
						item["privacy_option"] = prop.privacyOption;
						item["mod_approved"] = prop.modApproved;
						item["last_updated"] = prop.lastUpdatedTime;
						item["claimed_time"] = prop.claimedTime;
						item["reputation"] = prop.reputation;
						item["performance_cost"] = prop.performanceCost;
						item["rejection_reason"] = prop.rejectionReason;

						// Get owner character name
						auto charInfo = Database::Get()->GetCharacterInfo(prop.ownerId);
						if (charInfo) {
							item["owner_name"] = charInfo->name;
						} else {
							item["owner_name"] = "Unknown";
						}

						data.push_back(std::move(item));
					}

					response["data"] = std::move(data);

				} catch (std::exception& ex) {
					response["error"] = ex.what();
					return crow::response(500, response);
				}

				return crow::response(response);
			});

		// Approve/unapprove a property
		CROW_ROUTE(app, "/api/moderation/properties/<uint>/approve")
			.methods("POST"_method)
			([&](const crow::request& req, uint64_t property_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					auto prop = Database::Get()->GetPropertyInfo(property_id);
					if (!prop) {
						response["success"] = false;
						response["error"] = "Property not found";
						return crow::response(404, response);
					}

					// Toggle approval
					IProperty::Info updatedInfo = *prop;
					updatedInfo.modApproved = prop->modApproved ? 0 : 1;
					updatedInfo.rejectionReason = "";

					Database::Get()->UpdatePropertyModerationInfo(updatedInfo);

					response["success"] = true;
					response["approved"] = updatedInfo.modApproved;

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Reject a property with reason
		CROW_ROUTE(app, "/api/moderation/properties/<uint>/reject")
			.methods("POST"_method)
			([&](const crow::request& req, uint64_t property_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				auto body = crow::json::load(req.body);
				if (!body) {
					return crow::response(400, "{\"error\": \"Invalid JSON\"}");
				}

				crow::json::wvalue response;

				try {
					auto prop = Database::Get()->GetPropertyInfo(property_id);
					if (!prop) {
						response["success"] = false;
						response["error"] = "Property not found";
						return crow::response(404, response);
					}

					std::string reason;
					if (body.has("reason"))
						reason = std::string(body["reason"].s());
					else
						reason = "No reason provided";

					IProperty::Info updatedInfo = *prop;
					updatedInfo.modApproved = 0;
					updatedInfo.rejectionReason = reason;

					Database::Get()->UpdatePropertyModerationInfo(updatedInfo);

					response["success"] = true;
					response["message"] = "Property rejected";

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});
	}

} // namespace ModerationBlueprint
