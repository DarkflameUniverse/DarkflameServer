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

#ifndef ZTD_TEXT_NO_ENCODING_HPP
#define ZTD_TEXT_NO_ENCODING_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/code_point.hpp>
#include <ztd/text/code_unit.hpp>
#include <ztd/text/encode_result.hpp>
#include <ztd/text/decode_result.hpp>
#include <ztd/text/error_handler.hpp>
#include <ztd/text/forward.hpp>
#include <ztd/text/is_ignorable_error_handler.hpp>
#include <ztd/text/detail/empty_state.hpp>

#include <ztd/ranges/range.hpp>
#include <ztd/ranges/reconstruct.hpp>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	//////
	/// @addtogroup ztd_text_encodings Encodings
	/// @{
	//////

	//////
	/// @brief A UTF-32 Encoding that traffics in, specifically, the desired code unit type provided as a template
	/// argument.
	///
	/// @tparam _Type The code unit type to use.
	///
	/// @remarks This is a strict UTF-32 implementation that does not allow lone, unpaired surrogates either in or out.
	//////
	template <typename _CodeUnit, typename _CodePoint>
	class basic_no_encoding {
	public:
		//////
		/// @brief The state that can be used between calls to the encoder and decoder. It is an empty struct
		/// because there is no shift state to preserve between complete units of encoded information.
		//////
		using state = __txt_detail::__empty_state;
		//////
		/// @brief The individual units that result from an encode operation or are used as input to a decode
		/// operation.
		//////
		using code_unit = _CodeUnit;
		//////
		/// @brief The individual units that result from a decode operation or as used as input to an encode
		/// operation. For most encodings, this is going to be a Unicode Code Point or a Unicode Scalar Value.
		//////
		using code_point = _CodePoint;
		//////
		/// @brief Whether or not the decode operation can process all forms of input into code unit values. This is
		/// always true because this encoding does nothing.
		//////
		using is_decode_injective = ::std::integral_constant<bool, ::std::is_same_v<_CodeUnit, _CodePoint>>;
		//////
		/// @brief Whether or not the encode operation can process all forms of input into code unit values. This is
		/// always true because this encoding does nothing.
		//////
		using is_encode_injective = ::std::integral_constant<bool, ::std::is_same_v<_CodeUnit, _CodePoint>>;
		//////
		/// @brief The maximum number of code points a single complete operation of decoding can produce. This is 1
		/// because this encoding does nothing.
		//////
		inline static constexpr ::std::size_t max_code_points = 1;
		//////
		/// @brief The maximum code units a single complete operation of encoding can produce. This is 1 because this
		/// encoding does nothing.
		//////
		inline static constexpr ::std::size_t max_code_units = 1;

		//////
		/// @brief Decodes a single complete unit of information as code points and produces a result with the
		/// input and output ranges moved past what was successfully read and written; or, produces an error and
		/// returns the input and output ranges untouched.
		///
		/// @param[in] __input The input view to read code uunits from.
		/// @param[in] __output The output view to write code points into.
		/// @param[in] __error_handler The error handler to invoke if encoding fails.
		/// @param[in, out] __s The necessary state information. For this encoding, the state is empty and means
		/// very little.
		///
		/// @returns A ztd::text::decode_result object that contains the reconstructed input range,
		/// reconstructed output range, error handler, and a reference to the passed-in state.
		///
		/// @remarks To the best ability of the implementation, the iterators will be returned untouched (e.g.,
		/// the input models at least a view and a forward_range). If it is not possible, returned ranges may be
		/// incremented even if an error occurs due to the semantics of any view that models an input_range.
		//////
		template <typename _InputRange, typename _OutputRange, typename _ErrorHandler>
		static constexpr auto decode_one(
			_InputRange&& __input, _OutputRange&& __output, _ErrorHandler&& __error_handler, state& __s) {
			using _UInputRange   = remove_cvref_t<_InputRange>;
			using _UOutputRange  = remove_cvref_t<_OutputRange>;
			using _UErrorHandler = remove_cvref_t<_ErrorHandler>;
			using _Result        = __txt_detail::__reconstruct_decode_result_t<_InputRange, _OutputRange, state>;
			constexpr bool __call_error_handler = !is_ignorable_error_handler_v<_UErrorHandler>;

			auto __init   = ranges::ranges_adl::adl_begin(__input);
			auto __inlast = ranges::ranges_adl::adl_end(__input);
			if (__init == __inlast) {
				// an exhausted sequence is fine
				return _Result(ranges::reconstruct(
					               ::std::in_place_type<_UInputRange>, ::std::move(__init), ::std::move(__inlast)),
					ranges::reconstruct(
					     ::std::in_place_type<_UOutputRange>, ::std::forward<_OutputRange>(__output)),
					__s, encoding_error::ok);
			}

			auto __outit   = ranges::ranges_adl::adl_begin(__output);
			auto __outlast = ranges::ranges_adl::adl_end(__output);

			if constexpr (__call_error_handler) {
				if (__outit == __outlast) {
					basic_no_encoding __self {};
					return __error_handler(__self,
						_Result(ranges::reconstruct(::std::in_place_type<_UInputRange>, ::std::move(__init),
						             ::std::move(__inlast)),
						     ranges::reconstruct(::std::in_place_type<_UOutputRange>, ::std::move(__outit),
						          ::std::move(__outlast)),
						     __s, encoding_error::insufficient_output_space),
						::ztd::span<code_unit, 0>(), ::ztd::span<code_point, 0>());
				}
			}
			else {
				(void)__outlast;
			}

			code_unit __unit = *__init;
			ranges::advance(__init);

			*__outit = static_cast<code_point>(__unit);
			ranges::advance(__outit);

			return _Result(
				ranges::reconstruct(::std::in_place_type<_UInputRange>, ::std::move(__init), ::std::move(__inlast)),
				ranges::reconstruct(
				     ::std::in_place_type<_UOutputRange>, ::std::move(__outit), ::std::move(__outlast)),
				__s, encoding_error::ok);
		}

		//////
		/// @brief Encodes a single complete unit of information as code units and produces a result with the
		/// input and output ranges moved past what was successfully read and written; or, produces an error and
		/// returns the input and output ranges untouched.
		///
		/// @param[in] __input The input view to read code points from.
		/// @param[in] __output The output view to write code units into.
		/// @param[in] __error_handler The error handler to invoke if encoding fails.
		/// @param[in, out] __s The necessary state information. For this encoding, the state is empty and means
		/// very little.
		///
		/// @returns A ztd::text::encode_result object that contains the reconstructed input range,
		/// reconstructed output range, error handler, and a reference to the passed-in state.
		///
		/// @remarks To the best ability of the implementation, the iterators will be returned untouched (e.g.,
		/// the input models at least a view and a forward_range). If it is not possible, returned ranges may be
		/// incremented even if an error occurs due to the semantics of any view that models an input_range.
		//////
		template <typename _InputRange, typename _OutputRange, typename _ErrorHandler>
		static constexpr auto encode_one(
			_InputRange&& __input, _OutputRange&& __output, _ErrorHandler&& __error_handler, state& __s) {
			using _UInputRange   = remove_cvref_t<_InputRange>;
			using _UOutputRange  = remove_cvref_t<_OutputRange>;
			using _UErrorHandler = remove_cvref_t<_ErrorHandler>;
			using _Result        = __txt_detail::__reconstruct_encode_result_t<_InputRange, _OutputRange, state>;
			constexpr bool __call_error_handler = !is_ignorable_error_handler_v<_UErrorHandler>;

			auto __init   = ranges::ranges_adl::adl_begin(__input);
			auto __inlast = ranges::ranges_adl::adl_end(__input);
			if (__init == __inlast) {
				// an exhausted sequence is fine
				return _Result(ranges::reconstruct(
					               ::std::in_place_type<_UInputRange>, ::std::move(__init), ::std::move(__inlast)),
					ranges::reconstruct(
					     ::std::in_place_type<_UOutputRange>, ::std::forward<_OutputRange>(__output)),
					__s, encoding_error::ok);
			}

			auto __outit   = ranges::ranges_adl::adl_begin(__output);
			auto __outlast = ranges::ranges_adl::adl_end(__output);

			if constexpr (__call_error_handler) {
				if (__outit == __outlast) {
					basic_no_encoding __self {};
					return __error_handler(__self,
						_Result(ranges::reconstruct(::std::in_place_type<_UInputRange>, ::std::move(__init),
						             ::std::move(__inlast)),
						     ranges::reconstruct(::std::in_place_type<_UOutputRange>, ::std::move(__outit),
						          ::std::move(__outlast)),
						     __s, encoding_error::insufficient_output_space),
						::ztd::span<code_point, 0>(), ::ztd::span<code_unit, 0>());
				}
			}
			else {
				(void)__outlast;
			}

			code_point __points[1] {};
			__points[0]               = *__init;
			const code_point& __point = __points[0];
			ranges::advance(__init);

			*__outit = static_cast<code_unit>(__point);
			ranges::advance(__outit);

			return _Result(
				ranges::reconstruct(::std::in_place_type<_UInputRange>, ::std::move(__init), ::std::move(__inlast)),
				ranges::reconstruct(
				     ::std::in_place_type<_UOutputRange>, ::std::move(__outit), ::std::move(__outlast)),
				__s, encoding_error::ok);
		}
	};

	//////
	/// @brief A do-nothing encoding for `char` types of input.
	using no_encoding = basic_no_encoding<char, unicode_code_point>;

	//////
	/// @brief A do-nothing encoding for ztd::text::unicode_code_point types of input.
	using no_codepoint_encoding = basic_no_encoding<unicode_code_point, unicode_code_point>;

	//////
	/// @}

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text


#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_NO_ENCODING_HPP
