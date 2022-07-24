#include <gtest/gtest.h>

#include "NiPoint3.h"

/**
 * @brief Basic test for NiPoint3 functionality
 * 
 */
TEST(dCommonTests, NiPoint3Test){
	// Check that Unitize works
	ASSERT_EQ(NiPoint3(3,0,0).Unitize(), NiPoint3::UNIT_X);
	// Check what unitize does to a vector of length 0
	ASSERT_EQ(NiPoint3::ZERO.Unitize(), NiPoint3::ZERO);
}
