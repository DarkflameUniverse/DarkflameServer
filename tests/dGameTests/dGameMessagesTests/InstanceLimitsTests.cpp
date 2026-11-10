#include <gtest/gtest.h>

#include "InstanceLimits.h"

using InstanceLimits::CheckNewInstance;
using InstanceLimits::eNewInstance;

TEST(InstanceLimitsTest, AZoneOfTheZoneTableIsStarted) {
	EXPECT_EQ(CheckNewInstance(1000, true, 0, 256), eNewInstance::Allowed);
	EXPECT_EQ(CheckNewInstance(1150, true, 255, 256), eNewInstance::Allowed);
}

TEST(InstanceLimitsTest, AZoneTheZoneTableDoesNotHaveIsRefused) {
	EXPECT_EQ(CheckNewInstance(31337, false, 0, 256), eNewInstance::UnknownZone);
	EXPECT_EQ(CheckNewInstance(31337, false, 0, 0), eNewInstance::UnknownZone);
	// The unknown zone is the reason given, also when the limit is reached too
	EXPECT_EQ(CheckNewInstance(31337, false, 256, 256), eNewInstance::UnknownZone);
}

TEST(InstanceLimitsTest, NoInstanceIsStartedPastTheLimit) {
	EXPECT_EQ(CheckNewInstance(1000, true, 256, 256), eNewInstance::TooManyInstances);
	EXPECT_EQ(CheckNewInstance(1000, true, 1000, 256), eNewInstance::TooManyInstances);
	EXPECT_EQ(CheckNewInstance(1000, true, 1, 1), eNewInstance::TooManyInstances);
}

TEST(InstanceLimitsTest, ALimitOfZeroIsNoLimit) {
	EXPECT_EQ(CheckNewInstance(1000, true, 100000, 0), eNewInstance::Allowed);
}

TEST(InstanceLimitsTest, TheCharacterSelectIsAlwaysStarted) {
	// Zone 0 has no row in the ZoneTable
	EXPECT_EQ(CheckNewInstance(0, false, 0, 256), eNewInstance::Allowed);
	EXPECT_EQ(CheckNewInstance(0, false, 256, 256), eNewInstance::Allowed);
}
