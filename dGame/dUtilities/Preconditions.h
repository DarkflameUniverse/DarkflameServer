#pragma once
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
	HasLevel = 22
};


class Precondition final
{
public:
	explicit Precondition(uint32_t condition);

	bool Check(Entity* player, bool evaluateCosts = false) const;

private:
	bool CheckValue(Entity* player, uint32_t value, bool evaluateCosts = false) const;

	PreconditionType type;

	std::vector<uint32_t> values;

	uint32_t count;
};


class PreconditionExpression final
{
public:
	explicit PreconditionExpression(const std::string& conditions);

	bool Check(Entity* player, bool evaluateCosts = false) const;

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
	static bool Check(Entity* player, uint32_t condition, bool evaluateCosts = false);

	static PreconditionExpression CreateExpression(const std::string& conditions);

	~Preconditions();

private:
	static std::map<uint32_t, Precondition*> cache;
};
