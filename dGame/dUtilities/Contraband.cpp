#include "Contraband.h"

#include <ctime>
#include <optional>

#include "magic_enum.hpp"

#include "Character.h"
#include "ChatPackets.h"
#include "Database.h"
#include "dConfig.h"
#include "dServer.h"
#include "eGameMasterLevel.h"
#include "EconomyLedger.h"
#include "Entity.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "Inventory.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "Logger.h"
#include "Mail.h"
#include "User.h"
#include "ZCompression.h"

namespace {
	std::optional<Contraband::List> g_List;

	constexpr const char* ACTOR = "World server";

	bool IgnoreStaff() {
		return !Game::config || Game::config->GetValue("contraband_ignore_staff") != "0";
	}

	bool NotifyPlayers() {
		return !Game::config || Game::config->GetValue("contraband_notify_players") != "0";
	}

	eGameMasterLevel AccountLevel(const Entity* player) {
		const auto* character = player ? player->GetCharacter() : nullptr;
		const auto* user = character ? character->GetParentUser() : nullptr;
		return user ? user->GetMaxGMLevel() : eGameMasterLevel::CIVILIAN;
	}

	std::string Compress(const std::string& data) {
		std::string out(ZCompression::GetMaxCompressedLength(static_cast<uint32_t>(data.size())), '\0');
		const auto size = ZCompression::Compress(reinterpret_cast<const uint8_t*>(data.data()), static_cast<uint32_t>(data.size()),
			reinterpret_cast<uint8_t*>(out.data()), static_cast<uint32_t>(out.size()));
		out.resize(size > 0 ? static_cast<size_t>(size) : 0);
		return out;
	}

	std::string Where() {
		if (!Game::server) return "";
		return " (zone " + std::to_string(Game::server->GetZoneID()) + " instance " + std::to_string(Game::server->GetInstanceID()) + ")";
	}

	// Every item a player has (not the proxy items of sets), across every inventory
	std::vector<Contraband::HeldItem> HeldItems(InventoryComponent& inventory) {
		std::vector<Contraband::HeldItem> items;
		for (const auto& [type, inv] : inventory.GetInventories()) {
			if (!inv) continue;
			for (const auto& [id, item] : inv->GetItems()) {
				if (!item || item->GetParent() != LWOOBJID_EMPTY) continue;
				items.push_back({ id, item->GetLot(), item->GetCount(), type });
			}
		}
		return items;
	}

	// Remove every item of a LOT (all inventories); how many were removed
	uint32_t RemoveLot(InventoryComponent& inventory, LOT lot, bool silent) {
		uint32_t removed = 0;
		for (const auto& [type, inv] : inventory.GetInventories()) {
			if (!inv) continue;
			std::vector<Item*> items;
			for (const auto& [id, item] : inv->GetItems()) if (item && item->GetLot() == lot && item->GetParent() == LWOOBJID_EMPTY) items.push_back(item);
			for (auto* item : items) {
				removed += item->GetCount();
				item->SetCount(0, silent, false, false, eLootSourceType::MODERATION);
			}
		}
		return removed;
	}
}

namespace Contraband {
	bool CountsAsAdded(eLootSourceType source, eInventoryType sourceInventory) {
		return sourceInventory == eInventoryType::INVALID && source != eLootSourceType::RELOCATE && source != eLootSourceType::INVENTORY;
	}

	uint32_t Reload() {
		List list;
		for (const auto& item : Database::Get()->GetContrabandItems()) list[item.lot] = { item.reason, item.action };
		LOG("Loaded %zu contraband item(s)", list.size());
		g_List = std::move(list);
		return 1;
	}

	const List& Get() {
		if (!g_List) Reload();
		return *g_List;
	}

