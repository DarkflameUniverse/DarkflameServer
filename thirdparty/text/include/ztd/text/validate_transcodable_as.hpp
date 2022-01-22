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

#ifndef ZTD_TEXT_VALIDATE_TRANSCODABLE_AS_HPP
#define ZTD_TEXT_VALIDATE_TRANSCODABLE_AS_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/code_unit.hpp>
#include <ztd/text/code_point.hpp>
#include <ztd/text/default_encoding.hpp>
#include <ztd/text/validate_result.hpp>
#include <ztd/text/error_handler.hpp>
#include <ztd/text/state.hpp>
#include <ztd/text/detail/is_lossless.hpp>

#include <ztd/text/detail/encoding_range.hpp>
#include <ztd/text/type_traits.hpp>
#include <ztd/text/detail/transcode_one.hpp>
#include <ztd/idk/span.hpp>

#include <algorithm>
#include <string_view>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	//////
	/// @addtogroup ztd_text_validate_transcodable_as ztd::text::validate_transcodable_as
	/// @brief These functions check if the given input of code points will decode without an error. Note that this
	/// does not mean that an error handler will be invoked that can "smooth over" any possible errors: this checks
	/// solely if it will decode from code units into code points cleanly.
	/// @{
	//////

	//////
	/// @brief Validates the code units of the `__input` according to the `__from_encoding` with the given states
	/// `__decode_state` and `__encode_state` to see if it can be turned into code points, and then code units again.
	///
	/// @param[in] __input The input range of code units to validate is possible for encoding into code points.
	/// @param[in] __from_encoding The encoding to verify can properly encode the input of code units.
	/// @param[in] __to_encoding The encoding to verify can properly encode the input of code units.
	/// @param[in] __decode_state The state to use for the decoding portion of the validation check.
	/// @param[in] __encode_state The state to use for the encoding portion of the validation check.
	///
	/// @remarks This function explicitly does not call any extension points. It defers to doing a typical loop over
	/// the code points to verify it can be decoded into code points, and then encoded back into code units, with no
	/// errors and with the exact same value sequence as the original.
	//////
	template <typename _Input, typename _FromEncoding, typename _ToEncoding, typename _DecodeState,
		typename _EncodeState>
	constexpr auto basic_validate_transcodable_as(_Input&& __input, _FromEncoding&& __from_encoding,
		_ToEncoding&& __to_encoding, _DecodeState& __decode_state, _EncodeState& __encode_state) {
		using _UInput         = remove_cvref_t<_Input>;
		using _InputValueType = ranges::range_value_type_t<_UInput>;
		using _WorkingInput   = ranges::range_reconstruct_t<::std::conditional_t<::std::is_array_v<_UInput>,
               ::std::conditional_t<is_character_v<_InputValueType>, ::std::basic_string_view<_InputValueType>,
                    ::ztd::span<const _InputValueType>>,
               _UInput>>;
		using _UFromEncoding  = remove_cvref_t<_FromEncoding>;
		using _UToEncoding    = remove_cvref_t<_ToEncoding>;
		using _Result         = validate_transcode_result<_WorkingInput, _DecodeState, _EncodeState>;

		_WorkingInput __working_input(
			ranges::reconstruct(::std::in_place_type<_WorkingInput>, ::std::forward<_Input>(__input)));

		if constexpr (is_detected_v<__txt_detail::__detect_adl_text_validate_transcodable_as_one, _WorkingInput,
			              _FromEncoding, _ToEncoding, _DecodeState, _EncodeState>) {
			(void)__encode_state;
			for (;;) {
				auto __result = text_validate_transcodable_as_one(text_tag<_UFromEncoding, _UToEncoding> {},
					__working_input, __from_encoding, __to_encoding, __decode_state);
				if (!__result.valid) {
					return _Result(::std::move(__result.input), false, __decode_state);
				}
				__working_input = ::std::move(__result.input);
				if (ranges::ranges_adl::adl_empty(__working_input)) {
					break;
				}
			}
			return _Result(ranges::reconstruct(::std::in_place_type<_WorkingInput>, ::std::move(__working_input)),
				true, __decode_state, __encode_state);
		}
		else if constexpr (is_detected_v<__txt_detail::__detect_adl_internal_text_validate_transcodable_as_one,
			                   _WorkingInput, _FromEncoding, _ToEncoding, _DecodeState, _EncodeState>) {
			(void)__encode_state;
			for (;;) {
				auto __result = __text_validate_transcodable_as_one(text_tag<_UFromEncoding, _UToEncoding> {},
					__working_input, __from_encoding, __to_encoding, __decode_state);
				if (!__result.valid) {
					return _Result(::std::move(__result.input), false, __decode_state);
				}
				__working_input = ::std::move(__result.input);
				if (ranges::ranges_adl::adl_empty(__working_input)) {
					break;
				}
			}
			return _Result(ranges::reconstruct(::std::in_place_type<_WorkingInput>, ::std::move(__working_input)),
				true, __decode_state, __encode_state);
		}
		else {
			using _CodePoint = code_point_t<_UFromEncoding>;
			using _CodeUnit  = code_unit_t<_UToEncoding>;

			_CodePoint __intermediate[max_code_points_v<_UFromEncoding>] {};

			_CodeUnit __output_storage[max_code_units_v<_UToEncoding>] {};
			::ztd::span<_CodeUnit, max_code_units_v<_UToEncoding>> __output(__output_storage);

			pass_handler_t __handler {};

			for (;;) {
				auto __transcode_result = __txt_detail::__basic_transcode_one<__txt_detail::__consume::__no>(
					::std::move(__working_input), __from_encoding, __output, __to_encoding, __handler, __handler,
					__decode_state, __encode_state, __intermediate);
				if (__transcode_result.error_code != encoding_error::ok) {
					return _Result(
						ranges::reconstruct(::std::in_place_type<_WorkingInput>, ::std::move(__working_input)),
						false, __decode_state, __encode_state);
				}
				__working_input = ::std::move(__transcode_result.input);
				if (ranges::ranges_adl::adl_empty(__working_input)) {
					break;
				}
			}
			return _Result(ranges::reconstruct(::std::in_place_type<_WorkingInput>, ::std::move(__working_input)),
				true, __decode_state, __encode_state);
		}
	}

	//////
	/// @brief Validates the code units of the `__input` according to the `__encoding` with the given states @p
	/// __decode_state and `__encode_state` to see if it can be turned into code points.
	///
	/// @param[in] __input The input range of code units to validate is possible for encoding into code points.
	/// @param[in] __from_encoding The encoding to verify can properly encode the input of code units.
	/// @param[in] __to_encoding The encoding to verify can properly encode the input of code units.
	/// @param[in] __decode_state The state to use for the decoding portion of the validation check.
	/// @param[in] __encode_state The state to use for the encoding portion of the validation check.
	///
	/// @remarks This functions checks to see if extension points for `text_validate_transcodable_as` is available
	/// taking the available 4 parameters. If so, it calls this. Otherwise, it defers to
	/// ztd::text::validate_transcodable_as.
	//////
	template <typename _Input, typename _FromEncoding, typename _ToEncoding, typename _DecodeState,
		typename _EncodeState>
	constexpr auto validate_transcodable_as(_Input&& __input, _FromEncoding&& __from_encoding,
		_ToEncoding&& __to_encoding, _DecodeState& __decode_state, _EncodeState& __encode_state) {
		using _UFromEncoding = remove_cvref_t<_ToEncoding>;
		using _UToEncoding   = remove_cvref_t<_FromEncoding>;
		if constexpr (is_detected_v<__txt_detail::__detect_adl_text_validate_transcodable_as, _Input, _FromEncoding,
			              _ToEncoding, _DecodeState, _EncodeState>) {
			(void)__encode_state;
			return text_validate_transcodable_as(text_tag<_UFromEncoding, _UToEncoding> {},
				::std::forward<_Input>(__input), ::std::forward<_FromEncoding>(__from_encoding),
				::std::forward<_ToEncoding>(__to_encoding), __decode_state, __encode_state);
		}
		else if constexpr (is_detected_v<__txt_detail::__detect_adl_text_validate_transcodable_as, _Input,
			                   _FromEncoding, _ToEncoding, _DecodeState, _EncodeState>) {
			return text_validate_transcodable_as(text_tag<_UFromEncoding, _UToEncoding> {},
				::std::forward<_Input>(__input), ::std::forward<_FromEncoding>(__from_encoding),
				::std::forward<_ToEncoding>(__to_encoding), __decode_state, __encode_state);
		}
		else if constexpr (is_detected_v<__txt_detail::__detect_adl_internal_text_validate_transcodable_as, _Input,
			                   _FromEncoding, _ToEncoding, _DecodeState, _EncodeState>) {
			(void)__encode_state;
			return __text_validate_transcodable_as(text_tag<_UFromEncoding, _UToEncoding> {},
				::std::forward<_Input>(__input), ::std::forward<_FromEncoding>(__from_encoding),
				::std::forward<_ToEncoding>(__to_encoding), __decode_state, __encode_state);
		}
		else {
			return basic_validate_transcodable_as(::std::forward<_Input>(__input),
				::std::forward<_FromEncoding>(__from_encoding), ::std::forward<_ToEncoding>(__to_encoding),
				__decode_state, __encode_state);
		}
	}

	//////
	/// @brief Validates the code units of the `__input` according to the `__from_encoding` object with the given
	/// state `__decode_state` to see if it can be turned into code units of the `__to_encoding` object.
	///
	/// @param[in] __input The input range of code units to validate is possible for encoding into code points.
	/// @param[in] __from_encoding The encoding to verify can properly encode the input of code units.
	/// @param[in] __to_encoding The encoding to verify can properly encode the input of code units.
	/// @param[in] __decode_state The state to use for the decoding portion of the validation check.
	///
	/// @remarks This functions will call ztd::text::make_encode_state with `__to_encoding` to create a default @p
	/// encode_state.
	//////
	template <typename _Input, typename _FromEncoding, typename _ToEncoding, typename _DecodeState>
	constexpr auto validate_transcodable_as(_Input&& __input, _FromEncoding&& __from_encoding,
		_ToEncoding&& __to_encoding, _DecodeState& __decode_state) {
		auto __encode_state = ztd::text::make_encode_state(__to_encoding);
		auto __result       = validate_transcodable_as(::std::forward<_Input>(__input),
               ::std::forward<_FromEncoding>(__from_encoding), ::std::forward<_ToEncoding>(__to_encoding),
               __decode_state, __encode_state);
		return __txt_detail::__slice_to_stateless(::std::move(__result));
	}

	//////
	/// @brief Validates the code units of the `__input` according to the `__from_encoding` object to see if it can
	/// be turned into code units of the `__to_encoding` object.
	///
	/// @param[in] __input The input range of code units to validate is possible for encoding into code points.
	/// @param[in] __from_encoding The encoding to verify can properly encode the input of code units.
	/// @param[in] __to_encoding The encoding to verify can properly encode the input of code units.
	///
	/// @remarks This functions will call ztd::text::make_decode_state with the `__from_encoding` object to create a
	/// default `decode_state` to use before passing it to the next overload.
	//////
	template <typename _Input, typename _FromEncoding, typename _ToEncoding>
	constexpr auto validate_transcodable_as(
		_Input&& __input, _FromEncoding&& __from_encoding, _ToEncoding&& __to_encoding) {
		auto __decode_state = ztd::text::make_decode_state(__from_encoding);
		return validate_transcodable_as(::std::forward<_Input>(__input),
			::std::forward<_FromEncoding>(__from_encoding), ::std::forward<_ToEncoding>(__to_encoding),
			__decode_state);
	}

	//////
	/// @brief Validates the code units of the `__input` according to the `__from_encoding` object to see if it can
	/// be turned into code units of the `__to_encoding` object.
	///
	/// @param[in] __input The input range of code units to validate is possible for encoding into code points.
	/// @param[in] __to_encoding The encoding to verify can properly encode the input of code units.
	///
	/// @remarks This functions will call ztd::text::make_encode_state with `__to_encoding` to create a default @p
	/// encode_state.
	//////
	template <typename _Input, typename _ToEncoding>
	constexpr auto validate_transcodable_as(_Input&& __input, _ToEncoding&& __to_encoding) {
		using _UInput   = remove_cvref_t<_Input>;
		using _CodeUnit = remove_cvref_t<ranges::range_value_type_t<_UInput>>;
#if ZTD_IS_ON(ZTD_STD_LIBRARY_IS_CONSTANT_EVALUATED_I_)
		if (::std::is_constant_evaluated()) {
			// Use literal encoding instead, if we meet the right criteria
			using _FromEncoding = default_consteval_code_unit_encoding_t<_CodeUnit>;
			_FromEncoding __from_encoding {};
			return validate_transcodable_as(
				::std::forward<_Input>(__input), __from_encoding, ::std::forward<_ToEncoding>(__to_encoding));
		}
		else
#endif
		{
			using _FromEncoding = default_code_unit_encoding_t<_CodeUnit>;
			_FromEncoding __from_encoding {};
			return validate_transcodable_as(
				::std::forward<_Input>(__input), __from_encoding, ::std::forward<_ToEncoding>(__to_encoding));
		}
	}

	//////
	/// @}

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_VALIDATE_TRANSCODABLE_AS_HPP
