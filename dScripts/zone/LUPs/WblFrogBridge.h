#pragma once
#include "CppScripts.h"

// WBL_Frog_Bridge (Portabello): the frog puts its tongue out as a bridge (the four FrogBridge pieces move to their
// second waypoint) when a player who finished mission 946 comes near, then pulls it back piece by piece
class WblFrogBridge : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
private:
	static void StickOutTongue(Entity* self);
	static void MovePiece(int piece, uint32_t waypoint);
};