	void CheckOnLoad(Entity* player) {
		auto* character = player ? player->GetCharacter() : nullptr;
		auto* inventory = player ? player->GetComponent<InventoryComponent>() : nullptr;
		if (!character || !inventory || Get().empty() || !Applies(AccountLevel(player), IgnoreStaff())) return;

		const auto findings = Find(HeldItems(*inventory), Get());
		if (findings.empty()) return;

		const auto characterId = character->GetID();
		const auto accountId = character->GetParentUser() ? character->GetParentUser()->GetAccountID() : 0;
		bool snapshotTaken = false;
		std::vector<std::string> removedNames;
		for (const auto& finding : findings) {
			const bool remove = finding.entry.action == IContraband::eContrabandAction::REMOVE;
			const auto where = std::string(magic_enum::enum_name(finding.item.inventory));
			Database::Get()->InsertEconomyFlag({ 0, IDashboardAdmin::eFlagKind::CONTRABAND, characterId, finding.item.lot, finding.item.id,
				finding.item.count, remove ? 1 : 0, (remove ? "Removed at login from " : "Found at login in ") + where + ": " + finding.entry.reason });
			if (!remove) continue;

			// Keep the character as it was, so staff can give the items back from the character page
			if (!snapshotTaken) {
				const auto& xml = character->GetXMLData();
				Database::Get()->InsertCharacterSnapshot({ 0, characterId, static_cast<int64_t>(std::time(nullptr)), "before contraband removal", ACTOR,
					static_cast<uint32_t>(xml.size()), "", Compress(xml) });
				snapshotTaken = true;
			}
			auto* item = inventory->FindItemById(finding.item.id);
			if (!item) continue;
			// Silent: the character hasn't been sent to the client yet
			item->SetCount(0, true, false, false, eLootSourceType::MODERATION);
			removedNames.push_back(std::to_string(finding.item.count) + "x " + std::to_string(finding.item.lot));
			Database::Get()->InsertAuditLog(0, ACTOR, "contraband_removed", character->GetName() + ": removed " + std::to_string(finding.item.count) +
				" of item " + std::to_string(finding.item.lot) + " (id " + std::to_string(finding.item.id) + ", " + where + ") at login" + Where() + ": " + finding.entry.reason,
				accountId, characterId);
		}
		LOG("Character %llu:%s has %zu contraband item stack(s), %zu removed", characterId, character->GetName().c_str(), findings.size(), removedNames.size());

		if (!removedNames.empty() && NotifyPlayers()) {
			std::string body = "Items that are not allowed on this server were removed from your character:";
			for (const auto& finding : findings) {
				if (finding.entry.action != IContraband::eContrabandAction::REMOVE) continue;
				body += "\n- " + std::to_string(finding.item.count) + " x item " + std::to_string(finding.item.lot) + (finding.entry.reason.empty() ? "" : ": " + finding.entry.reason);
			}
			body += "\nIf you think this is a mistake, contact the server's staff.";
			Mail::SendMail(player, "Items removed", body, LOT_NULL, 0);
		}
	}

	void OnItemAdded(Entity* owner, LOT lot, uint32_t count, eLootSourceType source, eInventoryType sourceInventory) {
		if (!owner || !owner->IsPlayer() || count == 0) return;
		const auto& list = Get();
		const auto it = list.find(lot);
		if (it == list.end() || !CountsAsAdded(source, sourceInventory) || !Applies(AccountLevel(owner), IgnoreStaff())) return;

		auto* character = owner->GetCharacter();
		if (!character) return;
		const auto entry = it->second;
		const bool remove = entry.action == IContraband::eContrabandAction::REMOVE;
		const auto sourceName = std::string(magic_enum::enum_name(source));
		// One flag per character, item and day for items that arrive (the load check flags each item on its own)
		Database::Get()->InsertEconomyFlag({ EconomyLedger::Today(), IDashboardAdmin::eFlagKind::CONTRABAND, character->GetID(), lot, 0,
			count, remove ? 1 : 0, std::string(remove ? "Removed when received" : "Received") + " (" + sourceName + ")" + Where() + ": " + entry.reason });
		if (!remove) return;

		// Take it away once the add (and whatever trade, mail or loot code called it) has finished
		owner->AddCallbackTimer(0.0f, [owner, lot, entry]() {
			auto* inventory = owner->GetComponent<InventoryComponent>();
			auto* character = owner->GetCharacter();
			if (!inventory || !character) return;
			const auto removed = RemoveLot(*inventory, lot, false);
			if (removed == 0) return;
			const auto accountId = character->GetParentUser() ? character->GetParentUser()->GetAccountID() : 0;
			Database::Get()->InsertAuditLog(0, ACTOR, "contraband_removed", character->GetName() + ": removed " + std::to_string(removed) + " of item " +
				std::to_string(lot) + " right after receiving it" + Where() + ": " + entry.reason, accountId, character->GetID());
			if (NotifyPlayers()) {
				ChatPackets::SendSystemMessage(owner->GetSystemAddress(), "An item you received (" + std::to_string(lot) + ") is not allowed on this server and was removed" +
					(entry.reason.empty() ? "." : ": " + entry.reason));
			}
		});
	}
}
