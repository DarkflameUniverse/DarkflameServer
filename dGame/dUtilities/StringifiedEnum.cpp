// Modified from - https://mariusbancila.ro/blog/2023/08/17/how-to-convert-an-enum-to-string-in-cpp/

/**
 * This #define is part of the system used in StringifiedEnum.h.
 * Defining GENERATING_ENUM_STRINGS causes the 'StringifiedEnum::ToString)' function to be generated for each stringified enum
 * This needs to be done in a .cpp file as these functions are actual code and not just header entries
*/
#define GENERATE_ENUM_STRINGS

#include "eGameMessageType.h"
#include "eWorldMessageType.h"

#undef GENERATE_ENUM_STRINGS
/**
 * The #undef above stops further 'StringifiedEnum::ToString)' function generation.
 * Only header files between the #define and #undef statements will generate
*/
