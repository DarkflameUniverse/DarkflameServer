#include <stdexcept>

#include "NiPoint3.h"
#include "CommonCxxTests.h"

int TestNiPoint3(int argc, char** const argv) {
	// Check that Unitize works
	ASSERT_EQ(NiPoint3(3, 0, 0).Unitize(), NiPoint3::UNIT_X);
	// Check what unitize does to a vector of length 0
	ASSERT_EQ(NiPoint3::ZERO.Unitize(), NiPoint3::ZERO);
	// If we get here, all was successful
	return 0;
}
