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

#ifndef ZTD_TEXT_DETAIL_WIDE_EXECUTION_ISO10646_HPP
#define ZTD_TEXT_DETAIL_WIDE_EXECUTION_ISO10646_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/execution.hpp>
#include <ztd/text/encode_result.hpp>
#include <ztd/text/decode_result.hpp>
#include <ztd/text/is_ignorable_error_handler.hpp>
#include <ztd/text/is_unicode_encoding.hpp>
#include <ztd/text/is_full_range_representable.hpp>
#include <ztd/text/type_traits.hpp>
#include <ztd/text/detail/progress_handler.hpp>

#include <ztd/ranges/range.hpp>
#include <ztd/idk/span.hpp>

#include <cwchar>
#include <iterator>
#include <utility>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	//////
	/// @addtogroup ztd_text_encodings Encodings
	/// @{
	//////

	namespace __txt_impl {

		//////
		/// @brief The wide encoding, as envisioned by ISO 10646. Typically UTF-32 with native endianness.
		///
		/// @remarks This is generally only turned on when the Standard Definition is turn oned ( @c
		/// __STDC_ISO_10646__ ). It effectively uses UTF-32 since that's the only encoding that can meet the original
		/// requirement of the C Standard and C Standard Library with respect to what happens with individual @c
		/// wchar_t objects.
		//////
		class __wide_execution_iso10646 : private __utf32_with<__wide_execution_iso10646, wchar_t, char32_t> {
		private:
			using __base_t = __utf32_with<__wide_execution_iso10646, wchar_t, char32_t>;

		public:
			//////
			///@brief The code point type that is decoded to, and encoded from.
			using code_point = code_point_t<__base_t>;
			//////
			///@brief The code unit type that is decoded from, and encoded to.
			using code_unit = code_unit_t<__base_t>;
			//////
			///@brief The associated state for decode operations.
			using decode_state = decode_state_t<__base_t>;
			//////
			///@brief The associated state for encode operations.
			using encode_state = encode_state_t<__base_t>;

			//////
			///@brief Whether or not this encoding is a unicode encoding or not.
			using is_unicode_encoding = ::std::integral_constant<bool, is_unicode_encoding_v<__base_t>>;
			//////
			///@brief Whether or not this encoding's `decode_one` step is injective or not.
			using is_decode_injective = ::std::false_type;
			//////
			///@brief Whether or not this encoding's `encode_one` step is injective or not.
			using is_encode_injective = ::std::false_type;

			//////
			/// @brief The maximum code units a single complete operation of encoding can produce.
			inline static constexpr const ::std::size_t max_code_units = 8;
			//////
			/// @brief The maximum number of code points a single complete operation of decoding can produce.
			inline static constexpr const ::std::size_t max_code_points = 8;

			//////
			/// @brief Decodes a single complete unit of information as code points and produces a result with the
			/// input and output ranges moved past what was successfully read and written; or, produces an error and
			/// returns the input and output ranges untouched.
			///
			/// @param[in] __input The input view to read code uunits from.
			/// @param[in] __output The output view to write code points into.
			/// @param[in] __error_handler The error handler to invoke if encoding fails.
			/// @param[in, out] __s The necessary state information. Most encodings have no state, but because this
			/// is effectively a runtime encoding and therefore it is important to preserve and manage this state.
			///
			/// @returns A ztd::text::decode_result object that contains the reconstructed input range,
			/// reconstructed output range, error handler, and a reference to the passed-in state.
			template <typename _InputRange, typename _OutputRange, typename _ErrorHandler>
			static constexpr auto decode_one(_InputRange&& __input, _OutputRange&& __output,
				_ErrorHandler&& __error_handler, decode_state& __s) {
				// just go straight from UTF32
				__base_t __base_encoding {};
				return __base_encoding.decode_one(::std::forward<_InputRange>(__input),
					::std::forward<_OutputRange>(__output), ::std::forward<_ErrorHandler>(__error_handler), __s);
			}

			//////
			/// @brief Encodes a single complete unit of information as code units and produces a result with the
			/// input and output ranges moved past what was successfully read and written; or, produces an error and
			/// returns the input and output ranges untouched.
			///
			/// @param[in] __input The input view to read code uunits from.
			/// @param[in] __output The output view to write code points into.
			/// @param[in] __error_handler The error handler to invoke if encoding fails.
			/// @param[in, out] __s The necessary state information. Most encodings have no state, but because this
			/// is effectively a runtime encoding and therefore it is important to preserve and manage this state.
			///
			/// @returns A ztd::text::encode_result object that contains the reconstructed input range,
			/// reconstructed output range, error handler, and a reference to the passed-in state.
			template <typename _InputRange, typename _OutputRange, typename _ErrorHandler>
			static constexpr auto encode_one(_InputRange&& __input, _OutputRange&& __output,
				_ErrorHandler&& __error_handler, encode_state& __s) {
				// just go straight from UTF32
				__base_t __base_encoding {};
				return __base_encoding.encode_one(::std::forward<_InputRange>(__input),
					::std::forward<_OutputRange>(__output), ::std::forward<_ErrorHandler>(__error_handler), __s);
			}
		};

	} // namespace __txt_impl

	//////
	/// @}

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_DETAIL_WIDE_EXECUTION_ISO10646_HPP
