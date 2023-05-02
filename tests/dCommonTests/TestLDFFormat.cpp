#include "LDFFormat.h"

#include <gtest/gtest.h>

#include "Game.h"
#include "dCommonDependencies.h"
#include "dLogger.h"

class LDFTests : public dCommonDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
	}

	void TearDown() override {
		TearDownDependencies();
	}
};

#define LdfUniquePtr std::unique_ptr<LDFBaseData>  

// Suite of tests for parsing LDF values

TEST_F(LDFTests, LDFUTF16Test) {
	std::string testWord = "KEY=0:IAmA weird string with :::: and spac,./;'][\\es that I expect to be parsed correctly...; ";
	LdfUniquePtr data(LDFBaseData::DataFromString(testWord));
	ASSERT_NE(data, nullptr);
	ASSERT_EQ(data->GetValueType(), eLDFType::LDF_TYPE_UTF_16);
	ASSERT_EQ(data->GetKey(), u"KEY");
	ASSERT_EQ(((LDFData<std::u16string>*)data.get())->GetValue(), u"IAmA weird string with :::: and spac,./;'][\\es that I expect to be parsed correctly...; ");
	ASSERT_EQ(data->GetString(), testWord);
}

TEST_F(LDFTests, LDFUTF16EmptyTest) {
	std::string testWord = "KEY=0:";
	LdfUniquePtr data(LDFBaseData::DataFromString(testWord));
	ASSERT_NE(data, nullptr);
	ASSERT_EQ(data->GetValueType(), eLDFType::LDF_TYPE_UTF_16);
	ASSERT_EQ(data->GetKey(), u"KEY");
	ASSERT_EQ(((LDFData<std::u16string>*)data.get())->GetValue(), u"");
	ASSERT_EQ(data->GetString(), testWord);
}

TEST_F(LDFTests, LDFUTF16ColonTest) {
	std::string testWord = "KEY=0:::";
	LdfUniquePtr data(LDFBaseData::DataFromString(testWord));
	ASSERT_NE(data, nullptr);
	ASSERT_EQ(data->GetValueType(), eLDFType::LDF_TYPE_UTF_16);
	ASSERT_EQ(data->GetKey(), u"KEY");
	ASSERT_EQ(((LDFData<std::u16string>*)data.get())->GetValue(), u"::");
	ASSERT_EQ(data->GetString(), testWord);
}

TEST_F(LDFTests, LDFUTF16EqualsTest) {
	std::string testWord = "KEY=0:==";
	LdfUniquePtr data(LDFBaseData::DataFromString(testWord));
	ASSERT_NE(data, nullptr);
	ASSERT_EQ(data->GetValueType(), eLDFType::LDF_TYPE_UTF_16);
	ASSERT_EQ(data->GetKey(), u"KEY");
	ASSERT_EQ(((LDFData<std::u16string>*)data.get())->GetValue(), u"==");
	ASSERT_EQ(data->GetString(), testWord);
}

TEST_F(LDFTests, LDFS32Test) {
	LdfUniquePtr data(LDFBaseData::DataFromString("KEY=1:-15"));
	ASSERT_NE(data, nullptr);
	ASSERT_EQ(data->GetValueType(), eLDFType::LDF_TYPE_S32);
	ASSERT_EQ(data->GetKey(), u"KEY");
	ASSERT_EQ(((LDFData<int32_t>*)data.get())->GetValue(), -15);
	ASSERT_EQ(data->GetString(), "KEY=1:-15");
}
TEST_F(LDFTests, LDFU32Test) {
	LdfUniquePtr data(LDFBaseData::DataFromString("KEY=5:15"));
	ASSERT_NE(data, nullptr);
	ASSERT_EQ(data->GetValueType(), eLDFType::LDF_TYPE_U32);
	ASSERT_EQ(data->GetKey(), u"KEY");
	ASSERT_EQ(((LDFData<uint32_t>*)data.get())->GetValue(), 15);
	ASSERT_EQ(data->GetString(), "KEY=5:15");
}

