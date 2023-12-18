// Modified from - https://mariusbancila.ro/blog/2023/08/17/how-to-convert-an-enum-to-string-in-cpp/

#pragma once

#ifndef __STRINGIFIEDENUM__H__
#define __STRINGIFIEDENUM__H__

/**
 * The #defines in the #ifndef block below create the actual enum class in the corresponding stringified enum's header file.
 * The code generation for the string lookup function 'StringifiedEnum::ToString()' is defined in the #else block further below.
 * To use this macro, in any header file of your choosing, create an enum of the form:
 * 
 * 		#include "StringifiedEnum.h"
 * 
 * 		BEGIN_STRINGIFIED_ENUM(eMyEnum, uint_32t)
 * 			STRINGIFIED_ENUM_ENTRY(MY_STUFF, 2)
 * 			STRINGIFIED_ENUM_ENTRY(MY_JUNK, 6)
 * 		END_STRINGIFIED_ENUM(eMyEnum, uint_32t)
 * 
 * Then, in StringifiedEnum.cpp, include said header file between the #define GENERATE_ENUM_STRINGS and #undef GENERATE_ENUM_STRINGS statements
 * 
 * 		#define GENERATE_ENUM_STRINGS
 * 
 *		#include "eMyEnum.h"
 *
 *		#undef GENERATE_ENUM_STRINGS
 *
 * When the program is compiled, the string corresponding to each enum id can be found with the function 'StringifiedEnum::ToString()'
 * For instance, using the examples above
 * 		
 * 		const char* enumString = StringifiedEnum::ToString(eMyEnum::MY_STUFF)
 * 
 * The call to StringifiedEnum::ToString(eMyEnum::MY_STUFF) would output the string "MY_STUFF"
*/
#ifndef GENERATE_ENUM_STRINGS
	/**
	 * Begins a stringified enum. An entry of a stringified enum may be converted to string.
	 * @param ENUM_NAME The name of the enum class to be created
	 * @param DATA_TYPE The underlying data type of the enum class (e.g. uint32_t)
	*/
	#define BEGIN_STRINGIFIED_ENUM(ENUM_NAME, DATA_TYPE)\
		enum class ENUM_NAME : DATA_TYPE {

	/**
	 * Defines an entry of a stringified enum. Note that all entry values must be explicity specified.
	 * Make SURE that the header file of the stringified enum is #included in StringifiedEnum.h
	 * @param name The enum entry's name (will be addressable as string)
	 * @param value The enum entry value to be explicitly specified
	*/
	#define STRINGIFIED_ENUM_ENTRY(name, value)\
			name = value,

	/**
	 * Ends a stringified enum. Entries must be the same as those in BEGIN_STRINGIFIED_ENUM().
	 * @param ENUM_NAME The name of the enum class to be created
	 * @param DATA_TYPE The underlying data type of the enum class (e.g. uint32_t) 
	*/
	#define END_STRINGIFIED_ENUM(ENUM_NAME, DATA_TYPE)\
		};\
		namespace StringifiedEnum {\
			const char* ToString(ENUM_NAME index);\
		}

/**
 * The #defines in the #else block below govern the actual generation of the 'StringifiedEnum::ToString()' functions for each enum.
 * The header file of each stringified enum must be #included in StringifiedEnum.cpp for the preprocessor to generate the functions correctly.
 * As the macro inputs are identical to those above, the documentation of each is not repeated below.
*/
#else // GENERATE_ENUM_STRINGS

	#define BEGIN_STRINGIFIED_ENUM(ENUM_NAME, DATA_TYPE)\
		enum class ENUM_NAME : DATA_TYPE;\
		namespace StringifiedEnum {\
			const char* ToString(ENUM_NAME index);\
		};\
		const char* StringifiedEnum::ToString(ENUM_NAME value) {\
			static std::vector<std::pair<DATA_TYPE, const char*>> sv = {

	#define STRINGIFIED_ENUM_ENTRY(name, value)\
				{value, #name},

	#define END_STRINGIFIED_ENUM(ENUM_NAME, DATA_TYPE)\
			};\
			static bool sorted = false;\
			if (!sorted) std::sort(sv.begin(), sv.end(), [&](const std::pair<DATA_TYPE, const char*>& lhs, const std::pair<DATA_TYPE, const char*>& rhs) { return lhs.first < rhs.first; });\
			sorted = true;\
			const auto it = std::lower_bound(sv.begin(), sv.end(), static_cast<DATA_TYPE>(value), [&](const std::pair<DATA_TYPE, const char*>& lhs, const DATA_TYPE rhs) { return lhs.first < rhs; });\
			return it != sv.end() ? it->second : "<none>";\
		}

#endif // GENERATE_ENUM_STRINGS

#endif  //!__STRINGIFIEDENUM__H__
