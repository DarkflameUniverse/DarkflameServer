#include <gtest/gtest.h>

#include <vector>

#include "Amf3.h"

TEST(dCommonTests, AMF3AssociativeArrayTest) {

	AMFArrayValue array;
	array.Insert("true", true);
	array.Insert("false", false);

	// test associative can insert values
	ASSERT_EQ(array.GetAssociative().size(), 2);
	ASSERT_EQ(array.Get<bool>("true")->GetValueType(), eAmf::True);
	ASSERT_EQ(array.Get<bool>("false")->GetValueType(), eAmf::False);

	// Test associative can remove values
	array.Remove("true");
	ASSERT_EQ(array.GetAssociative().size(), 1);
	ASSERT_EQ(array.Get<bool>("true"), nullptr);
	ASSERT_EQ(array.Get<bool>("false")->GetValueType(), eAmf::False);

	array.Remove("false");
	ASSERT_EQ(array.GetAssociative().size(), 0);
	ASSERT_EQ(array.Get<bool>("true"), nullptr);
	ASSERT_EQ(array.Get<bool>("false"), nullptr);

	// Test that multiple of the same key respect only the first element of that key
	array.Insert("true", true);
	array.Insert("true", false);
	ASSERT_EQ(array.GetAssociative().size(), 1);
	ASSERT_EQ(array.Get<bool>("true")->GetValueType(), eAmf::True);
	array.Remove("true");

	// Now test the dense portion
	// Get some out of bounds values and cast to incorrect template types
	array.Push(true);
	array.Push(false);

	ASSERT_EQ(array.GetDense().size(), 2);
	ASSERT_EQ(array.Get<bool>(0)->GetValueType(), eAmf::True);
	ASSERT_EQ(array.Get<std::string>(0), nullptr);
	ASSERT_EQ(array.Get<bool>(1)->GetValueType(), eAmf::False);
	ASSERT_EQ(array.Get<bool>(155), nullptr);

	array.Pop();

	ASSERT_EQ(array.GetDense().size(), 1);
	ASSERT_EQ(array.Get<bool>(0)->GetValueType(), eAmf::True);
	ASSERT_EQ(array.Get<std::string>(0), nullptr);
	ASSERT_EQ(array.Get<bool>(1), nullptr);

	array.Pop();

	ASSERT_EQ(array.GetDense().size(), 0);
	ASSERT_EQ(array.Get<bool>(0), nullptr);
	ASSERT_EQ(array.Get<std::string>(0), nullptr);
	ASSERT_EQ(array.Get<bool>(1), nullptr);
}

TEST(dCommonTests, AMF3InsertionAssociativeTest) {
	AMFArrayValue array;
	array.Insert("CString", "string");
	array.Insert("String", std::string("string"));
	array.Insert("False", false);
	array.Insert("True", true);
	array.Insert<int32_t>("Integer", 42U);
	array.Insert("Double", 42.0);
	array.InsertArray("Array");
	array.Insert<std::vector<uint32_t>>("Undefined", {});
	array.Insert("Null", nullptr);

	ASSERT_EQ(array.Get<const char*>("CString")->GetValueType(), eAmf::String);
	ASSERT_EQ(array.Get<std::string>("String")->GetValueType(), eAmf::String);
	ASSERT_EQ(array.Get<bool>("False")->GetValueType(), eAmf::False);
	ASSERT_EQ(array.Get<bool>("True")->GetValueType(), eAmf::True);
	ASSERT_EQ(array.Get<int32_t>("Integer")->GetValueType(), eAmf::Integer);
	ASSERT_EQ(array.Get<double>("Double")->GetValueType(), eAmf::Double);
	ASSERT_EQ(array.GetArray("Array")->GetValueType(), eAmf::Array);
	ASSERT_EQ(array.Get<nullptr_t>("Null")->GetValueType(), eAmf::Null);
	ASSERT_EQ(array.Get<std::vector<uint32_t>>("Undefined")->GetValueType(), eAmf::Undefined);
}

TEST(dCommonTests, AMF3InsertionDenseTest) {
	AMFArrayValue array;
	array.Push<std::string>("string");
	array.Push("CString");
	array.Push(false);
	array.Push(true);
	array.Push<int32_t>(42U);
	array.Push(42.0);
	array.PushArray();
	array.Push(nullptr);
	array.Push<std::vector<uint32_t>>({});

	ASSERT_EQ(array.Get<std::string>(0)->GetValueType(), eAmf::String);
	ASSERT_EQ(array.Get<const char*>(1)->GetValueType(), eAmf::String);
	ASSERT_EQ(array.Get<bool>(2)->GetValueType(), eAmf::False);
	ASSERT_EQ(array.Get<bool>(3)->GetValueType(), eAmf::True);
	ASSERT_EQ(array.Get<int32_t>(4)->GetValueType(), eAmf::Integer);
	ASSERT_EQ(array.Get<double>(5)->GetValueType(), eAmf::Double);
	ASSERT_EQ(array.GetArray(6)->GetValueType(), eAmf::Array);
	ASSERT_EQ(array.Get<nullptr_t>(7)->GetValueType(), eAmf::Null);
	ASSERT_EQ(array.Get<std::vector<uint32_t>>(8)->GetValueType(), eAmf::Undefined);
}
