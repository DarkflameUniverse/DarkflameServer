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

#ifndef ZTD_TEXT_VALIDATE_RESULT_HPP
#define ZTD_TEXT_VALIDATE_RESULT_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/encoding_error.hpp>

#include <ztd/idk/reference_wrapper.hpp>
#include <ztd/ranges/reconstruct.hpp>

#include <cstddef>
#include <array>
#include <utility>
#include <system_error>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	//////
	/// @addtogroup ztd_text_result Result Types
	/// @{
	/////

	//////
	/// @brief The result of valdation operations (such as ztd_text_validate_decodable_as and
	/// ztd_text_validate_encodable_as) that specifically do not include a reference to the state.
	//////
	template <typename _Input>
	class stateless_validate_result {
	public:
		//////
		/// @brief The reconstructed input_view object, with its .begin() incremented by the number of code units
		/// successfully read (can be identical to .begin() on original range on failure).
		//////
		_Input input;
		//////
		/// @brief Whether or not the specified input is valid or not.
		bool valid;

		//////
		/// @brief Constructs a ztd::text::validate_result, defaulting the error code to
		/// ztd::text::encoding_error::ok if not provided.
		///
		/// @param[in] __input The input range to store.
		/// @param[in] __is_valid Whether or not the validation succeeded.
		//////
		template <typename _ArgInput>
		constexpr stateless_validate_result(_ArgInput&& __input, bool __is_valid)
		: input(::std::forward<_ArgInput>(__input)), valid(__is_valid) {
		}

		//////
		/// @brief A conversion for use in if statements and conditional operators.
		///
		/// @return Whether or not the result is valid or not.
		//////
		constexpr explicit operator bool() const noexcept {
			return valid;
		}
	};

	//////
	/// @brief The result of validation operations (such as ztd_text_validate_decodable_as and
	/// ztd_text_validate_encodable_as).

	template <typename _Input, typename _State>
	class validate_result : public stateless_validate_result<_Input> {
	private:
		using __base_t = stateless_validate_result<_Input>;

	public:
		//////
		/// @brief A reference to the state of the associated Encoding used for validating the input.
		::ztd::reference_wrapper<_State> state;

		//////
		/// @brief Constructs a ztd::text::validate_result, defaulting the error code to
		/// ztd::text::encoding_error::ok if not provided.
		///
		/// @param[in] __input The input range to store.
		/// @param[in] __is_valid Whether or not the validation succeeded.
		/// @param[in] __state The state related to the encoding that was used to do validation.
		//////
		template <typename _ArgInput, typename _ArgState>
		constexpr validate_result(_ArgInput&& __input, bool __is_valid, _ArgState&& __state)
		: __base_t(::std::forward<_ArgInput>(__input), __is_valid), state(::std::forward<_ArgState>(__state)) {
		}
	};

	//////
	/// @brief The result of a transcoding validation operations (e.g. from ztd_text_validate_transcodable_as).
	template <typename _Input, typename _DecodeState, typename _EncodeState>
	class validate_transcode_result : public stateless_validate_result<_Input> {
	private:
		using __base_t = stateless_validate_result<_Input>;

	public:
		//////
		/// @brief A reference to the state of the associated Encoding used for validating the input.
		::ztd::reference_wrapper<_DecodeState> from_state;
		//////
		/// @brief A reference to the state of the associated Encoding used for validating the input.
		::ztd::reference_wrapper<_EncodeState> to_state;

		//////
		/// @brief Constructs a ztd::text::validate_result, defaulting the error code to
		/// ztd::text::encoding_error::ok if not provided.
		///
		/// @param[in] __input The input range to store.
		/// @param[in] __is_valid Whether or not the validation succeeded.
		/// @param[in] __from_state The state related to the encoding that was used to do validation.
		/// @param[in] __to_state The state related to the encoding that was used to do validation.
		//////
		template <typename _ArgInput, typename _ArgFromState, typename _ArgToState>
		constexpr validate_transcode_result(
			_ArgInput&& __input, bool __is_valid, _ArgFromState&& __from_state, _ArgToState&& __to_state)
		: __base_t(::std::forward<_ArgInput>(__input), __is_valid)
		, from_state(::std::forward<_ArgFromState>(__from_state))
		, to_state(::std::forward<_ArgToState>(__to_state)) {
		}
	};

	//////
	/// @}
	/////

	namespace __txt_detail {
		template <typename _Input, typename _State>
		constexpr stateless_validate_result<_Input>
		__slice_to_stateless(validate_result<_Input, _State>&& __result) noexcept(
			::std::is_nothrow_constructible_v<stateless_validate_result<_Input>, validate_result<_Input, _State>>) {
			return __result;
		}

		template <typename _Input, typename _DecodeState, typename _EncodeState>
		constexpr stateless_validate_result<_Input>
		__slice_to_stateless(validate_transcode_result<_Input, _DecodeState, _EncodeState>&& __result) noexcept(
			::std::is_nothrow_constructible_v<stateless_validate_result<_Input>,
			     validate_transcode_result<_Input, _DecodeState, _EncodeState>>) {
			return __result;
		}

		template <typename _Input, typename _DecodeState, typename _EncodeState>
		constexpr validate_result<_Input, _DecodeState>
		__drop_single_state(validate_transcode_result<_Input, _DecodeState, _EncodeState>&& __result) noexcept(
			::std::is_nothrow_constructible_v<validate_result<_Input, _DecodeState>, _Input&&, bool&,
			     _DecodeState&>) {
			return validate_result<_Input, _DecodeState>(
				::std::move(__result.input), ::std::move(__result.valid), __result.from_state);
		}

		template <typename _InputRange, typename _State>
		using __reconstruct_validate_result_t = validate_result<ranges::range_reconstruct_t<_InputRange>, _State>;
	} // namespace __txt_detail

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_VALIDATE_RESULT_HPP
