#include "GeneralUtils.h"

/**
 * A bitset flag class capable of compile-time bounds-checking.
 * It should be possible to unify its methods with C++23, but until then
 * use the compile-time overloads preferentially.
*/
template <typename T>
	requires std::is_enum_v<T>
class Flag {
public:
	// Type aliases
	using type = T;
	using underlying_type = std::underlying_type_t<T>;

	// Static constants
	static constexpr auto MAX_FLAG_VAL = sizeof(underlying_type) * CHAR_BIT;

	// Constructors
	Flag() = default;

	// Run-time constructor
	constexpr Flag(const T value) : m_Flags{ ConvertFlag(value) } {}

	/**
	 * RUNTIME: Sets one or more flags
	 * @param flag Flag(s) to set
	*/
	template <std::same_as<T>... varArg>
	constexpr void Set(const varArg... flag) noexcept {
		m_Flags |= (ConvertFlag(flag) | ...);
	}

	/**
	 * COMPILETIME: Sets one or more flags
	 * @param flag Flag(s) to set
	*/
	template <T... flag>
	constexpr void Set() noexcept {
		m_Flags |= (ConvertFlag<flag>() | ...);
	}

	/**
	 * RUNTIME: Sets ONLY have the specified flag(s), clearing all others
	 * @param flag Flag(s) to set exclusively
	*/
	template <std::same_as<T>... varArg>
	constexpr void Reset(const varArg... flag) {
		m_Flags = (ConvertFlag(flag) | ...);
	}

	/**
	 * COMPILETIME: Sets ONLY have the specified flag(s), clearing all others
	 * @param flag Flag(s) to set exclusively
	*/
	template <T... flag>
	constexpr void Reset() noexcept {
		m_Flags = (ConvertFlag<flag>() | ...);
	}

	/**
	 * RUNTIME: Unsets one or more flags
	 * @param flag Flag(s) to unset
	*/
	template <std::same_as<T>... varArg>
	constexpr void Unset(const varArg... flag) {
		m_Flags &= ~(ConvertFlag(flag) | ...);
	}

	/**
	 * COMPILETIME: Unsets one or more flags
	 * @param flag Flag(s) to unset
	*/
	template <T... flag>
	constexpr void Unset() noexcept {
		m_Flags &= ~(ConvertFlag<flag>() | ...);
	}

	/**
	 * RUNTIME: Returns true all the specified flag(s) are set
	 * @param flag Flag(s) to check
	*/
	template <std::same_as<T>... varArg>
	[[nodiscard]]
	constexpr bool Has(const varArg... flag) const {
		return (m_Flags & (ConvertFlag(flag) | ...)) == (ConvertFlag(flag) | ...);
	}

	/**
	 * COMPILETIME: Returns true if all the specified flag(s) are set
	 * @param flag Flag(s) to check
	*/
	template <T... flag>
	[[nodiscard]]
	constexpr bool Has() const noexcept {
		return (m_Flags & (ConvertFlag<flag>() | ...)) == (ConvertFlag<flag>() | ...);
	}

	/**
	 * RUNTIME: Returns true if ONLY the specified flag(s) are set
	 * @param flag Flag(s) to check
	*/
	template <std::same_as<T>... varArg>
	[[nodiscard]]
	constexpr bool HasOnly(const varArg... flag) const {
		return m_Flags == (ConvertFlag(flag) | ...);
	}

	/**
	 * COMPILETIME: Returns true if ONLY the specified flag(s) are set
	 * @param flag Flag(s) to check
	*/
	template <T... flag>
	[[nodiscard]]
	constexpr bool HasOnly() const noexcept {
		return m_Flags == (ConvertFlag<flag>() | ...);
	}

	/**
	 * @return the raw value of the flags set
	*/
	[[nodiscard]]
	constexpr T Value() const noexcept {
		return static_cast<T>(m_Flags);
	}

	/**
	 * Operator overload to allow for '=' assignment
	*/
	constexpr Flag& operator=(const T value) {
		m_Flags = ConvertFlag(value);
		return *this;
	}

private:
	template <T flag>
	[[nodiscard]]
	static consteval underlying_type ConvertFlag() noexcept {
		constexpr auto flag_val = GeneralUtils::ToUnderlying(flag);
		static_assert(flag_val <= MAX_FLAG_VAL, "Flag value is too large to set!");

		return flag_val != 0 ? 1 << flag_val : flag_val;
	}

	[[nodiscard]]
	static constexpr underlying_type ConvertFlag(const T flag) {
		auto flag_val = GeneralUtils::ToUnderlying(flag);

		// This is less-efficient than the compile-time check, but still works
		// We can probably unify this and the above functions with C++23 and 'if consteval'
		if (flag_val > MAX_FLAG_VAL) {
			throw std::runtime_error{ "Flag value is too large to set!" };
		}

		return flag_val != 0 ? 1 << flag_val : flag_val;
	}

	underlying_type m_Flags;
};
