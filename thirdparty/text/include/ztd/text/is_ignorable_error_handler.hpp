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

#ifndef ZTD_TEXT_IS_IGNORABLE_ERROR_HANDLER_HPP
#define ZTD_TEXT_IS_IGNORABLE_ERROR_HANDLER_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/forward.hpp>
#include <ztd/text/pass_handler.hpp>
#include <ztd/text/throw_handler.hpp>
#include <ztd/text/assume_valid_handler.hpp>
#include <ztd/text/is_code_units_replaceable.hpp>
#include <ztd/text/is_code_points_replaceable.hpp>
#include <ztd/text/is_unicode_code_point.hpp>

#include <ztd/text/type_traits.hpp>

#include <type_traits>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	namespace __txt_detail {
		template <typename _Type>
		using __detect_decode_always_ok = decltype(_Type::decode_always_ok);

		template <typename _Type>
		using __detect_encode_always_ok = decltype(_Type::encode_always_ok);

		template <typename, typename = void>
		struct __is_ignorable_error_handler_sfinae : public ::std::false_type { };

		template <typename _Type>
		struct __is_ignorable_error_handler_sfinae<_Type, ::std::void_t<decltype(_Type::assume_valid)>>
		: public ::std::integral_constant<bool, _Type::assume_valid::value> { };

		template <typename _Encoding, typename _ErrorHandler>
		struct __decode_error_handler_always_returns_ok
		: public ::std::integral_constant<bool, is_detected_v<__detect_decode_always_ok, _ErrorHandler>> { };

		template <typename _Encoding>
		struct __decode_error_handler_always_returns_ok<_Encoding, replacement_handler_t>
		: public ::std::integral_constant<bool,
			  (is_code_points_replaceable_v<_Encoding> && !is_code_points_maybe_replaceable_v<_Encoding>)
			       || is_unicode_code_point_v<code_point_t<_Encoding>>> { };

		template <typename _Encoding>
		struct __decode_error_handler_always_returns_ok<_Encoding, throw_handler_t> : public ::std::true_type { };

		template <typename _Encoding, typename _ErrorHandler>
		struct __decode_error_handler_always_returns_ok<_Encoding, incomplete_handler<_Encoding, _ErrorHandler>>
		: public __decode_error_handler_always_returns_ok<_Encoding,
			  typename incomplete_handler<_Encoding, _ErrorHandler>::error_handler> { };

		template <typename _Encoding>
		struct __decode_error_handler_always_returns_ok<_Encoding, default_handler_t>
#if ZTD_IS_ON(ZTD_TEXT_DEFAULT_HANDLER_THROWS_I_)
		: public __decode_error_handler_always_returns_ok<_Encoding, throw_handler_t> {
		};
#else
		: public __decode_error_handler_always_returns_ok<_Encoding, replacement_handler_t> {
		};
#endif
		template <typename _Encoding>
		struct __decode_error_handler_always_returns_ok<_Encoding, assume_valid_handler_t> : public ::std::true_type {
		};

		template <typename _Encoding, typename _ErrorHandler>
		struct __encode_error_handler_always_returns_ok
		: public ::std::integral_constant<bool, is_detected_v<__detect_encode_always_ok, _ErrorHandler>> { };

		template <typename _Encoding>
		struct __encode_error_handler_always_returns_ok<_Encoding, replacement_handler_t>
		: public ::std::integral_constant<bool,
			  (is_code_units_replaceable_v<_Encoding> && !is_code_units_maybe_replaceable_v<_Encoding>)
			       || is_unicode_code_point_v<code_unit_t<_Encoding>>> { };

		template <typename _Encoding>
		struct __encode_error_handler_always_returns_ok<_Encoding, throw_handler_t> : public ::std::true_type { };

		template <typename _Encoding, typename _ErrorHandler>
		struct __encode_error_handler_always_returns_ok<_Encoding, incomplete_handler<_Encoding, _ErrorHandler>>
		: public __encode_error_handler_always_returns_ok<_Encoding,
			  typename incomplete_handler<_Encoding, _ErrorHandler>::error_handler> { };

		template <typename _Encoding>
		struct __encode_error_handler_always_returns_ok<_Encoding, default_handler_t>
