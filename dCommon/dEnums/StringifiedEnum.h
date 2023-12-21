#pragma once

#ifndef __STRINGIFIEDENUM_H__
#define __STRINGIFIEDENUM_H__

#include <string>
#include "magic_enum.hpp"

namespace StringifiedEnum {
	template<typename T>
	const std::string_view ToString(const T e) {
		constexpr auto sv = &magic_enum::enum_entries<T>();
		std::string_view output {};

		const auto it = std::lower_bound(
			sv->begin(), sv->end(), e,
			[&](const std::pair<T, auto>& lhs, const T rhs)
			{ return lhs.first < rhs; }
		);

		if (it != sv->end() && it->first == e)
			output = it->second;

		else
			output = "UNKNOWN";

		return output;
	}
}

#endif // !__STRINGIFIEDENUM_H__
