#include "NejlikaHelpers.h"


void nejlika::NejlikaHelpers::RenderDamageText(const std::string & text, Entity * attacker, Entity * damaged,
	float scale, int32_t fontSize, int32_t colorR, int32_t colorG, int32_t colorB, int32_t colorA)
{
	if (damaged == nullptr) {
		return;
	}

	auto damagedPosition = damaged->GetPosition();

	// Add a slight random offset to the damage position
	damagedPosition.x += (rand() % 10 - 5) / 5.0f;
	damagedPosition.y += (rand() % 10 - 5) / 5.0f;
	damagedPosition.z += (rand() % 10 - 5) / 5.0f;

	const auto& damageText = text;

	std::stringstream damageUIMessage;

	damageUIMessage << 0.0825 << ";" << 0.12 << ";" << damagedPosition.x << ";" << damagedPosition.y + 4.5f << ";" << damagedPosition.z << ";" << 0.1 << ";";
	damageUIMessage << 200 * scale << ";" << 200 * scale << ";" << 0.5 << ";" << 1.0 << ";" << damageText << ";" << 4 << ";" << fontSize << ";" << colorR << ";" << colorG << ";" << colorB << ";";
	damageUIMessage << colorA;

	const auto damageUIStr = damageUIMessage.str();

	if (damaged->IsPlayer()) {
		damaged->SetNetworkVar<std::string>(u"renderText", damageUIStr, UNASSIGNED_SYSTEM_ADDRESS);
	} else if (attacker != nullptr && attacker->IsPlayer()) {
		attacker->SetNetworkVar<std::string>(u"renderText", damageUIStr, UNASSIGNED_SYSTEM_ADDRESS);
	}
}