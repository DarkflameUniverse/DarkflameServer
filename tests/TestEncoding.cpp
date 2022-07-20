#include <stdexcept>
#include <string>

#include "GeneralUtils.h"
#include "CommonCxxTests.h"

void Test(std::string_view& text_under_test, char32_t[] list) {

}

int TestEncoding(int argc, char* *const argv) {
    std::string x = "Hello World!";
    std::string_view v(x);

    uint32_t out;
    GeneralUtils::_NextUTF8Char(v, out); ASSERT_EQ(out, 'H');
    GeneralUtils::_NextUTF8Char(v, out); ASSERT_EQ(out, 'e');
    GeneralUtils::_NextUTF8Char(v, out); ASSERT_EQ(out, 'l');
    GeneralUtils::_NextUTF8Char(v, out); ASSERT_EQ(out, 'l');
    GeneralUtils::_NextUTF8Char(v, out); ASSERT_EQ(out, 'o');
    ASSERT_EQ(GeneralUtils::_NextUTF8Char(v, out), true);

    x = u8"Frühling";
    v = x;
    GeneralUtils::_NextUTF8Char(v, out); ASSERT_EQ(out, U'F');
    GeneralUtils::_NextUTF8Char(v, out); ASSERT_EQ(out, U'r');
    GeneralUtils::_NextUTF8Char(v, out); ASSERT_EQ(out, U'ü');
    GeneralUtils::_NextUTF8Char(v, out); ASSERT_EQ(out, U'h');
    GeneralUtils::_NextUTF8Char(v, out); ASSERT_EQ(out, U'l');
    GeneralUtils::_NextUTF8Char(v, out); ASSERT_EQ(out, U'i');
    GeneralUtils::_NextUTF8Char(v, out); ASSERT_EQ(out, U'n');
    GeneralUtils::_NextUTF8Char(v, out); ASSERT_EQ(out, U'g');
    ASSERT_EQ(GeneralUtils::_NextUTF8Char(v, out), false);

    x = "中文字";
    v = x;
    GeneralUtils::_NextUTF8Char(v, out); ASSERT_EQ(out, U'中');
    GeneralUtils::_NextUTF8Char(v, out); ASSERT_EQ(out, U'文');
    GeneralUtils::_NextUTF8Char(v, out); ASSERT_EQ(out, U'字');
    ASSERT_EQ(GeneralUtils::_NextUTF8Char(v, out), false);

    x = "👨‍⚖️";
    v = x;
    GeneralUtils::_NextUTF8Char(v, out); ASSERT_EQ(out, 0x1F468);
    GeneralUtils::_NextUTF8Char(v, out); ASSERT_EQ(out, 0x200D);
    GeneralUtils::_NextUTF8Char(v, out); ASSERT_EQ(out, 0x2696);
    GeneralUtils::_NextUTF8Char(v, out); ASSERT_EQ(out, 0xFE0F);
    ASSERT_EQ(GeneralUtils::_NextUTF8Char(v, out), false);

    return 0;
}
