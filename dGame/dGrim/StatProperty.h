#pragma once

#ifndef __ESTAT__H__
#define __ESTAT__H__

#include <cstdint>
#include <string>

#include "StatModifier.h"
#include "StatTypes.h"

#define BASE_MULTIPLIER 100.0f

struct StatProperty
{
	eStatTypes type;
	eStatModifier modifier;
	float value;

	StatProperty(eStatTypes type, eStatModifier modifier, float value);

	std::string HtmlString();
};


#endif  //!__ESTAT__H__