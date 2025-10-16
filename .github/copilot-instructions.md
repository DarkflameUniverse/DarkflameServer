# GitHub Copilot Instructions

 * c++20 standard, please use the latest features except NO modules.
   * use `.contains` for searching in associative containers
 * use const as much as possible.  If it can be const, it should be made const
 * DO NOT USE const_cast EVER.
 * use `cstdint` bitwidth types ALWAYS for integral types.
 * NEVER use std::wstring. If wide strings are necessary, use std::u16string with conversion utilties in GeneralUtils.h.
 * Functions are ALWAYS PascalCase.
 * local variables are camelCase
 * NEVER use snake case
 * indentation is TABS, not SPACES.
 * TABS are 4 spaces by default
 * Use trailing braces ALWAYS
 * global variables are prefixed with `g_`
 * if global variables or functions are needed, they should be located in an anonymous namespace
 * Use `GeneralUtils::TryParse` for ANY parsing of strings to integrals.
 * Use brace initialization when possible.
 * ALWAYS default initialize variables.
 * Pointers should be avoided unless necessary. Use references when the pointer has been checked and should not be null
 * headers should be as compact as possible. Do NOT include extra data that isnt needed.
 * Remember to include logs (LOG macro uses printf style logging) while putting verbose logs under LOG_DEBUG.
 * NEVER USE `RakNet::BitStream::ReadBit`
 * NEVER assume pointers are good, always check if they are null. Once a pointer is checked and is known to be non-null, further accesses no longer need checking
 * Be wary of TOCTOU. Prevent all possible issues relating to TOCTOU.
 * new memory allocations should never be used unless absolutely necessary.
    * new for reconstruction of objects is allowed
 * Prefer following the format of the file over correct formatting.  Consistency over correctness.
 * When using auto, ALWAYS put a * for pointers.