#include "PropertyBuilders.h"

#include "BrickByBrick.h"
#include "dConfig.h"
#include "Game.h"

bool PropertyBuilders::BestFriendsBuild() {
	return Game::config && Game::config->GetValue("property_bff_build") == "1";
}

bool PropertyBuilders::CanBuild(const Player& player, const LWOOBJID owner, const bool ownerBuilding, const bool bestFriendsBuild) {
	if (owner == LWOOBJID_EMPTY || player.id == LWOOBJID_EMPTY) return false;
	if (player.id == owner) return true;
	if (!bestFriendsBuild || !player.isBestFriend) return false;
	// Joining needs the owner building; staying doesn't
	return ownerBuilding || player.isBuilding;
}

LWOOBJID PropertyBuilders::Placer(const LWOOBJID placedBy, const LWOOBJID owner) {
	return placedBy == LWOOBJID_EMPTY ? owner : placedBy;
}

PropertyBuilders::eModelReturn PropertyBuilders::PlanModelReturn(const LWOOBJID picker, const LWOOBJID placer, const bool placerHere, const int32_t deleteReason) {
	if (picker == placer) return eModelReturn::PICKER;
	// Taking a model apart opens it from the picker's own inventory (BBBLoadItemRequest), where it wouldn't be
	if (static_cast<BrickByBrick::eDeleteReason>(deleteReason) == BrickByBrick::eDeleteReason::BREAKING_MODEL_APART) return eModelReturn::NOT_THEIRS;
	return placerHere ? eModelReturn::PLACER : eModelReturn::PLACER_AWAY;
}
