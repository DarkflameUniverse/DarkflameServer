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

#ifndef ZTD_TEXT_ASCII_HPP
#define ZTD_TEXT_ASCII_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/code_point.hpp>
#include <ztd/text/code_unit.hpp>
#include <ztd/text/unicode_code_point.hpp>
#include <ztd/text/encode_result.hpp>
#include <ztd/text/decode_result.hpp>
#include <ztd/text/error_handler.hpp>
#include <ztd/text/is_ignorable_error_handler.hpp>
#include <ztd/text/detail/replacement_units.hpp>
#include <ztd/text/detail/empty_state.hpp>

#include <ztd/ranges/adl.hpp>
#include <ztd/ranges/range.hpp>
#include <ztd/idk/span.hpp>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	//////
	/// @addtogroup ztd_text_encodings Encodings
	/// @{
	//////

	//////
	/// @brief The American Standard Code for Information Exchange (ASCII) Encoding.
	///
	/// @tparam _CodeUnit The code unit type to work over.
	///
	/// @remarks The most vanilla and unimaginative encoding there is in the world, excluding tons of other languages,
	/// dialects, and even common English idioms and borrowed words. Please don't pick this unless you have good
	/// reason!
	//////
	template <typename _CodeUnit, typename _CodePoint = unicode_code_point>
	class basic_ascii {
	public:
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
		/// @brief The state that can be used between calls to the encoder and decoder.
		///
		/// @remarks It is an empty struct because there is no shift state to preserve between complete units of
		/// encoded information. It is also only `state` and not separately `decode_state` and `encode_state`
		/// because one type suffices for both.
		//////
		using state = __txt_detail::__empty_state;
		//////
		/// @brief Whether or not the decode operation can process all forms of input into code point values.
		///
		/// @remarks ASCII can decode from its 7-bit (unpacked) code units to Unicode Code Points. Since the converion
		/// is lossless, this property is true.
		//////
		using is_decode_injective = ::std::true_type;
		//////
		/// @brief Whether or not the encode operation can process all forms of input into code unit values. This is
		/// not true for ASCII, as many Unicode Code Point and Unicode Scalar Values cannot be represented in ASCII.
		/// Since the conversion is lossy, this property is false.
		//////
		using is_encode_injective = ::std::false_type;
		//////
		/// @brief The maximum code units a single complete operation of encoding can produce.
		inline static constexpr const ::std::size_t max_code_units = 1;
		//////
		/// @brief The maximum number of code points a single complete operation of decoding can produce. This is
		/// 1 for all Unicode Transformation Format (UTF) encodings.
		//////
		inline static constexpr const ::std::size_t max_code_points = 1;
		//////
		/// @brief A range of code units representing the values to use when a replacement happen. For ASCII, this
		/// must be '?' instead of the usual Unicode Replacement Character U'�'.
		//////
		static constexpr ::ztd::span<const code_unit, 1> replacement_code_units() noexcept {
			return __txt_detail::__question_mark_replacement_units<code_unit>;
		}

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
					ascii_t __self {};
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

			code_unit __units[1] {};
			__units[0]              = *__init;
			const code_unit& __unit = __units[0];
			ranges::advance(__init);

			if constexpr (__call_error_handler) {
				if (static_cast<signed char>(__unit) < static_cast<signed char>(0)) {
					ascii_t __self {};
					return __error_handler(__self,
						_Result(ranges::reconstruct(::std::in_place_type<_UInputRange>, ::std::move(__init),
						             ::std::move(__inlast)),
						     ranges::reconstruct(::std::in_place_type<_UOutputRange>, ::std::move(__outit),
						          ::std::move(__outlast)),
						     __s, encoding_error::invalid_sequence),
						::ztd::span<code_unit, 1>(::std::addressof(__units[0]), 1), ::ztd::span<code_point, 0>());
				}
			}

			*__outit = __unit;
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
					ascii_t __self {};
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

			if constexpr (__call_error_handler) {
				if (__point > __ztd_idk_detail_last_ascii_value) {
					ascii_t __self {};
					return __error_handler(__self,
						_Result(ranges::reconstruct(::std::in_place_type<_UInputRange>, ::std::move(__init),
						             ::std::move(__inlast)),
						     ranges::reconstruct(::std::in_place_type<_UOutputRange>, ::std::move(__outit),
						          ::std::move(__outlast)),
						     __s, encoding_error::invalid_sequence),
						::ztd::span<code_point, 1>(::std::addressof(__points[0]), 1),
						::ztd::span<code_unit, 0>());
				}
			}

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
	/// @brief The American Standard Code for Information Exchange (ASCII) Encoding.
	///
	/// @remarks The most vanilla and unimaginative encoding there is in the world, excluding tons of other languages,
	/// dialects, and even common English idioms and borrowed words. Please don't pick this unless you have good
	/// reason!
	//////
	using ascii_t = basic_ascii<char>;


	//////
	/// @brief An instance of the ascii_t type for ease of use.
	inline constexpr ascii_t ascii = {};

	//////
	/// @}

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_ASCII_HPP
