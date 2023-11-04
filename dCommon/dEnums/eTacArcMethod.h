#pragma once

#ifndef __ETACARCMETHOD__H__
#define __ETACARCMETHOD__H__

#include <cstdint>

enum class eTacArcMethod : uint32_t {
	PIE_SLICE = 0,
	CONE,
	FRUSTUM
};

#endif  //!__ETACARCMETHOD__H__



