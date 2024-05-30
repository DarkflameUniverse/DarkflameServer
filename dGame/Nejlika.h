#pragma once

namespace nejlika
{

void Initalize();

void ItemDescription(Entity* entity, const SystemAddress& sysAddr, const std::string args);

enum class ItemModifierType : uint8_t
{
	Health,
	Armor,
	Imagination,
	Slashing,
	Piercing,
	Bludgeoning,
	Fire,
	Cold,
	Lightning,
	Corruption,
	Psychic
};

struct ItemModifier
{
	ItemModifierType type;
	float value;
	bool isPercentage;

	ItemModifier(ItemModifierType type, float value, bool isPercentage) : type(type), value(value), isPercentage(isPercentage) {}

	ItemModifier(const std::string& config);

	std::string ToString() const;

	std::string GenerateHtmlString() const;
};

enum class ItemNameType : uint8_t
{
	Common,
	Uncommon,
	Rare,
	Epic,
	Legendary,
	Relic
};

struct ItemName
{
	ItemNameType type;
	std::string name;
	bool prefix;

	ItemName(ItemNameType type, const std::string& name, bool prefix) : type(type), name(name), prefix(prefix) {}

	ItemName(const std::string& config);

	std::string ToString() const;

	std::string GenerateHtmlString() const;

	static std::string GenerateHtmlString(const std::vector<ItemName>& names);
};

}