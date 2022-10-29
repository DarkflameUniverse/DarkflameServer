#pragma once

#include "Entity.h"

struct TradeItem
{
	LWOOBJID itemId;
	LOT itemLot;
	uint32_t itemCount;
};

class Trade
{
public:
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

	void Complete();
	void Cancel();

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


class TradingManager
{
public:
	static TradingManager* Instance() {
		if (!m_Address) {
			m_Address = new TradingManager();
		}

		return m_Address;
	}

	explicit TradingManager();
	~TradingManager();

	Trade* GetTrade(LWOOBJID tradeId) const;
	Trade* GetPlayerTrade(LWOOBJID playerId) const;
	void CancelTrade(LWOOBJID tradeId);
	Trade* NewTrade(LWOOBJID participantA, LWOOBJID participantB);

private:
	static TradingManager* m_Address; //For singleton method

	std::unordered_map<LWOOBJID, Trade*> trades;
};
