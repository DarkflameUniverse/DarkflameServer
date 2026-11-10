#include <gtest/gtest.h>

#include "InstanceLimits.h"

using InstanceLimits::CheckNewInstance;
using InstanceLimits::eNewInstance;
using InstanceLimits::TransferFailure;

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

TEST(InstanceLimitsTest, ARefusedInstanceHasAReasonForThePlayer) {
	// The limit: nothing is free to start the zone on
	EXPECT_EQ(TransferFailure(CheckNewInstance(1000, true, 256, 256)), eTransferFailure::NoServer);
	// No row in the ZoneTable: the zone can not be loaded
	EXPECT_EQ(TransferFailure(CheckNewInstance(31337, false, 0, 256)), eTransferFailure::MapLoad);
	// Refused for another reason, such as a master server that shuts down
	EXPECT_EQ(TransferFailure(eNewInstance::Allowed), eTransferFailure::Other);
}

TEST(InstanceLimitsTest, TheReasonsAreTheValuesTheClientKnows) {
	EXPECT_EQ(static_cast<uint16_t>(eTransferFailure::NoServer), 0);
	EXPECT_EQ(static_cast<uint16_t>(eTransferFailure::MapLoad), 1);
	EXPECT_EQ(static_cast<uint16_t>(eTransferFailure::InstanceFull), 2);
	EXPECT_EQ(static_cast<uint16_t>(eTransferFailure::Other), 3);
}
