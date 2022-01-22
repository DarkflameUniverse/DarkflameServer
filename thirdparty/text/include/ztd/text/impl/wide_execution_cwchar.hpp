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

#ifndef ZTD_TEXT_DETAIL_WIDE_EXECUTION_CWCHAR_HPP
#define ZTD_TEXT_DETAIL_WIDE_EXECUTION_CWCHAR_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/execution.hpp>
#include <ztd/text/encode_result.hpp>
#include <ztd/text/decode_result.hpp>
#include <ztd/text/error_handler.hpp>
#include <ztd/text/unicode_code_point.hpp>
#include <ztd/text/is_ignorable_error_handler.hpp>
#include <ztd/text/type_traits.hpp>
#include <ztd/text/detail/empty_state.hpp>
#include <ztd/text/detail/progress_handler.hpp>

#include <ztd/ranges/range.hpp>
#include <ztd/idk/span.hpp>
#include <ztd/idk/encoding_detection.h>

#include <ztd/prologue.hpp>

#include <cwchar>
#include <iterator>
#include <utility>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	namespace __txt_detail {

		class __wide_execution_decode_state {
		public:
			::std::mbstate_t __wide_state;
			decode_state_t<execution_t> __narrow_state;

			__wide_execution_decode_state() noexcept : __wide_state(), __narrow_state() {
				char __ghost_space[MB_LEN_MAX];
#if ZTD_IS_ON(ZTD_LIBVCXX_I_)
				::std::size_t __init_result {};
				errno_t err = wcrtomb_s(&__init_result, __ghost_space, MB_LEN_MAX, L'\0', &__wide_state);
				ZTD_TEXT_ASSERT_I_(err == 0);
#else
				::std::size_t __init_result = ::std::wcrtomb(__ghost_space, L'\0', &__wide_state);
#endif
				// make sure it is initialized
				ZTD_TEXT_ASSERT(__init_result == 1 && __ghost_space[0] == '\0');
				ZTD_TEXT_ASSERT(::std::mbsinit(&__wide_state) != 0);
			}
		};

		class __wide_execution_encode_state {
		public:
			::std::mbstate_t __wide_state;
			encode_state_t<execution_t> __narrow_state;

			__wide_execution_encode_state() noexcept : __wide_state(), __narrow_state() {
				wchar_t __ghost_space[2];
				::std::size_t __init_result = ::std::mbrtowc(__ghost_space, "", 1, &__wide_state);
				// make sure it is initialized
				ZTD_TEXT_ASSERT(__init_result == 0 && __ghost_space[0] == L'\0');
				ZTD_TEXT_ASSERT(::std::mbsinit(&__wide_state) != 0);
			}
		};
	} // namespace __txt_detail

	//////
	/// @addtogroup ztd_text_encodings Encodings
	/// @{
	//////

	namespace __txt_impl {

		//////
		/// @brief The Encoding that represents the "Wide Execution" (wide locale-based) encoding. This iteration
		/// uses the C Standard Library to do its job.
		///
		/// @remarks Because this encoding uses the C Standard Library's functions, it is both slower and
		/// effectively dangerous because it requires a roundtrip through the encoding to get to
		/// UTF-32, and vice-versa. This is only used when `wchar_t` and its locale-based runtime encoding
		/// cannot be determined to be UTF-32, UTF-16, or some other statically-known encoding. These conversions
		/// may also be lossy.
		//////
		class __wide_execution_cwchar {
		public:
			//////
			/// @brief The individual units that result from an encode operation or are used as input to a
			/// decode operation.
			///
			/// @remarks Please note that wchar_t is a variably sized type across platforms and may not
			/// represent either UTF-16 or UTF-32, including on *nix or POSIX platforms.
			//////
			using code_unit = wchar_t;
			//////
			/// @brief The individual units that result from a decode operation or as used as input to an encode
			/// operation. For most encodings, this is going to be a Unicode Code Point or a Unicode Scalar
			/// Value.
			//////
			using code_point = unicode_code_point;

		private:
			using __wide_decode_state = __txt_detail::__wide_execution_decode_state;
			using __wide_encode_state = __txt_detail::__wide_execution_encode_state;

		public:
			//////
			/// @brief The state of the wide encoding used between calls, which may potentially manage
			/// shift state.
			///
			/// @remarks This type can potentially have lots of state due to the way the C API is specified.
			//////
			using decode_state = __wide_decode_state;
			//////
			/// @brief The state of the wide encoding used between calls, which may potentially manage
			/// shift state.
			///
			/// @remarks This type can potentially have lots of state due to the way the C API is specified.
			//////
			using encode_state = __wide_encode_state;
			//////
			/// @brief Whether or not the decode operation can process all forms of input into code point
			/// values.
			///
			/// @remarks All known wide encodings can decode into Unicode just fine.
			//////
			using is_decode_injective = ::std::false_type;

			//////
			/// @brief Whether or not the encode operation can process all forms of input into code unit values.
			/// On Windows, this is guaranteed to be UTF-16 encoding for the platform. Normally, this is UTF-32
			/// on *nix/POSIX machines, but it can (and has been) changed before, sometimes even at runtime.
			///
			/// @remarks IBM encodings/computers make life interesting...
			//////
			using is_encode_injective = ::std::false_type;

			//////
			/// @brief Whether or not this encoding a Unicode encoding of some type.
			///
			/// @remarks On Windows, this is always true. On other platforms, the guarantees are not quite
			/// there. IBM encodings/computers make life interesting...
			//////
			using is_unicode_encoding = ::std::false_type;

			//////
			/// @brief The maximum code units a single complete operation of encoding can produce.
			inline static constexpr const ::std::size_t max_code_units = 8;
			//////
			/// @brief The maximum number of code points a single complete operation of decoding can produce.
			inline static constexpr const ::std::size_t max_code_points = 8;

			//////
			/// @brief Returns whether or not this encoding is a unicode encoding.
			///
			/// @remarks This function operates at runtime and queries the existing locale through a variety of
			/// platform-specific means (such as `nl_langinfo` for POSIX, ACP probing on Windows, or fallin
			/// back to
			/// `std::setlocale` name checking otherwise).
			//////
			static bool contains_unicode_encoding() noexcept {
				// even if the wide encoding is unicode, we have to round-trip through the execution encoding, so if
				// this doesn't work then nothing works with all unicode code points!
				if (!ztdc_is_execution_encoding_unicode()) {
					return false;
				}
				return ztdc_is_wide_execution_encoding_unicode();
			}

			//////
			/// @brief Encodes a single complete unit of information as code units and produces a result with
			/// the input and output ranges moved past what was successfully read and written; or, produces an
			/// error and returns the input and output ranges untouched.
			///
			/// @param[in] __input The input view to read code uunits from.
			/// @param[in] __output The output view to write code points into.
			/// @param[in] __error_handler The error handler to invoke if encoding fails.
			/// @param[in, out] __s The necessary state information. Most encodings have no state, but because
			/// this is effectively a runtime encoding and therefore it is important to preserve and manage this
			/// state.
			///
			/// @returns A ztd::text::encode_result object that contains the reconstructed input range,
			/// reconstructed output range, error handler, and a reference to the passed-in state.
			///
			/// @remarks Platform APIs and/or the C Standard Library may be used to properly decode one complete
			/// unit of information (alongside std::mbstate_t usage). Whether or not the state is used is based
			/// on the implementation and what it chooses. If ZTD_TEXT_USE_CUNEICODE is defined, the
			/// ztd.cuneicode library may be used to fulfill this functionality.
			///
			/// @remarks To the best ability of the implementation, the iterators will be
			/// returned untouched (e.g., the input models at least a view and a forward_range). If it is not
			/// possible, returned ranges may be incremented even if an error occurs due to the semantics of any
			/// view that models an input_range.
			//////
			template <typename _InputRange, typename _OutputRange, typename _ErrorHandler>
			static auto encode_one(_InputRange&& __input, _OutputRange&& __output, _ErrorHandler&& __error_handler,
				encode_state& __s) {
				using _UErrorHandler = remove_cvref_t<_ErrorHandler>;
				using _Result
					= __txt_detail::__reconstruct_encode_result_t<_InputRange, _OutputRange, encode_state>;
				constexpr bool __call_error_handler = !is_ignorable_error_handler_v<_UErrorHandler>;
				using _UInputRange                  = remove_cvref_t<_InputRange>;
				using _UOutputRange                 = remove_cvref_t<_OutputRange>;

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

				constexpr const ::std::size_t __state_max = 32;
				char __intermediate_buffer[__state_max + 1] {};
				char* __intermediate_start = &__intermediate_buffer[0];
				::ztd::span<char, __state_max> __intermediate_output(__intermediate_start, __state_max);
				execution_t __exec {};
				__txt_detail::__progress_handler<::std::integral_constant<bool, !__call_error_handler>,
					__wide_execution_cwchar>
					__intermediate_handler {};
				auto __result = __exec.encode_one(::std::forward<_InputRange>(__input), __intermediate_output,
					__intermediate_handler, __s.__narrow_state);
				if constexpr (__call_error_handler) {
					if (__result.error_code != encoding_error::ok) {
						__wide_execution_cwchar __self {};
						return __error_handler(__self,
							_Result(::std::move(__result.input),
							     ranges::reconstruct(::std::in_place_type<_UOutputRange>, ::std::move(__outit),
							          ::std::move(__outlast)),
							     __s, __result.error_code),
							__intermediate_handler._M_code_points_progress(),
							__intermediate_handler._M_code_units_progress());
					}
				}

				auto __current_input = ::ztd::span<char>(__intermediate_output.data(),
					::std::distance(__intermediate_output.data(), __result.output.data()));
				code_unit __units[1] {};
				::std::size_t __res = ::std::mbrtowc(::std::addressof(__units[0]), __current_input.data(),
					__current_input.size(), ::std::addressof(__s.__wide_state));
				if (__res == static_cast<::std::size_t>(-1)) {
					// error: cry about it
					if constexpr (__call_error_handler) {
						__wide_execution_cwchar __self {};
						return __error_handler(__self,
							_Result(::std::move(__result.input),
							     ranges::reconstruct(::std::in_place_type<_UOutputRange>, ::std::move(__outit),
							          ::std::move(__outlast)),
							     __s, encoding_error::invalid_sequence),
							__intermediate_handler._M_code_points_progress(),
							__intermediate_handler._M_code_units_progress());
					}
				}
				else if (__res == static_cast<::std::size_t>(-2)) {
					// incomplete sequence
					if constexpr (__call_error_handler) {
						__wide_execution_cwchar __self {};
						return __error_handler(__self,
							_Result(::std::move(__result.input),
							     ranges::reconstruct(::std::in_place_type<_UOutputRange>, ::std::move(__outit),
							          ::std::move(__outlast)),
							     __s, encoding_error::incomplete_sequence),
							__intermediate_handler._M_code_points_progress(),
							__intermediate_handler._M_code_units_progress());
					}
				}
				*__outit = __units[0];
				ranges::advance(__outit);

				return _Result(::std::move(__result.input),
					ranges::reconstruct(
					     ::std::in_place_type<_UOutputRange>, ::std::move(__outit), ::std::move(__outlast)),
					__s, __result.error_code);
			}

			//////
			/// @brief Decodes a single complete unit of information as code points and produces a result with
			/// the input and output ranges moved past what was successfully read and written; or, produces an
			/// error and returns the input and output ranges untouched.
			///
			/// @param[in] __input The input view to read code uunits from.
			/// @param[in] __output The output view to write code points into.
			/// @param[in] __error_handler The error handler to invoke if encoding fails.
			/// @param[in, out] __s The necessary state information. Most encodings have no state, but because
			/// this is effectively a runtime encoding and therefore it is important to preserve and manage this
			/// state.
			///
			/// @returns A ztd::text::decode_result object that contains the reconstructed input range,
			/// reconstructed output range, error handler, and a reference to the passed-in state.
			///
			/// @remarks Platform APIs and/or the C Standard Library may be used to properly decode one complete
			/// unit of information (alongside std::mbstate_t usage). Whether or not the state is used is based
			/// on the implementation and what it chooses. If `ZTD_TEXT_USE_CUNEICODE` is defined, the
			/// ztd.cuneicode library may be used to fulfill this functionality.
			///
			/// @remarks To the best ability of the implementation, the iterators will be
			/// returned untouched (e.g., the input models at least a view and a forward_range). If it is not
			/// possible, returned ranges may be incremented even if an error occurs due to the semantics of any
			/// view that models an input_range.
			//////
			template <typename _InputRange, typename _OutputRange, typename _ErrorHandler>
			static auto decode_one(_InputRange&& __input, _OutputRange&& __output, _ErrorHandler&& __error_handler,
				decode_state& __s) {
				using _UErrorHandler = remove_cvref_t<_ErrorHandler>;
				using _Result
					= __txt_detail::__reconstruct_decode_result_t<_InputRange, _OutputRange, decode_state>;
				constexpr bool __call_error_handler = !is_ignorable_error_handler_v<_UErrorHandler>;
				using _UInputRange                  = remove_cvref_t<_InputRange>;
				using _UOutputRange                 = remove_cvref_t<_OutputRange>;

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

				if constexpr (__call_error_handler) {
					if (__outit == __outlast) {
						__wide_execution_cwchar __self {};
						return __error_handler(__self,
							_Result(ranges::reconstruct(::std::in_place_type<_UInputRange>, ::std::move(__init),
							             ::std::move(__inlast)),
							     ranges::reconstruct(::std::in_place_type<_UOutputRange>, ::std::move(__outit),
							          ::std::move(__outlast)),
							     __s, encoding_error::insufficient_output_space),
							::ztd::span<code_unit, 0>(), ::ztd::span<code_point, 0>());
					}
				}

				constexpr const ::std::size_t __state_max = 32;
				char __intermediate_buffer[__state_max + 1] {};
				::std::size_t __state_count = 0;
				code_unit __units[__state_max] {};
				::std::size_t __units_count = 0;
				for (; __state_count < __state_max;) {
					__units[__units_count]  = *__init;
					const code_unit& __unit = __units[__units_count];
					++__units_count;
					ranges::advance(__init);
#if ZTD_IS_ON(ZTD_LIBVCXX_I_)
					::std::size_t __res;
					errno_t __err = wcrtomb_s(::std::addressof(__res), __intermediate_buffer, __state_max, __unit,
						::std::addressof(__s.__wide_state));
					if constexpr (__call_error_handler) {
						if (__err != 0) {
							// error: cry about it
							__wide_execution_cwchar __self {};
							return __error_handler(__self,
								_Result(ranges::reconstruct(::std::in_place_type<_UInputRange>,
								             ::std::move(__init), ::std::move(__inlast)),
								     ranges::reconstruct(::std::in_place_type<_UOutputRange>,
								          ::std::move(__outit), ::std::move(__outlast)),
								     __s, encoding_error::invalid_sequence),
								::ztd::span<code_unit>(::std::addressof(__units[0]), __units_count),
								::ztd::span<code_point, 0>());
						}
					}
					else {
						(void)__err;
					}
#else
					::std::size_t __res
						= ::std::wcrtomb(__intermediate_buffer, __unit, ::std::addressof(__s.__wide_state));
#endif

					if (__res == static_cast<::std::size_t>(-1)) {
						if constexpr (__call_error_handler) {
							// error: cry about it
							__wide_execution_cwchar __self {};
							return __error_handler(__self,
								_Result(ranges::reconstruct(::std::in_place_type<_UInputRange>,
								             ::std::move(__init), ::std::move(__inlast)),
								     ranges::reconstruct(::std::in_place_type<_UOutputRange>,
								          ::std::move(__outit), ::std::move(__outlast)),
								     __s, encoding_error::invalid_sequence),
								::ztd::span<code_unit>(::std::addressof(__units[0]), __units_count),
								::ztd::span<code_point, 0>());
						}
					}
					else if (__res == 0) {
						if (::std::mbsinit(::std::addressof(__s.__wide_state)) == 0) {
							// mixed conversion potential?!
							// technically, not standard behavior, but I don't really care?
							// Mr. Steve Downey points out I'm slightly right
							// about my assumption here: C has an open DR for this
							// (DR 488, http://www.open-std.org/jtc1/sc22/wg14/www/docs/n2059.htm#dr_488)
							// Another DR, DR 499
							// (http://www.open-std.org/jtc1/sc22/wg14/www/docs/n2059.htm#dr_498) addresses
							// thread safety issues, both should be solved is this is to be anywhere near
							// usable
							if constexpr (__call_error_handler) {
								if (__init == __inlast) {
									__wide_execution_cwchar __self {};
									return __error_handler(__self,
										_Result(::std::forward<_InputRange>(__input),
										     ::std::forward<_OutputRange>(__output), __s,
										     encoding_error::incomplete_sequence),
										::ztd::span<code_unit>(::std::addressof(__units[0]), __units_count),
										::ztd::span<code_point, 0>());
								}
							}
							continue;
						}
					}

					__state_count += __res;
					break;
				}

				execution_t __exec {};
				__txt_detail::__pass_through_handler_with<!__call_error_handler> __intermediate_handler {};
				::ztd::span<char, __state_max> __intermediate_input(__intermediate_buffer, __state_max);
				auto __result = __exec.decode_one(__intermediate_input, ::std::forward<_OutputRange>(__output),
					__intermediate_handler, __s.__narrow_state);
				if constexpr (__call_error_handler) {
					if (__result.error_code != encoding_error::ok) {
						__wide_execution_cwchar __self {};
						return __error_handler(__self,
							_Result(ranges::reconstruct(::std::in_place_type<_UInputRange>, ::std::move(__init),
							             ::std::move(__inlast)),
							     ::std::move(__result.output), __s, __result.error_code),
							::ztd::span<code_unit>(::std::addressof(__units[0]), __units_count),
							__intermediate_handler.__M_code_points_progress());
					}
				}
				return _Result(ranges::reconstruct(
					               ::std::in_place_type<_UInputRange>, ::std::move(__init), ::std::move(__inlast)),
					::std::move(__result.output), __s, __result.error_code);
			}
		};
	} // namespace __txt_impl

	//////
	/// @}

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_DETAIL_WIDE_EXECUTION_CWCHAR_HPP