TEST_F(LDFTests, LDFU32TrueTest) {
	LdfUniquePtr data(LDFBaseData::DataFromString("KEY=5:true"));
	ASSERT_NE(data, nullptr);
	ASSERT_EQ(data->GetValueType(), eLDFType::LDF_TYPE_U32);
	ASSERT_EQ(data->GetKey(), u"KEY");
	ASSERT_EQ(((LDFData<uint32_t>*)data.get())->GetValue(), 1);
	ASSERT_EQ(data->GetString(), "KEY=5:1");
}

TEST_F(LDFTests, LDFU32FalseTest) {
	LdfUniquePtr data(LDFBaseData::DataFromString("KEY=5:false"));
	ASSERT_NE(data, nullptr);
	ASSERT_EQ(data->GetValueType(), eLDFType::LDF_TYPE_U32);
	ASSERT_EQ(data->GetKey(), u"KEY");
	ASSERT_EQ(((LDFData<uint32_t>*)data.get())->GetValue(), 0);
	ASSERT_EQ(data->GetString(), "KEY=5:0");
}


// Use find since floats and doubles generally have appended 0s
TEST_F(LDFTests, LDFFloatTest) {
	LdfUniquePtr data(LDFBaseData::DataFromString("KEY=3:15.5"));
	ASSERT_NE(data, nullptr);
	ASSERT_EQ(data->GetValueType(), eLDFType::LDF_TYPE_FLOAT);
	ASSERT_EQ(data->GetKey(), u"KEY");
	ASSERT_EQ(((LDFData<float>*)data.get())->GetValue(), 15.5f);
	ASSERT_EQ(data->GetString().find("KEY=3:15.5"), 0);
}

TEST_F(LDFTests, LDFDoubleTest) {
	LdfUniquePtr data(LDFBaseData::DataFromString("KEY=4:15.5"));
	ASSERT_NE(data, nullptr);
	ASSERT_EQ(data->GetValueType(), eLDFType::LDF_TYPE_DOUBLE);
	ASSERT_EQ(data->GetKey(), u"KEY");
	ASSERT_EQ(((LDFData<double>*)data.get())->GetValue(), 15.5);
	ASSERT_EQ(data->GetString().find("KEY=4:15.5"), 0);
}


TEST_F(LDFTests, LDFBoolTrueTest) {
	LdfUniquePtr data(LDFBaseData::DataFromString("KEY=7:true"));
	ASSERT_NE(data, nullptr);
	ASSERT_EQ(data->GetValueType(), eLDFType::LDF_TYPE_BOOLEAN);
	ASSERT_EQ(data->GetKey(), u"KEY");
	ASSERT_EQ(((LDFData<bool>*)data.get())->GetValue(), true);
	ASSERT_EQ(data->GetString(), "KEY=7:1");
}

TEST_F(LDFTests, LDFBoolFalseTest) {
	LdfUniquePtr data(LDFBaseData::DataFromString("KEY=7:false"));
	ASSERT_NE(data, nullptr);
	ASSERT_EQ(data->GetValueType(), eLDFType::LDF_TYPE_BOOLEAN);
	ASSERT_EQ(data->GetKey(), u"KEY");
	ASSERT_EQ(((LDFData<bool>*)data.get())->GetValue(), false);
	ASSERT_EQ(data->GetString(), "KEY=7:0");
}

TEST_F(LDFTests, LDFBoolIntTest) {
	LdfUniquePtr data(LDFBaseData::DataFromString("KEY=7:3"));
	ASSERT_NE(data, nullptr);
	ASSERT_EQ(data->GetValueType(), eLDFType::LDF_TYPE_BOOLEAN);
	ASSERT_EQ(data->GetKey(), u"KEY");
	ASSERT_EQ(((LDFData<bool>*)data.get())->GetValue(), true);
	ASSERT_EQ(data->GetString(), "KEY=7:1");
}

TEST_F(LDFTests, LDFU64Test) {
	LdfUniquePtr data(LDFBaseData::DataFromString("KEY=8:15"));
	ASSERT_NE(data, nullptr);
	ASSERT_EQ(data->GetValueType(), eLDFType::LDF_TYPE_U64);
	ASSERT_EQ(data->GetKey(), u"KEY");
	ASSERT_EQ(((LDFData<uint64_t>*)data.get())->GetValue(), 15);
	ASSERT_EQ(data->GetString(), "KEY=8:15");
}

