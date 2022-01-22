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

#ifndef ZTD_TEXT_IS_FULL_RANGE_REPRESENTABLE_HPP
#define ZTD_TEXT_IS_FULL_RANGE_REPRESENTABLE_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/is_transcoding_compatible.hpp>


#include <ztd/text/type_traits.hpp>

#include <type_traits>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	namespace __txt_detail {
		template <typename _Type>
		using __detect_is_encode_injective = decltype(_Type::is_encode_injective::value);

		template <typename, typename = void>
		struct __is_encode_injective_sfinae : ::std::false_type { };

		template <typename _Type>
		struct __is_encode_injective_sfinae<_Type,
			::std::enable_if_t<is_detected_v<__detect_is_encode_injective, _Type>>>
		: ::std::integral_constant<bool, _Type::is_encode_injective::value> { };

		template <typename _Type>
		using __detect_is_decode_injective = decltype(_Type::is_decode_injective::value);

		template <typename, typename = void>
		struct __is_decode_injective_sfinae : ::std::false_type { };

		template <typename _Type>
		struct __is_decode_injective_sfinae<_Type,
			::std::enable_if_t<is_detected_v<__detect_is_decode_injective, _Type>>>
		: ::std::integral_constant<bool, _Type::is_decode_injective::value> { };
	} // namespace __txt_detail

	//////
	/// @addtogroup ztd_text_properties Property and Trait Helpers
	///
	/// @{
	/////

	//////
	/// @brief Checks whether or not the decoding step for `_Type` is injective (cannot possibly lose information
	/// regardless of whatever valid input is put in).
	///
	/// @tparam _Type The encoding type to check.
	///
	/// @remarks If the encoding object does not define is_decode_injective, it is assumed to be false (the safest
	/// default).
	//////
	template <typename _Type>
	class is_decode_injective : public __txt_detail::__is_decode_injective_sfinae<_Type> { };

	//////
	/// @brief A @c \::value alias for ztd::text::is_decode_injective.
	template <typename _Type>
	inline constexpr bool is_decode_injective_v = is_decode_injective<_Type>::value;

	//////
	/// @brief Checks whether or not the encoding step for `_Type` is injective (cannot possibly lose information
	/// regardless of whatever valid input is put in).
	///
	/// @tparam _Type The encoding type to check.
	///
	/// @remarks If the encoding object does not define is_encode_injective, it is assumed to be false (the safest
	/// default).
	//////
	template <typename _Type>
	class is_encode_injective : public __txt_detail::__is_encode_injective_sfinae<_Type> { };

	//////
	/// @brief A @c \::value alias for ztd::text::is_encode_injective.
	template <typename _Type>
	inline constexpr bool is_encode_injective_v = is_encode_injective<_Type>::value;

	//////
	/// @brief Checks whether a decode operation with `_From` piped to a decode operation with `_To` is
	template <typename _From, typename _To>
	class is_full_range_representable_between
	: public ::std::integral_constant<bool,
		  is_decode_injective_v<remove_cvref_t<_From>> && is_encode_injective_v<remove_cvref_t<_To>>> { };

	//////
	/// @brief A @c \::value alias for ztd::text::is_full_range_representable_between.
	template <typename _From, typename _To>
	inline constexpr bool is_full_range_representable_bewtween_v
		= is_full_range_representable_between<_From, _To>::value;

	//////
	/// @brief An alias for ztd::text::is_full_range_representable_between<_Type, _Type>.
	template <typename _Type>
	using is_full_range_representable = is_full_range_representable_between<_Type, _Type>;

	//////
	/// @brief A @c \::value alias for ztd::text::is_full_range_representable_between<_Type, _Type>.
	template <typename _Type>
	inline constexpr bool is_full_range_representable_v = is_full_range_representable<_Type>::value;

	//////
	/// @}
	/////

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_IS_FULL_RANGE_REPRESENTABLE_HPP
