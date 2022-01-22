// =============================================================================
//
// ztd.text
// Copyright © 2021 JeanHeyd "ThePhD" Meneide and Shepherd's Oasis, LLC
// Contact: opensource@soasis.org
//
// Commercial License Usage
// Licensees holding valid commercial ztd.text licenses may use this file in
// accordance with the commercial license agreement provided with the
// Software or, alternatively, in accordance with the terms contained in
// a written agreement between you and Shepherd's Oasis, LLC.
// For licensing terms and conditions see your agreement. For
// further information contact opensource@soasis.org.
//
// Apache License Version 2 Usage
// Alternatively, this file may be used under the terms of Apache License
// Version 2.0 (the "License") for non-commercial use; you may not use this
// file except in compliance with the License. You may obtain a copy of the
// License at
//
//		http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// ============================================================================>

#pragma once

#ifndef ZTD_TEXT_IS_UNICODE_ENCODING_HPP
#define ZTD_TEXT_IS_UNICODE_ENCODING_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/type_traits.hpp>

#include <type_traits>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	namespace __txt_detail {
		template <typename _Type>
		using __detect_contains_unicode_encoding
			= decltype(::std::declval<const _Type&>().contains_unicode_encoding());

		template <typename _Type>
		using __detect_is_unicode_encoding = decltype(_Type::is_unicode_encoding::value);

		template <typename, typename = void>
		struct __is_unicode_encoding_sfinae : ::std::false_type { };

		template <typename _Type>
		struct __is_unicode_encoding_sfinae<_Type,
			::std::enable_if_t<is_detected_v<__detect_is_unicode_encoding, _Type>>>
		: ::std::integral_constant<bool, _Type::is_unicode_encoding::value> { };
	} // namespace __txt_detail

	//////
	/// @addtogroup ztd_text_properties Property and Trait Helpers
	///
	/// @{
	/////

	//////
	/// @brief Checks whether or not the encoding has declared it can handle all of Unicode.
	///
	/// @tparam _Type The encoding type to check.
	///
	/// @remarks If the encoding object does not define is_unicode_encoding, it is assumed to be false (the safest
	/// default).
	//////
	template <typename _Type>
	class is_unicode_encoding : public __txt_detail::__is_unicode_encoding_sfinae<_Type> { };

	//////
	/// @brief A @c \::value alias for ztd::text::is_unicode_encoding.
	template <typename _Type>
	inline constexpr bool is_unicode_encoding_v = is_unicode_encoding<_Type>::value;

	//////
	/// @brief Whether or not the provided encoding is a Unicode encoding.
	///
	/// @param[in] __encoding The encoding to query.
	///
	/// @remarks This function first checks if there is a function called `contains_unicode_encoding` . If it is
	/// present, then it returns the value of that function directly. Otherwise, it checks if
	/// ztd::text::is_unicode_encoding_v is true for the provided `__encoding` . If that's the case, then `true` is
	/// returned. Otherwise, it assumes the encoding is not a Unicode-compatible encoding and returns false.
	//////
	template <typename _Encoding>
	constexpr bool contains_unicode_encoding(const _Encoding& __encoding) noexcept {
		if constexpr (is_detected_v<__txt_detail::__detect_contains_unicode_encoding, _Encoding>) {
			return __encoding.contains_unicode_encoding();
		}
		else if constexpr (is_unicode_encoding_v<_Encoding>) {
			return true;
		}
		else {
			return false;
		}
	}

	//////
	/// @}
	/////

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_IS_UNICODE_ENCODING_HPP
