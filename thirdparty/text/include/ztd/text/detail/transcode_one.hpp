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

#ifndef ZTD_TEXT_DETAIL_TRANSCODE_ONE_HPP
#define ZTD_TEXT_DETAIL_TRANSCODE_ONE_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/encode_result.hpp>
#include <ztd/text/decode_result.hpp>
#include <ztd/text/transcode_result.hpp>
#include <ztd/text/validate_result.hpp>
#include <ztd/text/count_result.hpp>
#include <ztd/text/state.hpp>
#include <ztd/text/code_point.hpp>
#include <ztd/text/code_unit.hpp>
#include <ztd/text/text_tag.hpp>
#include <ztd/text/propagate_error.hpp>
#include <ztd/text/is_ignorable_error_handler.hpp>
#include <ztd/text/detail/pass_through_handler.hpp>
#include <ztd/text/detail/progress_handler.hpp>
#include <ztd/text/detail/is_lossless.hpp>
#include <ztd/text/detail/encoding_iterator_storage.hpp>
#include <ztd/text/detail/encoding_range.hpp>
#include <ztd/text/detail/span_or_reconstruct.hpp>

#include <ztd/idk/span.hpp>
#include <ztd/ranges/adl.hpp>
#include <ztd/ranges/unbounded.hpp>
#include <ztd/ranges/subrange.hpp>
#include <ztd/ranges/blackhole_iterator.hpp>
#include <ztd/ranges/algorithm.hpp>

