#pragma once
#include <functional>
#include <optional>
#include <vector>

#include "Entity.h"


enum class PreconditionType
{
	ItemEquipped,
	ItemNotEquipped,
	HasItem,
	DoesNotHaveItem,
	HasAchievement,
	MissionAvailable,
	OnMission,
	MissionComplete,
	PetDeployed,
	HasFlag,
	WithinShape,
	InBuild,
	TeamCheck,
	IsPetTaming,
	HasFaction,
	DoesNotHaveFaction,
	HasRacingLicence,
	DoesNotHaveRacingLicence,
	LegoClubMember,
	NoInteraction,
	NotFreeTrial,
	MissionActive,
	HasLevel,
	DoesNotHaveFlag = 23
};


struct ItemCost {
	LOT lot{ LOT_NULL };
	uint32_t count{ 0 };
	bool operator==(const ItemCost&) const = default;
};

class Precondition final
{
public:
	explicit Precondition(uint32_t condition);

	bool Check(Entity* player) const;

	// The items a HasItem precondition takes as a cost (a quickbuild's): the first of its LOTs the player has enough
	// of, and how many. Nothing for other types or when the player has none of them.
	std::optional<ItemCost> GetItemCost(Entity* player) const;

	// The pick GetItemCost makes, given how many of a LOT the player has
	static std::optional<ItemCost> PickItemCost(const std::vector<uint32_t>& lots, uint32_t count, const std::function<uint32_t(LOT)>& lotCount);

private:
	bool CheckValue(Entity* player, uint32_t value) const;

	PreconditionType type;

	std::vector<uint32_t> values;

	uint32_t count;
};


class PreconditionExpression final
{
public:
	explicit PreconditionExpression(const std::string& conditions);

	bool Check(Entity* player) const;

	// The item costs of the HasItem preconditions in this expression that the player meets
	std::vector<ItemCost> GetItemCosts(Entity* player) const;

	~PreconditionExpression();

private:
	uint32_t condition = 0;

	bool m_or = false;

	bool empty = false;

	PreconditionExpression* next = nullptr;
};

class Preconditions final
{
public:
	static bool Check(Entity* player, uint32_t condition);

	static const Precondition& Get(uint32_t condition);

	static PreconditionExpression CreateExpression(const std::string& conditions);

	~Preconditions();

private:
	static std::map<uint32_t, Precondition*> cache;
};
