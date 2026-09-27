#include "PropertyRent.h"

#include <ctime>
#include <vector>

#include "CDClientDatabase.h"
#include "Character.h"
#include "ChatPackets.h"
#include "Database.h"
#include "DashboardNotify.h"
#include "dConfig.h"
#include "eLootSourceType.h"
#include "ePropertyPrivacyOption.h"
#include "Entity.h"
#include "EntityManager.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "Mail.h"

namespace {
	struct Templates {
		std::vector<HotPropertySlots::TemplateRow> rows;
		std::vector<HotPropertySlots::EntranceRow> entrances;
	};

	const Templates& GetTemplates() {
		static const auto templates = [] {
			Templates t;
			auto rows = CDClientDatabase::ExecuteQuery("SELECT id, mapID, spawnName, minimumPrice, rentDuration, durationType, reputationPerMinute FROM PropertyTemplate;");
			for (; !rows.eof(); rows.nextRow()) {
				t.rows.push_back({ static_cast<uint32_t>(rows.getIntField("id")), static_cast<uint32_t>(rows.getIntField("mapID")), rows.getStringField("spawnName", ""),
					rows.getIntField("minimumPrice", 0), rows.getIntField("rentDuration", 0), rows.getIntField("durationType", 0), rows.getIntField("reputationPerMinute", 0) });
			}
			auto entrances = CDClientDatabase::ExecuteQuery("SELECT mapID, propertyName FROM PropertyEntranceComponent;");
			for (; !entrances.eof(); entrances.nextRow()) {
				t.entrances.push_back({ static_cast<uint32_t>(entrances.getIntField("mapID")), entrances.getStringField("propertyName", "") });
			}
			return t;
		}();
		return templates;
	}

	int64_t Now() { return static_cast<int64_t>(std::time(nullptr)); }

	int64_t GraceSeconds() {
		return GeneralUtils::TryParse<int64_t>(Game::config->GetValue("property_rent_grace_days")).value_or(3) * PropertyRentRules::DAY;
	}

	std::string Coins(int64_t coins) { return std::to_string(coins) + " coins"; }

	std::string Days(int64_t seconds) { return std::to_string(seconds / PropertyRentRules::DAY) + " days"; }

	void Charge(LWOOBJID playerId) {
		auto* player = Game::entityManager->GetEntity(playerId);
		auto* character = player ? player->GetCharacter() : nullptr;
		if (!character || !PropertyRent::Enabled()) return;

		const auto grace = GraceSeconds();
		for (const auto& property : Database::Get()->GetPropertiesOfOwner(character->GetID())) {
			const auto rate = PropertyRent::RateFor(property.zoneId);
			const auto now = Now();
			const auto decision = PropertyRentRules::Decide(rate, property.rentDue, now, character->GetCoins(), grace);
			const auto name = property.name.empty() ? "your property" : property.name;
			switch (decision.outcome) {
			case PropertyRentRules::eOutcome::NOT_DUE:
				break;
			case PropertyRentRules::eOutcome::PAID:
				character->SetCoins(character->GetCoins() - decision.charge, eLootSourceType::PROPERTY);
				Database::Get()->SetPropertyRent(property.id, decision.charge, decision.newDue);
				DashboardNotify::Changed("properties", property.id);
				LOG("Charged %lld coins rent for property %llu of %llu:%s", decision.charge, property.id, character->GetID(), character->GetName().c_str());
				Mail::SendMail(player, "Rent paid", "You paid " + Coins(decision.charge) + " rent for " + name + ". The next rent is due in " +
					Days(rate->periodSeconds) + "; it is taken from your coins when you log in.", LOT_NULL, 0);
				break;
			case PropertyRentRules::eOutcome::UNPAID: {
				const bool firstTime = property.rentDue != decision.newDue;
				if (firstTime) Database::Get()->SetPropertyRent(property.id, rate->price, decision.newDue);
				const bool makePrivate = decision.overdue && property.privacyOption != static_cast<int32_t>(PropertyPrivacyOption::Private);
				if (makePrivate) {
					Database::Get()->SetPropertyPrivacy(property.id, static_cast<int32_t>(PropertyPrivacyOption::Private));
					DashboardNotify::Changed("properties", property.id);
					LOG("Property %llu of %llu:%s is private: its rent of %lld coins is unpaid", property.id, character->GetID(), character->GetName().c_str(), rate->price);
				}
				// Tell them when it first became due and when the property is made private, not on every login
				if (firstTime || makePrivate) {
					Mail::SendMail(player, "Rent due", "The rent for " + name + " is " + Coins(rate->price) + ", but you don't have enough coins. " +
						(decision.overdue ? std::string("Your property is private until the rent is paid; it is taken from your coins the next time you log in with enough.")
							: "If it isn't paid within " + Days(decision.newDue + grace - now) + ", your property becomes private until it is."), LOT_NULL, 0);
				}
				break;
			}
			}
		}
	}
}

namespace PropertyRent {
	bool Enabled() {
		return Game::config && Game::config->GetValue("property_rent_enabled") == "1";
	}

	const HotPropertySlots::TemplateRow* WorldTemplate(uint32_t mapId) {
		const auto& templates = GetTemplates();
		return HotPropertySlots::WorldTemplate(templates.rows, templates.entrances, mapId);
	}

	std::optional<PropertyRentRules::Rate> RateFor(uint32_t mapId) {
		const auto* row = WorldTemplate(mapId);
		const auto fromTemplate = row ? PropertyRentRules::TemplateRate(row->minimumPrice, row->rentDuration, row->durationType) : std::nullopt;
		for (const auto& rate : Database::Get()->GetPropertyRentRates()) {
			if (rate.mapId == mapId) return PropertyRentRules::Resolve(fromTemplate, rate.price, rate.periodDays);
		}
		return fromTemplate;
	}

	void OnOwnerLoaded(Entity* player) {
		if (!player || !Enabled()) return;
		const auto playerId = player->GetObjectID();
		player->AddCallbackTimer(5.0f, [playerId]() { Charge(playerId); });
	}

	bool IsOverdue(LWOOBJID propertyId, uint32_t mapId) {
		if (!Enabled()) return false;
		return PropertyRentRules::IsOverdue(RateFor(mapId), Database::Get()->GetPropertyRentDue(propertyId), Now(), GraceSeconds());
	}
}