#include <array>
#include <algorithm>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	namespace __txt_detail {

		enum class __consume : unsigned char { __no = 0, __embrace_the_void = 1 };
		enum class __transaction : unsigned char { __encode = 0, __decode = 1 };

		template <__consume _ConsumeIntoTheNothingness, typename _Encoding, typename _Input, typename _Output,
			typename _ErrorHandler, typename _State>
		constexpr auto __basic_decode_one(_Input&& __input, _Encoding&& __encoding, _Output&& __output,
			_ErrorHandler& __error_handler, _State& __state) {
			using _UOutput   = remove_cvref_t<_Output>;
			using _UEncoding = remove_cvref_t<_Encoding>;

			if constexpr (_ConsumeIntoTheNothingness == __consume::__embrace_the_void
				&& __is_decode_range_category_output_v<_UEncoding>) {
				(void)__output;
				using _Blackhole = ranges::unbounded_view<ranges::blackhole_iterator>;
				_Blackhole __output_range(ranges::blackhole_iterator {});
				return __encoding.decode_one(
					::std::forward<_Input>(__input), __output_range, __error_handler, __state);
			}
			else if constexpr (
				(!ranges::is_range_contiguous_range_v<_UOutput> || ranges::is_range_input_or_output_range_v<_UOutput>)&&__is_decode_range_category_contiguous_v<
				     _UEncoding>) {
				code_point_t<_UEncoding> __intermediate_output_storage[max_code_points_v<_UEncoding>];
				::ztd::span<code_point_t<_UEncoding>, max_code_points_v<_UEncoding>> __intermediate_output(
					__intermediate_output_storage);
				auto __intermediate_result = __encoding.decode_one(
					::std::forward<_Input>(__input), __intermediate_output, __error_handler, __state);
				using _ReconstructedResult = decode_result<decltype(__intermediate_result.input),
					ranges::range_reconstruct_t<_Output>, _State>;
				if (__intermediate_result.error_code != encoding_error::ok) {
					auto __intermediate_last = __intermediate_result.output.data();
					auto __out_it            = ranges::ranges_adl::adl_begin(::std::move(__output));
					auto __out_last          = ranges::ranges_adl::adl_end(::std::move(__output));
					for (auto __intermediate_it = __intermediate_output.data();
						__intermediate_it != __intermediate_last;) {
						*__out_it = *__intermediate_it;
						++__intermediate_it;
						++__out_it;
					}
					return _ReconstructedResult(::std::move(__intermediate_result.input),
						ranges::reconstruct(::std::in_place_type<_UOutput>, ::std::forward<_Output>(__output),
						     ::std::move(__out_it), ::std::move(__out_last)),
						__state, __intermediate_result.error_code, __intermediate_result.handled_errors);
				}
				return _ReconstructedResult(::std::move(__intermediate_result.input),
					ranges::reconstruct(::std::in_place_type<_UOutput>, ::std::forward<_Output>(__output)),
					__state, __intermediate_result.error_code, __intermediate_result.handled_errors);
			}
			else {
				return __encoding.decode_one(
					::std::forward<_Input>(__input), ::std::forward<_Output>(__output), __error_handler, __state);
			}
		}

		template <__consume _ConsumeIntoTheNothingness, typename _Encoding, typename _Input, typename _Output,
			typename _ErrorHandler, typename _State>
		constexpr auto __basic_encode_one(_Input&& __input, _Encoding&& __encoding, _Output&& __output,
			_ErrorHandler& __error_handler, _State& __state) {
			using _UOutput   = remove_cvref_t<_Output>;
			using _UEncoding = remove_cvref_t<_Encoding>;

			if constexpr (_ConsumeIntoTheNothingness == __consume::__embrace_the_void
				&& __is_encode_range_category_output_v<_UEncoding>) {
				(void)__output;
				using _Blackhole = ranges::unbounded_view<ranges::blackhole_iterator>;
				_Blackhole __output_range(ranges::blackhole_iterator {});
				return __encoding.encode_one(
					::std::forward<_Input>(__input), __output_range, __error_handler, __state);
			}
			else if constexpr (
				(!ranges::is_range_contiguous_range_v<_UOutput> || ranges::is_range_input_or_output_range_v<_UOutput>)&&__is_encode_range_category_contiguous_v<
				     _UEncoding>) {
				constexpr ::std::size_t _IntermediateMax = max_code_units_v<_UEncoding>;
				code_unit_t<_UEncoding> __intermediate_output_storage[_IntermediateMax];
				::ztd::span<code_unit_t<_UEncoding>, _IntermediateMax> __intermediate_output(
					__intermediate_output_storage);
				auto __intermediate_result = __encoding.encode_one(
					::std::forward<_Input>(__input), __intermediate_output, __error_handler, __state);
				using _ReconstructedResult = encode_result<decltype(__intermediate_result.input),
					ranges::range_reconstruct_t<_Output>, _State>;
				if (__intermediate_result.error_code != encoding_error::ok) {
					auto __intermediate_last = __intermediate_result.output.data();
					auto __out_it            = ranges::ranges_adl::adl_begin(::std::move(__output));
					auto __out_last          = ranges::ranges_adl::adl_end(::std::move(__output));
					for (auto __intermediate_it = __intermediate_output.data();
						__intermediate_it != __intermediate_last;) {
						*__out_it = *__intermediate_it;
						++__intermediate_it;
						++__out_it;
					}
					return _ReconstructedResult(::std::move(__intermediate_result.input),
						ranges::reconstruct(::std::in_place_type<_UOutput>, ::std::forward<_Output>(__output),
						     ::std::move(__out_it), ::std::move(__out_last)),
						__state, __intermediate_result.error_code, __intermediate_result.handled_errors);
				}
				return _ReconstructedResult(::std::move(__intermediate_result.input),
					ranges::reconstruct(::std::in_place_type<_UOutput>, ::std::forward<_Output>(__output)),
					__state, __intermediate_result.error_code, __intermediate_result.handled_errors);
			}
			else {
				return __encoding.encode_one(
					::std::forward<_Input>(__input), ::std::forward<_Output>(__output), __error_handler, __state);
			}
		}

		template <__consume _ConsumeIntoTheNothingness, __transaction __encode_or_decode, typename _Encoding,
			typename _Input, typename _OutputContainer, typename _ErrorHandler, typename _State>
		constexpr auto __basic_encode_or_decode_one(_Input&& __input, _Encoding&& __encoding,
			_OutputContainer& __output, _ErrorHandler& __error_handler, _State& __state) {
			if constexpr (__encode_or_decode == __transaction::__decode) {
				return __basic_decode_one<_ConsumeIntoTheNothingness>(::std::forward<_Input>(__input),
					::std::forward<_Encoding>(__encoding), __output, __error_handler, __state);
			}
			else {
				return __basic_encode_one<_ConsumeIntoTheNothingness>(::std::forward<_Input>(__input),
					::std::forward<_Encoding>(__encoding), __output, __error_handler, __state);
			}
		}

		template <__consume _ConsumeIntoTheNothingness, typename _Input, typename _FromEncoding, typename _Output,
			typename _ToEncoding, typename _FromErrorHandler, typename _ToErrorHandler, typename _FromState,
			typename _ToState, typename _Intermediate>
		constexpr auto __super_basic_transcode_one(_Input&& __input, _FromEncoding& __from_encoding,
			_Output&& __output, _ToEncoding& __to_encoding, _FromErrorHandler& __from_error_handler,
			_ToErrorHandler& __to_error_handler, _FromState& __from_state, _ToState& __to_state,
			_Intermediate& __intermediate) {
			using _InitialIntermediate = __span_or_reconstruct_t<_Intermediate>;
			using _DecodeResult        = decltype(__from_error_handler(__from_encoding, // clang-format hack
                    __basic_decode_one<_ConsumeIntoTheNothingness>(::std::forward<_Input>(__input), __from_encoding,
                         ::std::declval<_InitialIntermediate>(), __from_error_handler, __from_state),
                    ::ztd::span<code_unit_t<remove_cvref_t<_FromEncoding>>, 0>(),
                    ::ztd::span<code_point_t<remove_cvref_t<_FromEncoding>>, 0>()));
			using _InputView           = decltype(::std::declval<_DecodeResult>().input);
			using _WorkingIntermediate = decltype(::std::declval<_DecodeResult>().output);
			using _EncodeResult        = decltype(__to_error_handler(__to_encoding,
                    __basic_encode_one<_ConsumeIntoTheNothingness>(::std::declval<_WorkingIntermediate>(),
                         __to_encoding, ::std::forward<_Output>(__output), __to_error_handler, __to_state),
                    ::ztd::span<code_point_t<remove_cvref_t<_ToEncoding>>, 0>(),
                    ::ztd::span<code_unit_t<remove_cvref_t<_ToEncoding>>, 0>()));
			using _OutputView          = decltype(::std::declval<_EncodeResult>().output);
			using _Result              = transcode_result<_InputView, _OutputView, _FromState, _ToState>;

			static_assert(__txt_detail::__is_decode_lossless_or_deliberate_v<remove_cvref_t<_FromEncoding>,
				              remove_cvref_t<_FromErrorHandler>>,
				ZTD_TEXT_LOSSY_TRANSCODE_DECODE_MESSAGE_I_);
			static_assert(__txt_detail::__is_encode_lossless_or_deliberate_v<remove_cvref_t<_ToEncoding>,
				              remove_cvref_t<_ToErrorHandler>>,
				ZTD_TEXT_LOSSY_TRANSCODE_ENCODE_MESSAGE_I_);

#define ZTD_TEXT_SUPER_BASIC_TRANSCODE_COPY_PASTA_I_()                                                               \
	_WorkingIntermediate __working_input = ranges::reconstruct(::std::in_place_type<_WorkingIntermediate>,          \
	     ranges::ranges_adl::adl_begin(__working_output),                                                           \
	     ranges::ranges_adl::adl_begin(__intermediate_result.output));                                              \
	auto __end_result = __basic_encode_one<_ConsumeIntoTheNothingness>(::std::move(__working_input), __to_encoding, \
	     ::std::forward<_Output>(__output), __to_error_handler, __to_state);                                        \
	return _Result(::std::move(__intermediate_result.input), ::std::move(__end_result.output),                      \
	     __intermediate_result.state, __end_result.state, __end_result.error_code,                                  \
	     __intermediate_result.handled_errors + __end_result.handled_errors)

			constexpr bool _IsProgressHandler
				= is_specialization_of_v<decltype(__from_error_handler), __progress_handler> // clang-format hack
				|| is_specialization_of_v<decltype(__from_error_handler), __forwarding_progress_handler>;

			_WorkingIntermediate __working_output = __span_or_reconstruct(__intermediate);
			if constexpr (_IsProgressHandler) {
				auto __intermediate_result = __basic_decode_one<__consume::__no>(::std::forward<_Input>(__input),
					__from_encoding, __working_output, __from_error_handler, __from_state);
				if (__intermediate_result.error_code != encoding_error::ok) {
					::ztd::span<code_unit_t<remove_cvref_t<_ToEncoding>>, 0> __code_unit_progress {};
					return propagate_error<_Result>(::std::forward<_Output>(__output), __to_encoding,
						::std::move(__intermediate_result), __to_error_handler, __to_state,
						__from_error_handler._M_code_points_progress(), __code_unit_progress);
				}
				ZTD_TEXT_SUPER_BASIC_TRANSCODE_COPY_PASTA_I_();
			}
			else {
				using _CVFromEncoding     = ::std::remove_reference_t<_FromEncoding>;
				using _CVFromErrorHandler = ::std::remove_reference_t<_FromErrorHandler>;
				using _UFromErrorHandler  = remove_cvref_t<_FromErrorHandler>;
				using _FromAssumeValid
					= ::std::integral_constant<bool, is_ignorable_error_handler_v<_UFromErrorHandler>>;
				__forwarding_progress_handler<_FromAssumeValid, _CVFromErrorHandler, _CVFromEncoding>
					__intermediate_handler(__from_encoding, __from_error_handler);
				auto __intermediate_result = __basic_decode_one<__consume::__no>(::std::forward<_Input>(__input),
					__from_encoding, __working_output, __intermediate_handler, __from_state);
				if (__intermediate_result.error_code != encoding_error::ok) {
					::ztd::span<code_unit_t<remove_cvref_t<_ToEncoding>>, 0> __code_unit_progress {};
					return propagate_error<_Result>(::std::forward<_Output>(__output), __to_encoding,
						::std::move(__intermediate_result), __to_error_handler, __to_state,
						__intermediate_handler._M_code_points_progress(), __code_unit_progress);
				}
				ZTD_TEXT_SUPER_BASIC_TRANSCODE_COPY_PASTA_I_();
			}
#undef ZTD_TEXT_SUPER_BASIC_TRANSCODE_COPY_PASTA_I_
		}

		template <__consume _ConsumeIntoTheNothingness, typename _Input, typename _FromEncoding, typename _Output,
			typename _ToEncoding, typename _FromErrorHandler, typename _ToErrorHandler, typename _FromState,
			typename _ToState>
		constexpr auto __super_basic_transcode_one(_Input&& __input, _FromEncoding& __from_encoding,
			_Output&& __output, _ToEncoding& __to_encoding, _FromErrorHandler& __from_error_handler,
			_ToErrorHandler& __to_error_handler, _FromState& __from_state, _ToState& __to_state) {
			using _IntermediateCodePoint                       = code_point_t<_FromEncoding>;
			constexpr ::std::size_t _IntermediateMaxCodePoints = max_code_points_v<_FromEncoding>;

			_IntermediateCodePoint __intermediate_storage[_IntermediateMaxCodePoints] {};
			::ztd::span<_IntermediateCodePoint, _IntermediateMaxCodePoints> __intermediate(__intermediate_storage);
			return __super_basic_transcode_one<_ConsumeIntoTheNothingness>(::std::forward<_Input>(__input),
				__from_encoding, ::std::forward<_Output>(__output), __to_encoding, __from_error_handler,
				__to_error_handler, __from_state, __to_state, __intermediate);
		}

		template <__consume _ConsumeIntoTheNothingness, typename _Input, typename _FromEncoding, typename _Output,
			typename _ToEncoding, typename _FromErrorHandler, typename _ToErrorHandler, typename _FromState,
			typename _ToState, typename _Intermediate>
		constexpr auto __basic_transcode_one(_Input&& __input, _FromEncoding& __from_encoding, _Output&& __output,
			_ToEncoding& __to_encoding, _FromErrorHandler& __from_error_handler, _ToErrorHandler& __to_error_handler,
			_FromState& __from_state, _ToState& __to_state, _Intermediate& __intermediate) {
			using _UToEncoding = remove_cvref_t<_ToEncoding>;

			if constexpr (_ConsumeIntoTheNothingness == __consume::__embrace_the_void
				&& __is_encode_range_category_output_v<_UToEncoding>) {
				using _Blackhole = ranges::unbounded_view<ranges::blackhole_iterator>;
				if constexpr (is_detected_v<__detect_adl_text_transcode_one, _Input, _FromEncoding, _Blackhole,
					              _ToEncoding, _FromErrorHandler, _ToErrorHandler, _FromState, _ToState>) {
					(void)__intermediate;
					_Blackhole __output_range(ranges::blackhole_iterator {});
					return text_transcode_one(
						text_tag<remove_cvref_t<_FromEncoding>, remove_cvref_t<_ToEncoding>> {},
						::std::forward<_Input>(__input), __from_encoding, ::std::move(__output_range),
						__to_encoding, __from_error_handler, __to_error_handler, __from_state, __to_state);
				}
				else if constexpr (is_detected_v<__detect_adl_internal_text_transcode_one, _Input, _FromEncoding,
					                   _Blackhole, _ToEncoding, _FromErrorHandler, _ToErrorHandler, _FromState,
					                   _ToState>) {
					(void)__intermediate;
					_Blackhole __output_range(ranges::blackhole_iterator {});
					return __text_transcode_one(
						text_tag<remove_cvref_t<_FromEncoding>, remove_cvref_t<_ToEncoding>> {},
						::std::forward<_Input>(__input), __from_encoding, ::std::move(__output_range),
						__to_encoding, __from_error_handler, __to_error_handler, __from_state, __to_state);
				}
				else {
					return __super_basic_transcode_one<_ConsumeIntoTheNothingness>(::std::forward<_Input>(__input),
						__from_encoding, __intermediate, ::std::forward<_Output>(__output), __to_encoding,
						__from_error_handler, __to_error_handler, __from_state, __to_state);
				}
			}
			else {
				if constexpr (is_detected_v<__detect_adl_text_transcode_one, _Input, _FromEncoding, _Output,
					              _ToEncoding, _FromErrorHandler, _ToErrorHandler, _FromState, _ToState>) {
					(void)__intermediate;
					return text_transcode_one(
						text_tag<remove_cvref_t<_FromEncoding>, remove_cvref_t<_ToEncoding>> {},
						::std::forward<_Input>(__input), __from_encoding, ::std::forward<_Output>(__output),
						__to_encoding, __from_error_handler, __to_error_handler, __from_state, __to_state);
				}
				else if constexpr (is_detected_v<__detect_adl_internal_text_transcode_one, _Input, _FromEncoding,
					                   _Output, _ToEncoding, _FromErrorHandler, _ToErrorHandler, _FromState,
					                   _ToState>) {
					(void)__intermediate;
					return __text_transcode_one(
						text_tag<remove_cvref_t<_FromEncoding>, remove_cvref_t<_ToEncoding>> {},
						::std::forward<_Input>(__input), __from_encoding, ::std::forward<_Output>(__output),
						__to_encoding, __from_error_handler, __to_error_handler, __from_state, __to_state);
				}
				else {
					return __super_basic_transcode_one<_ConsumeIntoTheNothingness>(::std::forward<_Input>(__input),
						__from_encoding, ::std::forward<_Output>(__output), __to_encoding, __from_error_handler,
						__to_error_handler, __from_state, __to_state, __intermediate);
				}
			}
		}

		template <__consume _ConsumeIntoTheNothingness, typename _Input, typename _FromEncoding, typename _Output,
			typename _ToEncoding, typename _FromErrorHandler, typename _ToErrorHandler, typename _FromState,
			typename _ToState>
		constexpr auto __basic_transcode_one(_Input&& __input, _FromEncoding& __from_encoding, _Output&& __output,
			_ToEncoding& __to_encoding, _FromErrorHandler& __from_error_handler, _ToErrorHandler& __to_error_handler,
			_FromState& __from_state, _ToState& __to_state) {
			using _IntermediateCodePoint                       = code_point_t<_FromEncoding>;
			constexpr ::std::size_t _IntermediateMaxCodePoints = max_code_points_v<_FromEncoding>;

			_IntermediateCodePoint __intermediate_storage[_IntermediateMaxCodePoints] {};
			::ztd::span<_IntermediateCodePoint, _IntermediateMaxCodePoints> __intermediate(__intermediate_storage);
			return __basic_transcode_one<_ConsumeIntoTheNothingness>(::std::forward<_Input>(__input),
				__from_encoding, ::std::forward<_Output>(__output), __to_encoding, __from_error_handler,
				__to_error_handler, __from_state, __to_state, __intermediate);
		}

		template <typename _Input, typename _Encoding, typename _Output, typename _DecodeState, typename _EncodeState,
			typename _Intermediate>
		constexpr auto __basic_validate_decodable_as_one(_Input&& __input, _Encoding&& __encoding, _Output& __output,
			_DecodeState& __decode_state, _EncodeState& __encode_state, _Intermediate& __intermediate) {

			using _UInput  = remove_cvref_t<_Input>;
			using _UOutput = remove_cvref_t<_Output>;
			using _InSubRange
				= ranges::subrange<ranges::range_iterator_t<_UInput>, ranges::range_sentinel_t<_UInput>>;
			using _OutSubRange
				= ranges::subrange<ranges::range_iterator_t<_UOutput>, ranges::range_sentinel_t<_UOutput>>;

			_InSubRange __working_input(::std::forward<_Input>(__input));
			_OutSubRange __working_output(__output);
			__pass_through_handler __error_handler {};

			using _Result = validate_result<ranges::tag_range_reconstruct_t<_UInput, _InSubRange>, _DecodeState>;

			auto __transcode_result
				= __basic_transcode_one<__consume::__no>(__working_input, __encoding, __working_output, __encoding,
				     __error_handler, __error_handler, __decode_state, __encode_state, __intermediate);
			if (__transcode_result.error_code != encoding_error::ok) {
				return _Result(
					ranges::reconstruct(::std::in_place_type<_UInput>, ::std::move(__transcode_result.input)),
					false, __decode_state);
			}

			const bool __is_equal_transcode
				= ranges::__rng_detail::__equal(ranges::ranges_adl::adl_begin(__working_input),
				     ranges::ranges_adl::adl_begin(__transcode_result.input),
				     ranges::ranges_adl::adl_begin(__working_output),
				     ranges::ranges_adl::adl_begin(__transcode_result.output));
			if (!__is_equal_transcode) {
				return _Result(
					ranges::reconstruct(::std::in_place_type<_UInput>, ::std::move(__transcode_result.input)),
					false, __decode_state);
			}
			return _Result(ranges::reconstruct(::std::in_place_type<_UInput>, ::std::move(__transcode_result.input)),
				true, __decode_state);
		}

		template <typename _Input, typename _Encoding, typename _DecodeState, typename _EncodeState>
		constexpr auto __basic_validate_decodable_as_one(
			_Input&& __input, _Encoding&& __encoding, _DecodeState& __decode_state, _EncodeState& __encode_state) {
			using _UEncoding = remove_cvref_t<_Encoding>;
			using _CodeUnit  = code_unit_t<_UEncoding>;
			using _CodePoint = code_point_t<_UEncoding>;

			_CodePoint __intermediate_storage[max_code_points_v<_UEncoding>] {};
			::ztd::span<_CodePoint, max_code_points_v<_UEncoding>> __intermediate(__intermediate_storage);
			_CodeUnit __output_storage[max_code_units_v<_UEncoding>] {};
			::ztd::span<_CodeUnit, max_code_units_v<_UEncoding>> __output(__output_storage);
			return __basic_validate_decodable_as_one(::std::forward<_Input>(__input),
				::std::forward<_Encoding>(__encoding), __output, __decode_state, __encode_state, __intermediate);
		}

		template <typename _Input, typename _Encoding, typename _CodeUnitContainer, typename _EncodeState,
			typename _DecodeState, typename _Intermediate>
		constexpr auto __basic_validate_encodable_as_one(_Input&& __input, _Encoding&& __encoding,
			_CodeUnitContainer& __output, _EncodeState& __encode_state, _DecodeState& __decode_state,
			_Intermediate& __intermediate) {

			using _UInput        = remove_cvref_t<_Input>;
			using _UIntermediate = remove_cvref_t<_Intermediate>;
			using _InSubRange
				= ranges::subrange<ranges::range_iterator_t<_UInput>, ranges::range_sentinel_t<_UInput>>;
			using _IntermediateSubRange = ranges::subrange<ranges::range_iterator_t<_UIntermediate>,
				ranges::range_sentinel_t<_UIntermediate>>;
			using _Result = validate_result<ranges::tag_range_reconstruct_t<_UInput, _InSubRange>, _EncodeState>;

			__pass_through_handler __error_handler {};
			_InSubRange __working_input(::std::forward<_Input>(__input));

			auto __encode_result = __basic_encode_one<__consume::__no>(
				__working_input, __encoding, __output, __error_handler, __encode_state);
			if (__encode_result.error_code != encoding_error::ok) {
				return _Result(
					ranges::reconstruct(::std::in_place_type<_UInput>, ::std::move(__encode_result.input)), false,
					__encode_state);
			}

			_IntermediateSubRange __working_intermediate(__intermediate);

			auto __decode_result = __basic_decode_one<__consume::__no>(
				__output, __encoding, __working_intermediate, __error_handler, __decode_state);
			if (__decode_result.error_code != encoding_error::ok) {
				return _Result(
					ranges::reconstruct(::std::in_place_type<_UInput>, ::std::move(__encode_result.input)), false,
					__encode_state);
			}

			const bool __is_equal_transcode
				= ranges::__rng_detail::__equal(ranges::ranges_adl::adl_begin(__working_input),
				     ranges::ranges_adl::adl_begin(__encode_result.input),
				     ranges::ranges_adl::adl_begin(__working_intermediate),
				     ranges::ranges_adl::adl_begin(__decode_result.output));
			if (!__is_equal_transcode) {
				return _Result(
					ranges::reconstruct(::std::in_place_type<_UInput>, ::std::move(__encode_result.input)), false,
					__encode_state);
			}
			return _Result(ranges::reconstruct(::std::in_place_type<_UInput>, ::std::move(__encode_result.input)),
				true, __encode_state);
		}

		template <typename _Input, typename _Encoding, typename _EncodeState, typename _DecodeState>
		constexpr auto __basic_validate_encodable_as_one(
			_Input&& __input, _Encoding&& __encoding, _EncodeState& __encode_state, _DecodeState& __decode_state) {
			using _UEncoding = remove_cvref_t<_Encoding>;
			using _CodePoint = code_point_t<_UEncoding>;
			using _CodeUnit  = code_unit_t<_UEncoding>;

			_CodeUnit __output_storage[max_code_units_v<_UEncoding>] {};
			::ztd::span<_CodeUnit, max_code_units_v<_UEncoding>> __output(__output_storage);
			_CodePoint __intermediate_storage[max_code_points_v<_UEncoding>] {};
			::ztd::span<_CodePoint, max_code_points_v<_UEncoding>> __intermediate(__intermediate_storage);
			return __basic_validate_encodable_as_one(::std::forward<_Input>(__input),
				::std::forward<_Encoding>(__encoding), __output, __encode_state, __decode_state, __intermediate);
		}

		template <typename _Input, typename _Encoding, typename _ErrorHandler, typename _State,
			typename _Intermediate>
		constexpr auto __basic_count_as_encoded_one(_Input&& __input, _Encoding&& __encoding,
			_ErrorHandler&& __error_handler, _State& __state, _Intermediate& __intermediate) {
			using _Result = count_result<ranges::reconstruct_t<_Input>, _State>;

			auto __intermediate_result = __basic_encode_one<__consume::__no>(::std::forward<_Input>(__input),
				::std::forward<_Encoding>(__encoding), __intermediate,
				::std::forward<_ErrorHandler>(__error_handler), __state);
			::std::size_t __written
				= static_cast<::std::size_t>(ranges::ranges_adl::adl_data(__intermediate_result.output)
				     - ranges::ranges_adl::adl_data(__intermediate));

			return _Result(::std::move(__intermediate_result.input), __written, __intermediate_result.state,
				__intermediate_result.error_code, __intermediate_result.handled_errors);
		}

		template <typename _Input, typename _Encoding, typename _ErrorHandler, typename _State>
		constexpr auto __basic_count_as_encoded_one(
			_Input&& __input, _Encoding&& __encoding, _ErrorHandler&& __error_handler, _State& __state) {
			using _UEncoding                                  = remove_cvref_t<_Encoding>;
			using _IntermediateCodeUnit                       = code_unit_t<_UEncoding>;
			constexpr ::std::size_t _IntermediateMaxCodeUnits = max_code_units_v<_UEncoding>;

			_IntermediateCodeUnit __intermediate_storage[_IntermediateMaxCodeUnits] {};
			::ztd::span<_IntermediateCodeUnit, _IntermediateMaxCodeUnits> __intermediate(__intermediate_storage);
			return __basic_count_as_encoded_one(::std::forward<_Input>(__input),
				::std::forward<_Encoding>(__encoding), ::std::forward<_ErrorHandler>(__error_handler), __state,
				__intermediate);
		}

		template <typename _Input, typename _Encoding, typename _ErrorHandler, typename _State,
			typename _Intermediate>
		constexpr auto __basic_count_as_decoded_one(_Input&& __input, _Encoding&& __encoding,
			_ErrorHandler&& __error_handler, _State& __state, _Intermediate& __intermediate) {
			using _Result = count_result<ranges::reconstruct_t<_Input>, _State>;

			auto __intermediate_result = __basic_decode_one<__consume::__no>(::std::forward<_Input>(__input),
				::std::forward<_Encoding>(__encoding), __intermediate,
				::std::forward<_ErrorHandler>(__error_handler), __state);
			::std::size_t __written
				= static_cast<::std::size_t>(ranges::ranges_adl::adl_data(__intermediate_result.output)
				     - ranges::ranges_adl::adl_data(__intermediate));

			return _Result(::std::move(__intermediate_result.input), __written, __intermediate_result.state,
				__intermediate_result.error_code, __intermediate_result.handled_errors);
		}

		template <typename _Input, typename _Encoding, typename _ErrorHandler, typename _State>
		constexpr auto __basic_count_as_decoded_one(
			_Input&& __input, _Encoding&& __encoding, _ErrorHandler&& __error_handler, _State& __state) {
			using _UEncoding                                   = remove_cvref_t<_Encoding>;
			using _IntermediateCodePoint                       = code_point_t<_UEncoding>;
			constexpr ::std::size_t _IntermediateMaxCodePoints = max_code_points_v<_UEncoding>;


			_IntermediateCodePoint __intermediate_storage[_IntermediateMaxCodePoints] {};
			::ztd::span<_IntermediateCodePoint, _IntermediateMaxCodePoints> __intermediate(__intermediate_storage);
			return __basic_count_as_decoded_one(::std::forward<_Input>(__input),
				::std::forward<_Encoding>(__encoding), ::std::forward<_ErrorHandler>(__error_handler), __state,
				__intermediate);
		}
	} // namespace __txt_detail

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_DETAIL_TRANSCODE_ONE_HPP
