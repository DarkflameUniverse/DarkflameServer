#pragma once
#include "PetFromObjectServer.h"

class NjEarthPetServer : public PetFromObjectServer {
	void OnStartup(Entity* self) override;
	const std::string m_Precondition = "279";
};
