#include "EditMode.h"

#include <algorithm>

#include "CharacterComponent.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "dConfig.h"

eGameMasterLevel EditMode::RequiredLevel() {
	const auto level = GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("editor_gm_level")).value_or(CharacterComponent::DEFAULT_EDITOR_GM_LEVEL);
	return static_cast<eGameMasterLevel>(std::clamp<uint32_t>(level, 1, 9));
}
