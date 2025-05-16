#include "WorldPackets.h"
#include "Amf3.h"
#include "dConfig.h"
#include "GameMessages.h"
#include "Entity.h"

namespace WorldPackets {
	
	bool UIHelpTop5::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(languageCode));
		return true;
	}

	void UIHelpTop5::Handle() {
		AMFArrayValue data;
		switch (languageCode) {
			case eLanguageCodeID::EN_US:
				// Summaries
				data.Insert("Summary0", Game::config->GetValue("help_0_summary"));
				data.Insert("Summary1", Game::config->GetValue("help_1_summary"));
				data.Insert("Summary2", Game::config->GetValue("help_2_summary"));
				data.Insert("Summary3", Game::config->GetValue("help_3_summary"));
				data.Insert("Summary4", Game::config->GetValue("help_4_summary"));

				// Descriptions
				data.Insert("Description0", Game::config->GetValue("help_0_description"));
				data.Insert("Description1", Game::config->GetValue("help_1_description"));
				data.Insert("Description2", Game::config->GetValue("help_2_description"));
				data.Insert("Description3", Game::config->GetValue("help_3_description"));
				data.Insert("Description4", Game::config->GetValue("help_4_description"));
				break;
			case eLanguageCodeID::PL_US:
			case eLanguageCodeID::DE_DE:
			case eLanguageCodeID::EN_GB:
			default:
				break;
		}
		GameMessages::SendUIMessageServerToSingleClient(player, sysAddr, "UIHelpTop5", data);
	}
	

}
