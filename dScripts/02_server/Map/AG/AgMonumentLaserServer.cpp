#include "AgMonumentLaserServer.h"

void AgMonumentLaserServer::OnStartup(Entity* self) {
	/*
	self->SetProximityRadius(m_Radius, "MonumentLaser");

	std::cout << "Monument Laser " << self->GetObjectID() << " is at " << self->GetPosition().GetX()
		<< ","<< self->GetPosition().GetY() << "," << self->GetPosition().GetZ() << std::endl;
	*/
}

void AgMonumentLaserServer::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	/*
	if (status == "ENTER") {

		std::cout << "Monument laser ID: " << self->GetObjectID() << std::endl;
	}
	*/
}

