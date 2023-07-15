#pragma once

#ifndef __ESKILLBAR_H__
#define __ESKILLBAR_H__

#include <cstdint>

enum class eSkillBar : int32_t {
	Primary,
	Gear,
	ClassPrimary,
	ClassSecondary,
	Consumable
};

#endif // __ESKILLBAR_H__