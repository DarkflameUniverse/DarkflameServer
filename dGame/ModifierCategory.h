#pragma once

#include <cstdint>

namespace nejlika
{

enum class ModifierCategory : uint8_t
{
	Player = 0 << 0,
	Pet = 1 << 0
};

}