#if ZTD_IS_ON(ZTD_TEXT_DEFAULT_HANDLER_THROWS_I_)
		: public __encode_error_handler_always_returns_ok<_Encoding, throw_handler_t> {
		};
#else
		: public __encode_error_handler_always_returns_ok<_Encoding, replacement_handler_t> {
		};

#endif
		template <typename _Encoding>
		struct __encode_error_handler_always_returns_ok<_Encoding, assume_valid_handler_t> : ::std::true_type { };

	} // namespace __txt_detail

	//////
	/// @addtogroup ztd_text_properties Property and Trait Helpers
	///
	/// @{
	/////

	//////
	/// @brief Whether or not the given `_Type` is an error handler that can be ignored.
	///
	/// @tparam _Type the Error Handling type to chec.
	///
	/// @remarks An error handler type can mark itself as ignorable by using a `using` `assume_valid` @c =
	/// `std::integral_constant`<bool, `value`> where `value` determines if the type's error handling callback can
	/// be ignored. This is what ztd::text::assume_valid does. Being configurable means templated error handlers can
	/// select whether or not they should be ignorable based on compile time, safe conditions that you can make up
	/// (including checking Macros or other environment data as a means of determining whether or not validity should
	/// be ignored.) If this results in a type derived from `std::true_type` and the encoder object using it
	/// encounters an error, then it is Undefined Behavior what occurs afterwards.
	//////
	template <typename _Type>
	class is_ignorable_error_handler : public __txt_detail::__is_ignorable_error_handler_sfinae<_Type> { };

	//////
	/// @brief A @c \::value alias for ztd::text::is_ignorable_error_handler.
	template <typename _Type>
	inline constexpr bool is_ignorable_error_handler_v = is_ignorable_error_handler<_Type>::value;

	//////
	/// @brief Whether or not the given `_Encoding` and `_Input` with the provided `_ErrorHandler` will always
	/// return ztd::text::encoding_error::ok for any failure that is not related to an output being too small
	/// (ztd::text::encoding_error::insufficient_output_space).
	///
	/// @tparam _Encoding The encoding type whose `decode_one` function will be used with the error handler.
	/// @tparam _Input The input range that will be used with the `decode_one` function of the encoding.
	/// @tparam _ErrorHandler The error handler that will be called with the given `_Encoding` object and `_Input`
	/// range.
	///
	/// @remarks This is a compile time assertion. If the encoding may exhibit different behavior at runtime based on
	/// runtime conditions, then this should return false. This is meant for cases where it is provable at compile
	/// time, this should return true. For example, if the ztd::text::replacement_handler_t is used in conjunction with
	/// ztd::text::utf8_t, then this will return true as
	//////
	template <typename _Encoding, typename _ErrorHandler>
	class decode_error_handler_always_returns_ok
	: public __txt_detail::__decode_error_handler_always_returns_ok<_Encoding, _ErrorHandler> { };

	//////
	/// @brief A @c \::value alias for ztd::text::decode_error_handler_always_returns_ok_v
	template <typename _Encoding, typename _ErrorHandler>
	inline constexpr bool decode_error_handler_always_returns_ok_v
		= decode_error_handler_always_returns_ok<_Encoding, _ErrorHandler>::value;

	//////
	/// @brief Whether or not the given `_Type` is an error handler that can be ignored.
	template <typename _Encoding, typename _ErrorHandler>
	class encode_error_handler_always_returns_ok
	: public __txt_detail::__encode_error_handler_always_returns_ok<_Encoding, _ErrorHandler> { };

	//////
	/// @brief A @c \::value alias for ztd::text::decode_error_handler_always_returns_ok_v
	template <typename _Encoding, typename _ErrorHandler>
	inline constexpr bool encode_error_handler_always_returns_ok_v
		= encode_error_handler_always_returns_ok<_Encoding, _ErrorHandler>::value;

	//////
	/// @}
	/////

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_IS_IGNORABLE_ERROR_HANDLER_HPP
