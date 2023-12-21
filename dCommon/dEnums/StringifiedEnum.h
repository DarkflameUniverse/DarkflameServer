#pragma once

#ifndef __STRINGIFIEDENUM_H__
#define __STRINGIFIEDENUM_H__

#include "magic_enum.hpp"

namespace StringifiedEnum {
	template<typename T>
	const auto ToString(const T e) {
		constexpr auto sv = &magic_enum::enum_entries<T>();
		
		// ISSUE: This algorithm doesn't account for "gaps" in the enum
		const auto it = std::lower_bound(
			sv->begin(),
			sv->end(),
			e,
			[&](const std::pair<T, auto>& lhs, const T rhs)
			{ return lhs.first < rhs; }
		);
		return it != sv->end() ? it->second.data() : NULL;
	}
}

#endif // !__STRINGIFIEDENUM_H__
