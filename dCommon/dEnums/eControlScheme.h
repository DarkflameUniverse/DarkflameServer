#ifndef __ECONTROLSCHEME__H__
#define __ECONTROLSCHEME__H__

#include <cstdint>

enum class eControlScheme : uint32_t {
	SCHEME_A,
	SCHEME_D,
	SCHEME_GAMEPAD,
	SCHEME_E,
	SCHEME_FPS,
	SCHEME_DRIVING,
	SCHEME_TAMING,
	SCHEME_MODULAR_BUILD,
	SCHEME_WEAR_A_ROBOT //== freecam?
};

#endif  //!__ECONTROLSCHEME__H__
