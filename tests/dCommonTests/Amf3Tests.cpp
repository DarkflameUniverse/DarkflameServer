#include <gtest/gtest.h>

#include "Amf3.h"

TEST(dCommonTests, Amf3AssociativeArrayTest) {

	AMFArrayValue array;
	array.InsertAssociative("true", true);
	array.InsertAssociative("false", false);

	// test associative can insert values
	ASSERT_EQ(array.GetAssociative().size(), 2);
	ASSERT_EQ(array.FindValue<AMFTrueValue>("true")->GetValueType(), AMFValueType::AMFTrue);
	ASSERT_EQ(array.FindValue<AMFFalseValue>("false")->GetValueType(), AMFValueType::AMFFalse);

	// Test associative can remove values
	array.RemoveAssociative("true");
	ASSERT_EQ(array.GetAssociative().size(), 1);
	ASSERT_EQ(array.FindValue<AMFTrueValue>("true"), nullptr);
	ASSERT_EQ(array.FindValue<AMFFalseValue>("false")->GetValueType(), AMFValueType::AMFFalse);

	array.RemoveAssociative("false");
	ASSERT_EQ(array.GetAssociative().size(), 0);
	ASSERT_EQ(array.FindValue<AMFTrueValue>("true"), nullptr);
	ASSERT_EQ(array.FindValue<AMFFalseValue>("false"), nullptr);

	// Test that multiple of the same key respect only the first element of that key
	array.InsertAssociative("true", true);
	array.InsertAssociative("true", false);
	ASSERT_EQ(array.GetAssociative().size(), 1);
	ASSERT_EQ(array.FindValue<AMFTrueValue>("true")->GetValueType(), AMFValueType::AMFTrue);
	array.RemoveAssociative("true");

	// Now test the dense portion
	// Get some out of bounds values and cast to incorrect template types
	array.PushDense(true);
	array.PushDense(false);

	ASSERT_EQ(array.GetDense().size(), 2);
	ASSERT_EQ(array.GetValueAt<AMFTrueValue>(0)->GetValueType(), AMFValueType::AMFTrue);
	ASSERT_EQ(array.GetValueAt<AMFFalseValue>(0), nullptr);
	ASSERT_EQ(array.GetValueAt<AMFFalseValue>(1)->GetValueType(), AMFValueType::AMFFalse);
	ASSERT_EQ(array.GetValueAt<AMFTrueValue>(155), nullptr);

	array.PopDense();

	ASSERT_EQ(array.GetDense().size(), 1);
	ASSERT_EQ(array.GetValueAt<AMFTrueValue>(0)->GetValueType(), AMFValueType::AMFTrue);
	ASSERT_EQ(array.GetValueAt<AMFFalseValue>(0), nullptr);
	ASSERT_EQ(array.GetValueAt<AMFFalseValue>(1), nullptr);

	array.PopDense();

	ASSERT_EQ(array.GetDense().size(), 0);
	ASSERT_EQ(array.GetValueAt<AMFTrueValue>(0), nullptr);
	ASSERT_EQ(array.GetValueAt<AMFFalseValue>(0), nullptr);
	ASSERT_EQ(array.GetValueAt<AMFFalseValue>(1), nullptr);
}
