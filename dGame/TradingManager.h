#pragma once

#include "Entity.h"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

struct TradeItem {
	LWOOBJID itemId;
	LOT itemLot;
	uint32_t itemCount;

	bool operator==(const TradeItem& other) const = default;
};

// What a player holds of an item they offered
struct HeldTradeItem {
	LOT lot;
	uint32_t count;
	// The trade window takes neither a bound item nor an equipped one
	bool bound = false;
	bool equipped = false;
	// Nor an item the player may not delete, nor a brick built model that is not approved
	bool deletable = true;
	bool approved = true;
};

struct LwoNameValue;

class InventoryComponent;

class Trade {
public:
	/**
	 * Checks the items a player offered against the items they hold.
	 * @param offered the items as the player sent them
	 * @param findHeld gives the held item with an ID, or nothing if the player has no such item
	 * @return the offer with every LOT taken from the held item and every count limited to the held count,
	 * or nothing if the offer names an item the player does not hold, the same item twice, a count of 0,
	 * a bound or equipped item, an item the player may not delete, a brick built model that is not approved,
	 * or more items than the trade window has slots
	 */
	static std::optional<std::vector<TradeItem>> ResolveOffer(
		const std::vector<TradeItem>& offered,
		const std::function<std::optional<HeldTradeItem>(LWOOBJID)>& findHeld);

	/**
	 * Gives the lookup ResolveOffer needs for the items in an inventory
	 */
	static std::function<std::optional<HeldTradeItem>(LWOOBJID)> HeldIn(const InventoryComponent& inventory);

	/**
	 * Whether the trade window takes an item for its moderation status. Only a brick built model has one,
	 * the userModelMod of its config, and only an approved one is taken.
	 */
	static bool IsApproved(LOT lot, const LwoNameValue& config);

	// The item a brick built model is
	static constexpr LOT BrickBuiltModelLot = 6662;

	// The userModelMod of an approved model
	static constexpr int32_t ApprovedModerationStatus = 1;

	// The slots one side of the trade window has
	static constexpr uint32_t MaxOfferedItems = 8;

	explicit Trade(LWOOBJID tradeId, LWOOBJID participantA, LWOOBJID participantB);
	~Trade();

	LWOOBJID GetTradeId() const;

	bool IsParticipant(LWOOBJID playerId) const;

	LWOOBJID GetParticipantA() const;
	LWOOBJID GetParticipantB() const;

	Entity* GetParticipantAEntity() const;
	Entity* GetParticipantBEntity() const;

	void SetCoins(LWOOBJID participant, uint64_t coins);
	void SetItems(LWOOBJID participant, std::vector<TradeItem> items);
	void SetAccepted(LWOOBJID participant, bool value);

	/**
	 * Whether a participant has given their final accept
	 */
	bool GetAccepted(LWOOBJID participant) const;

	/**
	 * Takes back both final accepts. A changed offer does this, as both clients do when one arrives.
	 */
	void ResetAccepted();

	void Complete();
	void Cancel(const LWOOBJID canceller);

	void SendUpdateToOther(LWOOBJID participant);

private:
	LWOOBJID m_TradeId = LWOOBJID_EMPTY;
	LWOOBJID m_ParticipantA = LWOOBJID_EMPTY;
	LWOOBJID m_ParticipantB = LWOOBJID_EMPTY;

	uint64_t m_CoinsA = 0;
	uint64_t m_CoinsB = 0;

	std::vector<TradeItem> m_ItemsA{};
	std::vector<TradeItem> m_ItemsB{};

	bool m_AcceptedA = false;
	bool m_AcceptedB = false;
};


class TradingManager {
public:
	static TradingManager* Instance() {
		if (!m_Address) {
			m_Address = new TradingManager();
		}

		return m_Address;
	}

	const std::unique_ptr<Trade>& GetTrade(LWOOBJID tradeId) const;
	const std::unique_ptr<Trade>& GetPlayerTrade(LWOOBJID playerId) const;
	void CancelTrade(const LWOOBJID canceller, LWOOBJID tradeId, const bool sendCancelMessage = true);
	void NewTrade(LWOOBJID participantA, LWOOBJID participantB);

private:
	static TradingManager* m_Address; //For singleton method

	std::unordered_map<LWOOBJID, std::unique_ptr<Trade>> trades;
};
