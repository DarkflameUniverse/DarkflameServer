#ifndef __STRINGIFIEDENUM_H__
#define __STRINGIFIEDENUM_H__

#include <string>
#include "magic_enum.hpp"

namespace StringifiedEnum {
	template<typename T>
	constexpr std::string_view ToString(const T& e) {
		// Check type
		static_assert(std::is_enum<T>::value, "Not an enum");

		if (__builtin_constant_p(e)) {
			return magic_enum::enum_name<T>(e).empty() ? "UNKNOWN" : magic_enum::enum_name<T>(e);
		}

		// Otherwise, evaluate in real-time
		constexpr auto sv = &magic_enum::enum_entries<T>();
		std::string_view output;

		const auto it = std::lower_bound(
			sv->begin(), sv->end(), e,
			[&](const std::pair<T, std::string_view>& lhs, const T rhs) { return lhs.first < rhs; }
		);

		if (it != sv->end() && it->first == e) {
			output = it->second;
		} else {
			output = "UNKNOWN";
		}
		return output;
	}
}

#endif // !__STRINGIFIEDENUM_H__
