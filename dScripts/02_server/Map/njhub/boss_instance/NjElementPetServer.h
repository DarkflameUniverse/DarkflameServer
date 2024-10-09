#pragma once
#include "PetFromObjectServer.h"

class NjElementPetServer : public PetFromObjectServer {
	void OnStartup(Entity* self) override;
	const std::string m_Precondition_Fire = "359";
	const std::string m_Precondition_Ice = "358";
	const std::string m_Precondition_Lightning = "357";
};
