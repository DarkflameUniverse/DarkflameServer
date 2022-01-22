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

#ifndef ZTD_TEXT_UTF8_HPP
#define ZTD_TEXT_UTF8_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/encode_result.hpp>
#include <ztd/text/decode_result.hpp>
#include <ztd/text/unicode_code_point.hpp>
#include <ztd/text/is_ignorable_error_handler.hpp>
#include <ztd/text/is_transcoding_compatible.hpp>
#include <ztd/text/type_traits.hpp>
#include <ztd/text/detail/empty_state.hpp>
#include <ztd/text/detail/cast.hpp>

#include <ztd/idk/charN_t.hpp>
#include <ztd/ranges/adl.hpp>
#include <ztd/ranges/range.hpp>
#include <ztd/idk/detail/unicode.hpp>

#include <array>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	namespace __txt_impl {
		//////
		/// @brief Internal text_tag for detecting a ztd::text-derved UTF-8 type.
		///
		/// @internal
		//////
		class __utf8_tag { };

		//////
		/// @brief An internal type meant to provide the bulk of the UTF-8 functionality.
		///
		/// @internal
		///
		/// @remarks Relies on CRTP.
		//////
		template <typename _Derived = void, typename _CodeUnit = uchar8_t, typename _CodePoint = unicode_code_point,
			typename _DecodeState = __txt_detail::__empty_state, typename _EncodeState = __txt_detail::__empty_state,
			bool __overlong_allowed = false, bool __surrogates_allowed = false,
			bool __use_overlong_null_only = false>
		class __utf8_with : public __utf8_tag {
		private:
			using __self_t = ::std::conditional_t<::std::is_void_v<_Derived>, __utf8_with, _Derived>;

		public:
			//////
			/// @brief Whether or not this encoding that can encode all of Unicode.
			using is_unicode_encoding = ::std::true_type;
			//////
			/// @brief The state that can be used between calls to the encoder and decoder. It is normally an empty
			/// struct because there is no shift state to preserve between complete units of encoded information.
			//////
			using decode_state = _DecodeState;
			//////
			/// @brief The state that can be used between calls to the encoder and decoder. It is normally an empty
			/// struct because there is no shift state to preserve between complete units of encoded information.
			//////
			using encode_state = _EncodeState;
			//////
			/// @brief The individual units that result from an encode operation or are used as input to a decode
			/// operation. For UTF-8 formats, this is usually char8_t, but this can change (see
			/// ztd::text::basic_utf8).
			//////
			using code_unit = _CodeUnit;
			//////
			/// @brief The individual units that result from a decode operation or as used as input to an encode
			/// operation. For most encodings, this is going to be a Unicode Code Point or a Unicode Scalar Value.
			//////
			using code_point = _CodePoint;
			//////
			/// @brief Whether or not the decode operation can process all forms of input into code point values.
			/// Thsi is true for all Unicode Transformation Formats (UTFs), which can encode and decode without a
			/// loss of information from a valid collection of code units.
			//////
			using is_decode_injective = ::std::true_type;
			//////
			/// @brief Whether or not the encode operation can process all forms of input into code unit values.
			/// This is true for all Unicode Transformation Formats (UTFs), which can encode and decode without loss
			/// of information from a valid input code point.
			//////
			using is_encode_injective = ::std::true_type;
			//////
			/// @brief The maximum number of code points a single complete operation of decoding can produce. This is
			/// 1 for all Unicode Transformation Format (UTF) encodings.
			//////
			inline static constexpr ::std::size_t max_code_points = 1;
			//////
			/// @brief The maximum code units a single complete operation of encoding can produce. If overlong
			/// sequence allowed, this is 6: otherwise, this is 4.
			//////
			inline static constexpr ::std::size_t max_code_units = __overlong_allowed ? 6 : 4;

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
			static constexpr auto encode_one(_InputRange&& __input, _OutputRange&& __output,
				_ErrorHandler&& __error_handler, encode_state& __s) {
				using _UInputRange   = remove_cvref_t<_InputRange>;
				using _UOutputRange  = remove_cvref_t<_OutputRange>;
				using _UErrorHandler = remove_cvref_t<_ErrorHandler>;
				using _Result
					= __txt_detail::__reconstruct_encode_result_t<_InputRange, _OutputRange, encode_state>;
				constexpr bool __call_error_handler = !is_ignorable_error_handler_v<_UErrorHandler>;

				auto __init   = ranges::ranges_adl::adl_begin(__input);
				auto __inlast = ranges::ranges_adl::adl_end(__input);

				if (__init == __inlast) {
					// an exhausted sequence is fine
					return _Result(ranges::reconstruct(::std::in_place_type<_UInputRange>, ::std::move(__init),
						               ::std::move(__inlast)),
						ranges::reconstruct(
						     ::std::in_place_type<_UOutputRange>, ::std::forward<_OutputRange>(__output)),
						__s, encoding_error::ok);
				}

				auto __outit   = ranges::ranges_adl::adl_begin(__output);
				auto __outlast = ranges::ranges_adl::adl_end(__output);

				code_point __points[1] {};
				__points[0]               = *__init;
				const code_point& __point = __points[0];
				ranges::advance(__init);

				if constexpr (__call_error_handler) {
					if (__point > __ztd_idk_detail_last_unicode_code_point) {
						__self_t __self {};
						return __error_handler(__self,
							_Result(ranges::reconstruct(::std::in_place_type<_UInputRange>, ::std::move(__init),
							             ::std::move(__inlast)),
							     ranges::reconstruct(::std::in_place_type<_UOutputRange>, ::std::move(__outit),
							          ::std::move(__outlast)),
							     __s, encoding_error::invalid_sequence),
							::ztd::span<code_point, 1>(::std::addressof(__points[0]), 1),
							::ztd::span<code_unit, 0>());
					}
					if constexpr (!__surrogates_allowed) {
						if (__ztd_idk_detail_is_surrogate(__point)) {
							__self_t __self {};
							return __error_handler(__self,
								_Result(ranges::reconstruct(::std::in_place_type<_UInputRange>,
								             ::std::move(__init), ::std::move(__inlast)),
								     ranges::reconstruct(::std::in_place_type<_UOutputRange>,
								          ::std::move(__outit), ::std::move(__outlast)),
								     __s, encoding_error::invalid_sequence),
								::ztd::span<code_point, 1>(::std::addressof(__points[0]), 1),
								::ztd::span<code_unit, 0>());
						}
					}
				}

				if constexpr (__use_overlong_null_only) {
					if (__point == static_cast<code_point>(0)) {
						// overlong MUTF-8
						constexpr uchar8_t __payload[]         = { 0b11000000u, 0b10000000u };
						constexpr ::std::size_t __payload_size = static_cast<::std::size_t>(2);
						for (::std::size_t i = 0; i < __payload_size; ++i) {
							if constexpr (__call_error_handler) {
								if (__outit == __outlast) {
									__self_t __self {};
									return __error_handler(__self,
										_Result(ranges::reconstruct(::std::in_place_type<_UInputRange>,
										             ::std::move(__init), ::std::move(__inlast)),
										     ranges::reconstruct(::std::in_place_type<_UOutputRange>,
										          ::std::move(__outit), ::std::move(__outlast)),
										     __s, encoding_error::insufficient_output_space),
										::ztd::span<code_point, 1>(::std::addressof(__points[0]), 1),
										::ztd::span<code_unit>(__payload + i, __payload_size - i));
								}
							}
							*__outit = static_cast<code_unit>(__payload[i]);
							ranges::advance(__outit);
						}
						return _Result(ranges::reconstruct(::std::in_place_type<_UInputRange>,
							               ::std::move(__init), ::std::move(__inlast)),
							ranges::reconstruct(::std::in_place_type<_UOutputRange>, ::std::move(__outit),
							     ::std::move(__outlast)),
							__s, encoding_error::ok);
					}
				}

				if constexpr (__call_error_handler) {
					if (__outit == __outlast) {
						__self_t __self {};
						return __error_handler(__self,
							_Result(ranges::reconstruct(::std::in_place_type<_UInputRange>, ::std::move(__init),
							             ::std::move(__inlast)),
							     ranges::reconstruct(::std::in_place_type<_UOutputRange>, ::std::move(__outit),
							          ::std::move(__outlast)),
							     __s, encoding_error::insufficient_output_space),
							::ztd::span<code_point, 1>(::std::addressof(__points[0]), 1),
							::ztd::span<code_unit, 0>());
					}
				}
				constexpr uchar8_t __first_mask_continuation_values[][2] = {
					{ 0b01111111, __ztd_idk_detail_start_1byte_continuation },
					{ 0b00011111, __ztd_idk_detail_start_2byte_continuation },
					{ 0b00001111, __ztd_idk_detail_start_3byte_continuation },
					{ 0b00000111, __ztd_idk_detail_start_4byte_continuation },
					{ 0b00000011, __ztd_idk_detail_start_5byte_continuation },
					{ 0b00000001, __ztd_idk_detail_start_6byte_continuation },
				};

				::std::size_t __length = ::ztd::__idk_detail::__utf8_decode_length<__overlong_allowed>(__point);
				::std::size_t __length_index          = static_cast<::std::size_t>(__length - 1);
				const auto& __first_mask_continuation = __first_mask_continuation_values[__length_index];
				const uchar8_t& __first_mask          = __first_mask_continuation[0];
				const uchar8_t& __first_continuation  = __first_mask_continuation[1];
				::std::size_t __current_shift         = static_cast<::std::size_t>(6 * __length_index);
				uchar8_t __first
					= __first_continuation | static_cast<uchar8_t>((__point >> __current_shift) & __first_mask);

				*__outit = static_cast<code_unit>(__first);
				ranges::advance(__outit);

				constexpr ::std::size_t __values_size = 5;
				code_unit __values[__values_size] {};
				if (__length_index > 0) {
					__current_shift -= 6;
					for (::std::size_t __index = 0; __index < __length_index; ++__index) {
						__values[__index] = static_cast<code_unit>(__ztd_idk_detail_continuation_signature
							| static_cast<uchar8_t>(
							     (__point >> __current_shift) & __ztd_idk_detail_continuation_mask_value));
						__current_shift -= 6;
					}
					for (::std::size_t __index = 0; __index < __length_index; ++__index) {
						if constexpr (__call_error_handler) {
							if (__outit == __outlast) {
								__self_t __self {};
								::ztd::span<code_unit> __code_unit_progress(
									::std::addressof(__values[__index]), __length_index - __index);
								return __error_handler(__self,
									_Result(ranges::reconstruct(::std::in_place_type<_UInputRange>,
									             ::std::move(__init), ::std::move(__inlast)),
									     ranges::reconstruct(::std::in_place_type<_UOutputRange>,
									          ::std::move(__outit), ::std::move(__outlast)),
									     __s, encoding_error::insufficient_output_space),
									::ztd::span<code_point, 1>(::std::addressof(__points[0]), 1),
									__code_unit_progress);
							}
						}

						*__outit = __values[__index];
						ranges::advance(__outit);
					}
				}

				return _Result(ranges::reconstruct(
					               ::std::in_place_type<_UInputRange>, ::std::move(__init), ::std::move(__inlast)),
					ranges::reconstruct(
					     ::std::in_place_type<_UOutputRange>, ::std::move(__outit), ::std::move(__outlast)),
					__s, encoding_error::ok);
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
			static constexpr auto decode_one(_InputRange&& __input, _OutputRange&& __output,
				_ErrorHandler&& __error_handler, decode_state& __s) {
				using _UInputRange   = remove_cvref_t<_InputRange>;
				using _UOutputRange  = remove_cvref_t<_OutputRange>;
				using _UErrorHandler = remove_cvref_t<_ErrorHandler>;
				using _Result
					= __txt_detail::__reconstruct_decode_result_t<_InputRange, _OutputRange, decode_state>;
				constexpr bool __call_error_handler = !is_ignorable_error_handler_v<_UErrorHandler>;

				auto __init   = ranges::ranges_adl::adl_begin(__input);
				auto __inlast = ranges::ranges_adl::adl_end(__input);

				if (__init == __inlast) {
					// the empty sequence is an OK sequence
					return _Result(ranges::reconstruct(::std::in_place_type<_UInputRange>, ::std::move(__init),
						               ::std::move(__inlast)),
						ranges::reconstruct(
						     ::std::in_place_type<_UOutputRange>, ::std::forward<_OutputRange>(__output)),
						__s, encoding_error::ok);
				}

				auto __outit   = ranges::ranges_adl::adl_begin(__output);
				auto __outlast = ranges::ranges_adl::adl_end(__output);

				if constexpr (__call_error_handler) {
					if (__outit == __outlast) {
						__self_t __self {};
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

				::std::array<code_unit, max_code_units> __units {};
				__units[0]               = __txt_detail::static_cast_if_lossless<code_unit>(*__init);
				const code_unit& __unit0 = __units[0];
				ranges::advance(__init);
				::std::size_t __length = static_cast<::std::size_t>(
					__ztd_idk_detail_utf8_sequence_length(static_cast<uchar8_t>(__unit0)));

				if constexpr (!__overlong_allowed) {
					if constexpr (__call_error_handler) {
						if (__length > 4) {
							__self_t __self {};
							return __error_handler(__self,
								_Result(ranges::reconstruct(::std::in_place_type<_UInputRange>,
								             ::std::move(__init), ::std::move(__inlast)),
								     ranges::reconstruct(::std::in_place_type<_UOutputRange>,
								          ::std::move(__outit), ::std::move(__outlast)),
								     __s, encoding_error::invalid_sequence),
								::ztd::span<code_unit, 1>(__units.data(), __units.size()),
								::ztd::span<code_point, 0>());
						}
					}
				}

				if (__length == 1) {
					*__outit = static_cast<code_point>(__unit0);
					ranges::advance(__outit);
					return _Result(ranges::reconstruct(::std::in_place_type<_UInputRange>, ::std::move(__init),
						               ::std::move(__inlast)),
						ranges::reconstruct(
						     ::std::in_place_type<_UOutputRange>, ::std::move(__outit), ::std::move(__outlast)),
						__s);
				}

				if constexpr (__call_error_handler) {
					const bool __is_invalid_cu = __ztd_idk_detail_utf8_is_invalid(static_cast<uchar8_t>(__unit0));
					if (__is_invalid_cu || __ztd_idk_detail_is_lead_utf8(static_cast<uchar8_t>(__unit0))) {
						__self_t __self {};
						return __error_handler(__self,
							_Result(ranges::reconstruct(::std::in_place_type<_UInputRange>, ::std::move(__init),
							             ::std::move(__inlast)),
							     ranges::reconstruct(::std::in_place_type<_UOutputRange>, ::std::move(__outit),
							          ::std::move(__outlast)),
							     __s,
							     __is_invalid_cu ? encoding_error::invalid_sequence
							                     : encoding_error::invalid_sequence),
							::ztd::span<code_unit, 1>(__units.data(), 1), ::ztd::span<code_point, 0>());
					}
				}

				for (::std::size_t i = 1; i < __length; ++i) {
					if constexpr (__call_error_handler) {
						if (__init == __inlast) {
							__self_t __self {};
							return __error_handler(__self,
								_Result(ranges::reconstruct(::std::in_place_type<_UInputRange>,
								             ::std::move(__init), ::std::move(__inlast)),
								     ranges::reconstruct(::std::in_place_type<_UOutputRange>,
								          ::std::move(__outit), ::std::move(__outlast)),
								     __s, encoding_error::incomplete_sequence),
								::ztd::span<code_unit>(__units.data(), i), ::ztd::span<code_point, 0>());
						}
					}
					__units[i] = __txt_detail::static_cast_if_lossless<code_unit>(*__init);
					ranges::advance(__init);
					if (!__ztd_idk_detail_is_lead_utf8(static_cast<uchar8_t>(__units[i]))) {
						__self_t __self {};
						return __error_handler(__self,
							_Result(ranges::reconstruct(::std::in_place_type<_UInputRange>, ::std::move(__init),
							             ::std::move(__inlast)),
							     ranges::reconstruct(::std::in_place_type<_UOutputRange>, ::std::move(__outit),
							          ::std::move(__outlast)),
							     __s, encoding_error::invalid_sequence),
							::ztd::span<code_unit>(__units.data(), i + 1), ::ztd::span<code_point, 0>());
					}
				}

				code_point __decoded {};
				switch (__length) {
				case 2:
					__decoded = __ztd_idk_detail_utf8_decode(
						static_cast<uchar8_t>(__units[0]), static_cast<uchar8_t>(__units[1]));
					break;
				case 3:
					__decoded = __ztd_idk_detail_utf8_decode(static_cast<uchar8_t>(__units[0]),
						static_cast<uchar8_t>(__units[1]), static_cast<uchar8_t>(__units[2]));
					break;
				case 4:
				default:
					__decoded = __ztd_idk_detail_utf8_decode(static_cast<uchar8_t>(__units[0]),
						static_cast<uchar8_t>(__units[1]), static_cast<uchar8_t>(__units[2]),
						static_cast<uchar8_t>(__units[3]));
					break;
				}

				if constexpr (__call_error_handler) {
					if constexpr (!__overlong_allowed) {
						if (__ztd_idk_detail_utf8_is_overlong(__decoded, __length)) {
							__self_t __self {};
							return __error_handler(__self,
								_Result(ranges::reconstruct(::std::in_place_type<_UInputRange>,
								             ::std::move(__init), ::std::move(__inlast)),
								     ranges::reconstruct(::std::in_place_type<_UOutputRange>,
								          ::std::move(__outit), ::std::move(__outlast)),
								     __s, encoding_error::invalid_sequence),
								::ztd::span<code_unit>(__units.data(), __length), ::ztd::span<code_point, 0>());
						}
					}
					if constexpr (!__surrogates_allowed) {
						if (__ztd_idk_detail_is_surrogate(__decoded)) {
							__self_t __self {};
							return __error_handler(__self,
								_Result(ranges::reconstruct(::std::in_place_type<_UInputRange>,
								             ::std::move(__init), ::std::move(__inlast)),
								     ranges::reconstruct(::std::in_place_type<_UOutputRange>,
								          ::std::move(__outit), ::std::move(__outlast)),
								     __s, encoding_error::invalid_sequence),
								::ztd::span<code_unit>(__units.data(), __length), ::ztd::span<code_point, 0>());
						}
					}
					if (static_cast<char32_t>(__decoded) > __ztd_idk_detail_last_unicode_code_point) {
						__self_t __self {};
						return __error_handler(__self,
							_Result(ranges::reconstruct(::std::in_place_type<_UInputRange>, ::std::move(__init),
							             ::std::move(__inlast)),
							     ranges::reconstruct(::std::in_place_type<_UOutputRange>, ::std::move(__outit),
							          ::std::move(__outlast)),
							     __s, encoding_error::invalid_sequence),
							::ztd::span<code_unit>(__units.data(), __length), ::ztd::span<code_point, 0>());
					}
				}

				// then everything is fine
				*__outit = __decoded;
				ranges::advance(__outit);

				return _Result(ranges::reconstruct(
					               ::std::in_place_type<_UInputRange>, ::std::move(__init), ::std::move(__inlast)),
					ranges::reconstruct(
					     ::std::in_place_type<_UOutputRange>, ::std::move(__outit), ::std::move(__outlast)),
					__s);
			}
		};
	} // namespace __txt_impl


	//////
	/// @addtogroup ztd_text_encodings Encodings
	/// @brief These classes are implementations of the Encoding concept. They serve as the backbone of the library,
	/// and the point of extension for users.
	/// @{
	//////

	//////
	/// @brief A UTF-8 Encoding that traffics in, specifically, the desired code unit type provided as a template
	/// argument.
	///
	/// @tparam _CodeUnit The code unit type to use.
	/// @tparam _CodePoint The code point type to use.
	///
	/// @remarks This type as a maximum of 4 input code points and a maximum of 1 output code point. It strictly
	/// follows the Unicode Specification for allowed conversions. For overlong sequences (e.g., similar to Android or
	/// Java UTF-8 implementations) and other quirks, see ztd::text::basic_mutf8 or ztd::text::basic_wtf8 .
	//////
	template <typename _CodeUnit, typename _CodePoint = unicode_code_point>
	class basic_utf8 : public __txt_impl::__utf8_with<basic_utf8<_CodeUnit, _CodePoint>, _CodeUnit, _CodePoint> { };

	//////
	/// @brief A UTF-8 Encoding that traffics in uchar8_t. See ztd::text::basic_utf8 for more details.
	using utf8_t = basic_utf8<uchar8_t>;

	//////
	/// @brief An instance of the UTF-8 encoding for ease of use.
	inline constexpr utf8_t utf8 = {};

	//////
	/// @brief A UTF-8 Encoding that traffics in char, for compatibility purposes with older codebases. See
	/// ztd::text::basic_utf8 for more details.
	//////
	using compat_utf8_t = basic_utf8<char>;

	//////
	/// @brief An instance of the compatibility UTF-8 encoding for ease of use.
	inline constexpr compat_utf8_t compat_utf8 = {};

	//////
	/// @brief A "Wobbly Transformation Format 8" (WTF-8) Encoding that traffics in, specifically, the desired code
	/// unit type provided as a template argument.
	///
	/// @tparam _CodeUnit The code unit type to use.
	/// @tparam _CodePoint The code point type to use.
	///
	/// @remarks This type as a maximum of 4 input code points and a maximum of 1 output code point. Unpaired
	/// surrogates are allowed in this type, which may be useful for dealing with legacy storage and implementations of
	/// the Windows Filesystem (modern Windows no longer lets non-Unicode filenames through). For a strict,
	/// Unicode-compliant UTF-8 Encoding, see ztd::text::basic_utf8 .
	//////
	template <typename _CodeUnit, typename _CodePoint = unicode_code_point>
	class basic_wtf8 : public __txt_impl::__utf8_with<basic_wtf8<_CodeUnit, _CodePoint>, _CodeUnit, _CodePoint,
		                   __txt_detail::__empty_state, __txt_detail::__empty_state, false, true, false> { };

	//////
	/// @brief A "Wobbly Transformation Format 8" (WTF-8) Encoding that traffics in char8_t. See
	/// ztd::text::basic_wtf8 for more details.
	//////
	using wtf8_t = basic_wtf8<uchar8_t>;

	//////
	/// @brief An instance of the WTF-8 type for ease of use.
	inline constexpr wtf8_t wtf8 = {};

	//////
	/// @brief A Modified UTF-8 Encoding that traffics in, specifically, the desired code unit type provided as a
	/// template argument.
	///
	/// @tparam _CodeUnit The code unit type to use.
	/// @tparam _CodePoint The code point type to use.
	///
	/// @remarks This type as a maximum of 6 input code points and a maximum of 1 output code point. Null values are
	/// encoded as an overlong sequence to specifically avoid problems with C-style strings, which is useful for
	/// working with bad implementations sitting on top of POSIX or other Operating System APIs. For a strict,
	/// Unicode-compliant UTF-8 Encoding, see ztd::text::basic_utf8 .
	//////
	template <typename _CodeUnit, typename _CodePoint = unicode_code_point>
	class basic_mutf8 : public __txt_impl::__utf8_with<basic_mutf8<_CodeUnit, _CodePoint>, _CodeUnit, _CodePoint,
		                    __txt_detail::__empty_state, __txt_detail::__empty_state, true, false, true> { };

	//////
	/// @brief A Modified UTF-8 Encoding that traffics in char8_t. See ztd::text::basic_mutf8 for more details.
	using mutf8_t = basic_mutf8<uchar8_t>;


	//////
	/// @brief An instance of the MUTF-8 type for ease of use.
	inline constexpr mutf8_t mutf8 = {};


	namespace __txt_detail {

		template <typename _UTF8Unit, typename _UTF8Point, typename _WTF8Unit, typename _WTF8Point>
		struct __is_bitwise_transcoding_compatible<basic_utf8<_UTF8Unit, _UTF8Point>,
			basic_wtf8<_WTF8Unit, _WTF8Point>>
		: ::std::integral_constant<bool,
			  (sizeof(_UTF8Unit) == sizeof(_WTF8Unit)) && (alignof(_UTF8Unit) == alignof(_WTF8Unit))> { };

		template <typename _UTF8Unit, typename _UTF8Point, typename _MUTF8Unit, typename _MUTF8Point>
		struct __is_bitwise_transcoding_compatible<basic_utf8<_UTF8Unit, _UTF8Point>,
			basic_mutf8<_MUTF8Unit, _MUTF8Point>>
		: ::std::integral_constant<bool,
			  (sizeof(_UTF8Unit) == sizeof(_MUTF8Unit)) && (alignof(_UTF8Unit) == alignof(_MUTF8Unit))> { };

	} // namespace __txt_detail

	//////
	/// @}

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_UTF8_HPP
