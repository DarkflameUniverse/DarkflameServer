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

TEST(dCommonTests, Amf3InsertionAssociativeTest) {
	AMFArrayValue array;
	array.InsertAssociative("CString", "string");
	array.InsertAssociative("String", std::string("string"));
	array.InsertAssociative("False", false);
	array.InsertAssociative("True", true);
	array.InsertAssociative("Integer", 42U);
	array.InsertAssociative("Double", 42.0);
	array.InsertAssociativeArray("Array");
	array.InsertAssociativeUndefined("Undefined");
	array.InsertAssociativeNull("Null");

	ASSERT_EQ(array.FindValue<AMFStringValue>("CString")->GetValueType(), eAmf::String);
	ASSERT_EQ(array.FindValue<AMFStringValue>("String")->GetValueType(), eAmf::String);
	ASSERT_EQ(array.FindValue<AMFFalseValue>("False")->GetValueType(), eAmf::False);
	ASSERT_EQ(array.FindValue<AMFTrueValue>("True")->GetValueType(), eAmf::True);
	ASSERT_EQ(array.FindValue<AMFIntegerValue>("Integer")->GetValueType(), eAmf::Integer);
	ASSERT_EQ(array.FindValue<AMFDoubleValue>("Double")->GetValueType(), eAmf::Double);
	ASSERT_EQ(array.FindValue<AMFArrayValue>("Array")->GetValueType(), eAmf::Array);
	ASSERT_EQ(array.FindValue<AMFNullValue>("Null")->GetValueType(), eAmf::Null);
	ASSERT_EQ(array.FindValue<AMFUndefinedValue>("Undefined")->GetValueType(), eAmf::Undefined);
}

TEST(dCommonTests, Amf3InsertionDenseTest) {
	AMFArrayValue array;
	array.PushDense(std::string("string"));
	array.PushDense("CString");
	array.PushDense(false);
	array.PushDense(true);
	array.PushDense(42U);
	array.PushDense(42.0);
	array.PushDenseArray();
	array.PushDenseNull();
	array.PushDenseUndefined();

	ASSERT_EQ(array.GetValueAt<AMFStringValue>(0)->GetValueType(), eAmf::String);
	ASSERT_EQ(array.GetValueAt<AMFStringValue>(1)->GetValueType(), eAmf::String);
	ASSERT_EQ(array.GetValueAt<AMFFalseValue>(2)->GetValueType(), eAmf::False);
	ASSERT_EQ(array.GetValueAt<AMFTrueValue>(3)->GetValueType(), eAmf::True);
	ASSERT_EQ(array.GetValueAt<AMFIntegerValue>(4)->GetValueType(), eAmf::Integer);
	ASSERT_EQ(array.GetValueAt<AMFDoubleValue>(5)->GetValueType(), eAmf::Double);
	ASSERT_EQ(array.GetValueAt<AMFArrayValue>(6)->GetValueType(), eAmf::Array);
	ASSERT_EQ(array.GetValueAt<AMFNullValue>(7)->GetValueType(), eAmf::Null);
	ASSERT_EQ(array.GetValueAt<AMFUndefinedValue>(8)->GetValueType(), eAmf::Undefined);
}
