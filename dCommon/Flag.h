#include "GeneralUtils.h"

#include <bit>
// TODO: Test bounds checking

template <typename T>
	requires std::is_enum_v<T>
class Flag {
public:
	using type = T;
	using underlying_type = std::underlying_type_t<T>;
	static constexpr auto MAX_FLAG_VAL = std::bit_width(sizeof(underlying_type));

	/**
	 * Sets one or more flags
	 * @param flag Flag(s) to set
	*/
	template <std::same_as<T>... varArg>
	constexpr void Set(varArg... flag) noexcept {
		m_Flags |= (ConvertFlag(flag) | ...);
	}

	/**
	 * Sets ONLY have the specified flag(s), clearing all others
	 * @param flag Flag(s) to set exclusively
	*/
	template <std::same_as<T>... varArg>
	constexpr void Reset(varArg... flag) {
		m_Flags = (ConvertFlag(flag) | ...);
	}

	/**
	 * Unsets one or more flags
	 * @param flag Flag(s) to unset
	*/
	template <std::same_as<T>... varArg>
	constexpr void Unset(varArg... flag) {
		m_Flags &= ~(ConvertFlag(flag) | ...);
	}

	/**
	 * Returns true all the specified flag(s) are set
	 * @param flag Flag(s) to check
	*/
	template <std::same_as<T>... varArg>
	constexpr bool Has(varArg... flag) const {
		return (m_Flags & (ConvertFlag(flag) | ...)) == (ConvertFlag(flag) | ...);
	}

	/**
	 * Returns true ONLY the specified flag(s) are set
	 * @param flag Flag(s) to check
	*/
	template <std::same_as<T>... varArg>
	constexpr bool HasOnly(varArg... flag) const {
		return m_Flags == (ConvertFlag(flag) | ...);
	}

	/**
	 * Operator overload to allow for '=' assignment
	*/
	constexpr Flag& operator=(const T value) {
		Reset(value);
		return *this;
	}

private:
	[[nodiscard]]
	static constexpr underlying_type ConvertFlag(const T flag) {
		auto flag_val = GeneralUtils::ToUnderlying(flag);
		if (flag_val != 0) {
			return 1 << flag_val;
		}
		// This should theoeretically be possible to do at compile time, but idk how
		if (std::bit_width(flag_val) > MAX_FLAG_VAL) {
			throw std::runtime_error{ "Enum value too large to be a flag!" };
		}
		return flag_val;
	}

	underlying_type m_Flags;
};
