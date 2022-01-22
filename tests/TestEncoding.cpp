#include "GeneralUtils.h"
#include "CommonCxxTests.h"

/**
 * @brief Test converting a string through ztd::text
 * 
 * @param argc Number of command line arguments for this test
 * @param argv Command line arguments
 * @return 0 on success, non-zero on failure
 */
int TestEncoding(int argc, char* *const argv) {
	std::string testString8 = "Café";
    std::u16string testString16 = u"Café";

    // Check that the UTF-8 string is as expected
    ASSERT_EQ(testString8, "Café");

    // Check that the UTF-16 string is as expected
    ASSERT_EQ(testString16, u"Café");

    // Check that the UTF-8 string can be converted to UTF-16
    ASSERT_EQ(testString16, GeneralUtils::UTF8ToUTF16(testString8));

    // Check that the UTF-16 string can be converted to UTF-8
    ASSERT_EQ(testString8, GeneralUtils::UTF16ToUTF8(testString16));

	return 0;
}