TEST_F(LDFTests, LDFLWOOBJIDTest) {
	LdfUniquePtr data(LDFBaseData::DataFromString("KEY=9:15"));
	ASSERT_NE(data, nullptr);
	ASSERT_EQ(data->GetValueType(), eLDFType::LDF_TYPE_OBJID);
	ASSERT_EQ(data->GetKey(), u"KEY");
	ASSERT_EQ(((LDFData<uint64_t>*)data.get())->GetValue(), 15);
	ASSERT_EQ(data->GetString(), "KEY=9:15");
}

TEST_F(LDFTests, LDFUTF8Test) {
	std::string testWord = "KEY=13:IAmA weird string with :::: and spac,./;'][\\es that I expect to be parsed correctly...; ";
	LdfUniquePtr data(LDFBaseData::DataFromString(testWord));
	ASSERT_NE(data, nullptr);
	ASSERT_EQ(data->GetValueType(), eLDFType::LDF_TYPE_UTF_8);
	ASSERT_EQ(data->GetKey(), u"KEY");
	ASSERT_EQ(((LDFData<std::string>*)data.get())->GetValue(), "IAmA weird string with :::: and spac,./;'][\\es that I expect to be parsed correctly...; ");
	ASSERT_EQ(data->GetString(), testWord);
}

TEST_F(LDFTests, LDFUTF8EmptyTest) {
	std::string testWord = "KEY=13:";
	LdfUniquePtr data(LDFBaseData::DataFromString(testWord));
	ASSERT_NE(data, nullptr);
	ASSERT_EQ(data->GetValueType(), eLDFType::LDF_TYPE_UTF_8);
	ASSERT_EQ(data->GetKey(), u"KEY");
	ASSERT_EQ(((LDFData<std::string>*)data.get())->GetValue(), "");
	ASSERT_EQ(data->GetString(), testWord);
}

TEST_F(LDFTests, LDFUTF8ColonsTest) {
	std::string testWord = "KEY=13:::";
	LdfUniquePtr data(LDFBaseData::DataFromString(testWord));
	ASSERT_NE(data, nullptr);
	ASSERT_EQ(data->GetValueType(), eLDFType::LDF_TYPE_UTF_8);
	ASSERT_EQ(data->GetKey(), u"KEY");
	ASSERT_EQ(((LDFData<std::string>*)data.get())->GetValue(), "::");
	ASSERT_EQ(data->GetString(), testWord);
}
TEST_F(LDFTests, LDFUTF8EqualsTest) {
	std::string testWord = "KEY=13:==";
	LdfUniquePtr data(LDFBaseData::DataFromString(testWord));
	ASSERT_NE(data, nullptr);
	ASSERT_EQ(data->GetValueType(), eLDFType::LDF_TYPE_UTF_8);
	ASSERT_EQ(data->GetKey(), u"KEY");
	ASSERT_EQ(((LDFData<std::string>*)data.get())->GetValue(), "==");
	ASSERT_EQ(data->GetString(), testWord);
}


TEST_F(LDFTests, LDFParseEdgeCaseTest) {
	std::vector<std::string> tests = {
		// Test undefined data
		"", // Empty
		"=", // Only equals sign
		":", // Only colon
		"=:", // Only colon and equals sign

		// Test no LDFType
		"KEY=:", // No LDF Type
		"KEY=:44", // No LDF Type, but has value
	
		// Test invalid values, but valid types
		"key=1:", // no value for int32 
		"key=1:banana", // invalid value for int32 
		"key=3:", // No value for float
		"key=3:banana", // invalid for float
		"key=4:", // No value for double
		"key=4:banana", // invalid for double
		"key=5:", // No value for U32
		"key=5:banana", // invalid for U32
		"key=7:", // No value for bool
		"key=7:banana", // invalid for bool
		"key=8:", // No value for U64
		"key=8:banana", // invalid for U64
		"key=9:", // No value for LWOOBJID
		"key=9:banana", // invalid for LWOOBJID

		// Test invalid LDF types
		"key=14:value", // invalid LDF type
		"key=-1:value", // invalid LDF type
		"key=Garbage:value", // invalid LDF type
	};
	for (auto testString : tests) {
		Game::logger->Log("LDFTests", "Testing LDF Parsing of invalid string (%s)", testString.c_str());
		EXPECT_NO_THROW(LDFBaseData::DataFromString(testString));
	}
}
