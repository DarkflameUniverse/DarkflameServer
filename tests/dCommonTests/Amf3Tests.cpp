#include <gtest/gtest.h>

#include "Amf3.h"

TEST(dCommonTests, Amf3AssociativeArrayTest) {

	AMFArrayValue array;
	array.InsertAssociative("true", true);
	array.InsertAssociative("false", false);

	// test associative can insert values
	ASSERT_EQ(array.GetAssociative().size(), 2);
	ASSERT_EQ(array.FindValue<AMFTrueValue>("true")->GetValueType(), eAmf::True);
	ASSERT_EQ(array.FindValue<AMFFalseValue>("false")->GetValueType(), eAmf::False);

	// Test associative can remove values
	array.RemoveAssociative("true");
	ASSERT_EQ(array.GetAssociative().size(), 1);
	ASSERT_EQ(array.FindValue<AMFTrueValue>("true"), nullptr);
	ASSERT_EQ(array.FindValue<AMFFalseValue>("false")->GetValueType(), eAmf::False);

	array.RemoveAssociative("false");
	ASSERT_EQ(array.GetAssociative().size(), 0);
	ASSERT_EQ(array.FindValue<AMFTrueValue>("true"), nullptr);
	ASSERT_EQ(array.FindValue<AMFFalseValue>("false"), nullptr);

	// Test that multiple of the same key respect only the first element of that key
	array.InsertAssociative("true", true);
	array.InsertAssociative("true", false);
	ASSERT_EQ(array.GetAssociative().size(), 1);
	ASSERT_EQ(array.FindValue<AMFTrueValue>("true")->GetValueType(), eAmf::True);
	array.RemoveAssociative("true");

	// Now test the dense portion
	// Get some out of bounds values and cast to incorrect template types
	array.PushDense(true);
	array.PushDense(false);

	ASSERT_EQ(array.GetDense().size(), 2);
	ASSERT_EQ(array.GetValueAt<AMFTrueValue>(0)->GetValueType(), eAmf::True);
	ASSERT_EQ(array.GetValueAt<AMFFalseValue>(0), nullptr);
	ASSERT_EQ(array.GetValueAt<AMFFalseValue>(1)->GetValueType(), eAmf::False);
	ASSERT_EQ(array.GetValueAt<AMFTrueValue>(155), nullptr);

	array.PopDense();

	ASSERT_EQ(array.GetDense().size(), 1);
	ASSERT_EQ(array.GetValueAt<AMFTrueValue>(0)->GetValueType(), eAmf::True);
	ASSERT_EQ(array.GetValueAt<AMFFalseValue>(0), nullptr);
	ASSERT_EQ(array.GetValueAt<AMFFalseValue>(1), nullptr);

	array.PopDense();

	ASSERT_EQ(array.GetDense().size(), 0);
	ASSERT_EQ(array.GetValueAt<AMFTrueValue>(0), nullptr);
	ASSERT_EQ(array.GetValueAt<AMFFalseValue>(0), nullptr);
	ASSERT_EQ(array.GetValueAt<AMFFalseValue>(1), nullptr);
}
