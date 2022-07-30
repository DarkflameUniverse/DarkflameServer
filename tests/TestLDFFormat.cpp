#include "LDFFormat.h"
#include "CommonCxxTests.h"

/**
 * @brief Test parsing an LDF value
 *
 * @param argc Number of command line arguments for this test
 * @param argv Command line arguments
 * @return 0 on success, non-zero on failure
 */
int TestLDFFormat(int argc, char** const argv) {
	// Create
	auto* data = LDFBaseData::DataFromString("KEY=0:VALUE");

	// Check that the data type is correct
	ASSERT_EQ(data->GetValueType(), eLDFType::LDF_TYPE_UTF_16);

	// Check that the key is correct
	ASSERT_EQ(data->GetKey(), u"KEY");

	// Check that the value is correct
	ASSERT_EQ(((LDFData<std::u16string>*)data)->GetValue(), u"VALUE");

	// Check that the serialization is correct
	ASSERT_EQ(data->GetString(), "KEY=0:VALUE");

	// Cleanup the object
	delete data;

	return 0;
}
