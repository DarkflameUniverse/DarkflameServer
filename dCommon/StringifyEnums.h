// Source - https://mariusbancila.ro/blog/2023/08/17/how-to-convert-an-enum-to-string-in-cpp/

#pragma once

#ifndef __STRINGIFYENUMS__H__
#define __STRINGIFYENUMS__H__

#	undef DECL_ENUM_ELEMENT
#	undef BEGIN_ENUM
#	undef END_ENUM

#	ifndef GENERATE_ENUM_STRINGS

#		define DECLARE_ENUM_ELEMENT(element)  element,
#		define BEGIN_ENUM(ENUM_NAME, TYPE)	 typedef enum class tag##ENUM_NAME : TYPE {
#		define BEGIN_ENUM_INT(ENUM_NAME)		BEGIN_ENUM(ENUM_NAME, int)
#		define END_ENUM(ENUM_NAME)				} ENUM_NAME; const char* ENUM_NAME##_as_string(enum tag##ENUM_NAME index);

#		define DECLARE_ENUM_ELEMENT_WITH_VALUE(element, value)  element = value,

#		define BEGIN_ENUM_WITH_VALUES(ENUM_NAME, TYPE)			 BEGIN_ENUM(ENUM_NAME, TYPE)
#		define BEGIN_ENUM_WITH_VALUES_INT(ENUM_NAME)				BEGIN_ENUM(ENUM_NAME, int)
#		define END_ENUM_WITH_VALUES(ENUM_NAME, TYPE)						END_ENUM(ENUM_NAME)

#	else // GENERATE_ENUM_STRINGS
#		define NO_VALUE							  "<none>"

#		define DECLARE_ENUM_ELEMENT(element)	  #element,

#		define BEGIN_ENUM(ENUM_NAME, TYPE)		 enum class tag##ENUM_NAME : TYPE;\
const char* ENUM_NAME##_as_string(enum tag##ENUM_NAME value) {\
	std::size_t index = static_cast<std::size_t>(value);\
	static const char* s_##ENUM_NAME[] = {

#		define BEGIN_ENUM_INT(ENUM_NAME)			BEGIN_ENUM(ENUM_NAME, int)

#		define END_ENUM(ENUM_NAME)					};\
	static const std::size_t s_##ENUM_NAME_len = sizeof(s_##ENUM_NAME)/sizeof(const char*);\
	if(index >=0 && index < s_##ENUM_NAME_len)\
		return s_##ENUM_NAME[index]; \
	return NO_VALUE;\
}

#		define DECLARE_ENUM_ELEMENT_WITH_VALUE(element, value) {value, #element},

#		define BEGIN_ENUM_WITH_VALUES(ENUM_NAME, TYPE)			 enum class tag##ENUM_NAME : TYPE;\
const char* ENUM_NAME##_as_string(enum tag##ENUM_NAME value) {\
	static std::vector<std::pair<TYPE, const char*>> sv = {

#		define BEGIN_ENUM_WITH_VALUES_INT(ENUM_NAME)				BEGIN_ENUM_WITH_VALUES(ENUM_NAME, int)

#		define END_ENUM_WITH_VALUES(ENUM_NAME, TYPE)						};\
	static bool sorted = false;\
	if (!sorted) std::sort(sv.begin(), sv.end(), [&](const std::pair<TYPE, const char*>& lhs, const std::pair<TYPE, const char*>& rhs) { return lhs.first < rhs.first; });\
	sorted = true;\
	const auto it = std::lower_bound(sv.begin(), sv.end(), static_cast<TYPE>(value), [&](const std::pair<TYPE, const char*>& lhs, const TYPE rhs) { return lhs.first < rhs; });\
	return it != sv.end() ? it->second : NO_VALUE;\
}

#	endif // GENERATE_ENUM_STRINGS

#endif  //!__STRINGIFYENUMS__H__
