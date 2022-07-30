#pragma once

class ChooseYourDestinationNsToNt
{
public:
	bool CheckChoice(Entity* self, Entity* player);
	void SetDestination(Entity* self, Entity* player);
	void BaseChoiceBoxRespond(Entity* self, Entity* sender, int32_t button, const std::u16string& buttonIdentifier, const std::u16string& identifier);
};
