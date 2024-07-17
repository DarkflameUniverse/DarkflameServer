#include <Entity.h>

namespace nejlika::NejlikaHelpers
{

void RenderDamageText(const std::string& text, Entity* attacker, Entity* damaged, 
	float scale = 1, int32_t fontSize = 4, int32_t colorR = 255, int32_t colorG = 255, int32_t colorB = 255, int32_t colorA = 0);

}
