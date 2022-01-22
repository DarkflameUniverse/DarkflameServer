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

#ifndef ZTD_TEXT_DETAIL_ENCODING_RANGE_HPP
#define ZTD_TEXT_DETAIL_ENCODING_RANGE_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/state.hpp>
#include <ztd/text/code_point.hpp>
#include <ztd/text/code_unit.hpp>
#include <ztd/text/text_tag.hpp>
#include <ztd/text/type_traits.hpp>

#include <ztd/ranges/adl.hpp>
#include <ztd/ranges/iterator.hpp>

#include <utility>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_
	namespace __txt_detail {

		// validation: decode
		template <typename _Input, typename _Encoding, typename _State, typename... _AdditionalStates>
		using __detect_adl_internal_text_validate_decodable_as
			= decltype(__text_validate_decodable_as(text_tag<remove_cvref_t<_Encoding>> {}, ::std::declval<_Input>(),
			     ::std::declval<_Encoding>(), ::std::declval<_State&>(), ::std::declval<_AdditionalStates&>()...));

		template <typename _Input, typename _Encoding, typename _State, typename... _AdditionalStates>
		using __detect_adl_text_validate_decodable_as
			= decltype(text_validate_decodable_as(text_tag<remove_cvref_t<_Encoding>> {}, ::std::declval<_Input>(),
			     ::std::declval<_Encoding>(), ::std::declval<_State&>(), ::std::declval<_AdditionalStates&>()...));

		template <typename _Input, typename _Encoding, typename _State, typename... _AdditionalStates>
		using __detect_adl_text_validate_decodable_as_one = decltype(text_validate_decodable_as_one(
			text_tag<remove_cvref_t<_Encoding>> {}, ::std::declval<_Input>(), ::std::declval<_Encoding>(),
			::std::declval<_State&>(), ::std::declval<_AdditionalStates&>()...));

		template <typename _Input, typename _Encoding, typename _State, typename... _AdditionalStates>
		using __detect_adl_internal_text_validate_decodable_as_one = decltype(__text_validate_decodable_as_one(
			text_tag<remove_cvref_t<_Encoding>> {}, ::std::declval<_Input>(), ::std::declval<_Encoding>(),
			::std::declval<_State&>(), ::std::declval<_AdditionalStates&>()...));

		// validation: encode
		template <typename _Input, typename _Encoding, typename _State, typename... _AdditionalStates>
		using __detect_adl_internal_text_validate_encodable_as
			= decltype(__text_validate_encodable_as(text_tag<remove_cvref_t<_Encoding>> {}, ::std::declval<_Input>(),
			     ::std::declval<_Encoding>(), ::std::declval<_State&>(), ::std::declval<_AdditionalStates&>()...));

		template <typename _Input, typename _Encoding, typename _State, typename... _AdditionalStates>
		using __detect_adl_text_validate_encodable_as
			= decltype(text_validate_encodable_as(text_tag<remove_cvref_t<_Encoding>> {}, ::std::declval<_Input>(),
			     ::std::declval<_Encoding>(), ::std::declval<_State&>(), ::std::declval<_AdditionalStates&>()...));

		template <typename _Input, typename _Encoding, typename _State, typename... _AdditionalStates>
		using __detect_adl_internal_text_validate_encodable_as_one = decltype(__text_validate_encodable_as_one(
			text_tag<remove_cvref_t<_Encoding>> {}, ::std::declval<_Input>(), ::std::declval<_Encoding>(),
			::std::declval<_State&>(), ::std::declval<_AdditionalStates&>()...));

		template <typename _Input, typename _Encoding, typename _State, typename... _AdditionalStates>
		using __detect_adl_text_validate_encodable_as_one = decltype(text_validate_encodable_as_one(
			text_tag<remove_cvref_t<_Encoding>> {}, ::std::declval<_Input>(), ::std::declval<_Encoding>(),
			::std::declval<_State&>(), ::std::declval<_AdditionalStates&>()...));

		// validation: transcode
		template <typename _Input, typename _FromEncoding, typename _ToEncoding, typename _FromState,
			typename _ToState>
		using __detect_adl_internal_text_validate_transcodable_as = decltype(__text_validate_encodable_as(
			text_tag<remove_cvref_t<_FromEncoding>, remove_cvref_t<_ToEncoding>> {}, ::std::declval<_Input>(),
			::std::declval<_FromEncoding>(), ::std::declval<_ToEncoding>(), ::std::declval<_FromState&>(),
			::std::declval<_ToState&>()));

		template <typename _Input, typename _FromEncoding, typename _ToEncoding, typename _FromState,
			typename _ToState>
		using __detect_adl_text_validate_transcodable_as = decltype(text_validate_transcodable_as(
			text_tag<remove_cvref_t<_FromEncoding>, remove_cvref_t<_ToEncoding>> {}, ::std::declval<_Input>(),
			::std::declval<_FromEncoding>(), ::std::declval<_ToEncoding>(), ::std::declval<_FromState&>(),
			::std::declval<_ToState&>()));

		template <typename _Input, typename _FromEncoding, typename _ToEncoding, typename _FromState,
			typename _ToState>
		using __detect_adl_internal_text_validate_transcodable_as_one = decltype(__text_validate_transcodable_as_one(
			text_tag<remove_cvref_t<_FromEncoding>, remove_cvref_t<_ToEncoding>> {}, ::std::declval<_Input>(),
			::std::declval<_FromEncoding>(), ::std::declval<_ToEncoding>(), ::std::declval<_FromState&>(),
			::std::declval<_ToState&>()));

		template <typename _Input, typename _FromEncoding, typename _ToEncoding, typename _FromState,
			typename _ToState>
		using __detect_adl_text_validate_transcodable_as_one = decltype(text_validate_transcodable_as_one(
			text_tag<remove_cvref_t<_FromEncoding>, remove_cvref_t<_ToEncoding>> {}, ::std::declval<_Input>(),
			::std::declval<_FromEncoding>(), ::std::declval<_ToEncoding>(), ::std::declval<_FromState&>(),
			::std::declval<_ToState&>()));

		// counting: code units
		template <typename _Input, typename _Encoding, typename _Handler, typename _State>
		using __detect_adl_internal_text_count_as_decoded_one
			= decltype(__text_count_as_decoded_one(text_tag<remove_cvref_t<_Encoding>> {}, ::std::declval<_Input>(),
			     ::std::declval<_Encoding>(), ::std::declval<_Handler>(), ::std::declval<_State&>()));

		template <typename _Input, typename _Encoding, typename _Handler, typename _State>
		using __detect_adl_text_count_as_decoded_one
			= decltype(text_count_as_decoded_one(text_tag<remove_cvref_t<_Encoding>> {}, ::std::declval<_Input>(),
			     ::std::declval<_Encoding>(), ::std::declval<_Handler>(), ::std::declval<_State&>()));

		template <typename _Input, typename _Encoding, typename _Handler, typename _State>
		using __detect_adl_text_count_as_decoded
			= decltype(text_count_as_decoded(text_tag<remove_cvref_t<_Encoding>> {}, ::std::declval<_Input>(),
			     ::std::declval<_Encoding>(), ::std::declval<_Handler>(), ::std::declval<_State&>()));

		template <typename _Input, typename _Encoding, typename _Handler, typename _State>
		using __detect_adl_internal_text_count_as_decoded
			= decltype(__text_count_as_decoded(text_tag<remove_cvref_t<_Encoding>> {}, ::std::declval<_Input>(),
			     ::std::declval<_Encoding>(), ::std::declval<_Handler>(), ::std::declval<_State&>()));

		// counting: code points
		template <typename _Input, typename _Encoding, typename _Handler, typename _State>
		using __detect_adl_internal_text_count_as_encoded_one = decltype(__text_count_as_encoded_one(
			text_tag<remove_cvref_t<_Encoding>> {}, ::std::declval<_Encoding>(), ::std::declval<_Input>(),
			::std::declval<_Handler>(), ::std::declval<_State&>()));

		template <typename _Input, typename _Encoding, typename _Handler, typename _State>
		using __detect_adl_text_count_as_encoded_one
			= decltype(text_count_as_encoded_one(text_tag<remove_cvref_t<_Encoding>> {}, ::std::declval<_Encoding>(),
			     ::std::declval<_Input>(), ::std::declval<_Handler>(), ::std::declval<_State&>()));

		template <typename _Input, typename _Encoding, typename _Handler, typename _State>
		using __detect_adl_text_count_as_encoded
			= decltype(text_count_as_encoded(text_tag<remove_cvref_t<_Encoding>> {}, ::std::declval<_Input>(),
			     ::std::declval<_Encoding>(), ::std::declval<_Handler>(), ::std::declval<_State&>()));

		template <typename _Input, typename _Encoding, typename _Handler, typename _State>
		using __detect_adl_internal_text_count_as_encoded = decltype(__text_count_as_encoded(::std::declval<_Input>(),
			::std::declval<_Encoding>(), ::std::declval<_Handler>(), ::std::declval<_State&>()));

		// counting: transcode code units
		template <typename _Input, typename _FromEncoding, typename _ToEncoding, typename _FromHandler,
			typename _ToHandler, typename _FromState, typename _ToState>
		using __detect_adl_internal_text_count_as_transcoded_one = decltype(__text_count_as_encoded_one(
			text_tag<remove_cvref_t<_FromEncoding>, remove_cvref_t<_ToEncoding>> {}, ::std::declval<_FromEncoding>(),
			::std::declval<_ToEncoding>(), ::std::declval<_Input>(), ::std::declval<_FromHandler>(),
			::std::declval<_ToHandler>(), ::std::declval<_FromState&>(), ::std::declval<_ToState&>()));

		template <typename _Input, typename _FromEncoding, typename _ToEncoding, typename _FromHandler,
			typename _ToHandler, typename _FromState, typename _ToState>
		using __detect_adl_text_count_as_transcoded_one = decltype(text_count_as_encoded_one(
			text_tag<remove_cvref_t<_FromEncoding>, remove_cvref_t<_ToEncoding>> {}, ::std::declval<_Input>(),
			::std::declval<_FromEncoding>(), ::std::declval<_ToEncoding>(), ::std::declval<_FromHandler>(),
			::std::declval<_ToHandler>(), ::std::declval<_FromState&>(), ::std::declval<_ToState&>()));

		template <typename _Input, typename _FromEncoding, typename _ToEncoding, typename _FromHandler,
			typename _ToHandler, typename _FromState, typename _ToState>
		using __detect_adl_text_count_as_transcoded = decltype(text_count_as_encoded(
			text_tag<remove_cvref_t<_FromEncoding>, remove_cvref_t<_ToEncoding>> {}, ::std::declval<_Input>(),
			::std::declval<_FromEncoding>(), ::std::declval<_ToEncoding>(), ::std::declval<_FromHandler>(),
			::std::declval<_ToHandler>(), ::std::declval<_FromState&>(), ::std::declval<_ToState&>()));

		template <typename _Input, typename _FromEncoding, typename _ToEncoding, typename _FromHandler,
			typename _ToHandler, typename _FromState, typename _ToState>
		using __detect_adl_internal_text_count_as_transcoded = decltype(__text_count_as_encoded(
			text_tag<remove_cvref_t<_FromEncoding>, remove_cvref_t<_ToEncoding>> {}, ::std::declval<_Input>(),
			::std::declval<_FromEncoding>(), ::std::declval<_ToEncoding>(), ::std::declval<_FromHandler>(),
			::std::declval<_ToHandler>(), ::std::declval<_FromState&>(), ::std::declval<_ToState&>()));


		// decode
		template <typename _Encoding, typename _Input, typename _Output, typename _Handler, typename _State>
		using __detect_object_decode_one = decltype(::std::declval<_Encoding>().decode_one(::std::declval<_Input>(),
			::std::declval<_Output>(), ::std::declval<_Handler>(), ::std::declval<_State&>()));

		template <typename _Encoding, typename _Input, typename _Output, typename _Handler, typename _State>
		using __detect_object_decode_one_backwards
			= decltype(::std::declval<_Encoding>().decode_one_backwards(::std::declval<_Input>(),
			     ::std::declval<_Output>(), ::std::declval<_Handler>(), ::std::declval<_State&>()));

		template <typename _Input, typename _Encoding, typename _Output, typename _Handler, typename _State>
		using __detect_adl_text_decode = decltype(text_decode(text_tag<remove_cvref_t<_Encoding>> {},
			::std::declval<_Input>(), ::std::declval<_Encoding>(), ::std::declval<_Output>(),
			::std::declval<_Handler>(), ::std::declval<_State&>()));

		template <typename _Input, typename _Encoding, typename _Output, typename _Handler, typename _State>
		using __detect_adl_internal_text_decode = decltype(__text_decode(text_tag<remove_cvref_t<_Encoding>> {},
			::std::declval<_Input>(), ::std::declval<_Encoding>(), ::std::declval<_Output>(),
			::std::declval<_Handler>(), ::std::declval<_State&>()));

		// encode
		template <typename _Encoding, typename _Input, typename _Output, typename _Handler, typename _State>
		using __detect_object_encode_one = decltype(::std::declval<_Encoding>().encode_one(::std::declval<_Input>(),
			::std::declval<_Output>(), ::std::declval<_Handler>(), ::std::declval<_State&>()));

		template <typename _Encoding, typename _Input, typename _Output, typename _Handler, typename _State>
		using __detect_object_encode_one_backwards
			= decltype(::std::declval<_Encoding>().encode_one_backwards(::std::declval<_Input>(),
			     ::std::declval<_Output>(), ::std::declval<_Handler>(), ::std::declval<_State&>()));

		template <typename _Input, typename _Encoding, typename _Output, typename _Handler, typename _State>
		using __detect_adl_text_encode = decltype(text_encode(text_tag<remove_cvref_t<_Encoding>> {},
			::std::declval<_Input>(), ::std::declval<_Encoding>(), ::std::declval<_Output>(),
			::std::declval<_Handler>(), ::std::declval<_State&>()));

		template <typename _Input, typename _Encoding, typename _Output, typename _Handler, typename _State>
		using __detect_adl_internal_text_encode = decltype(__text_encode(text_tag<remove_cvref_t<_Encoding>> {},
			::std::declval<_Input>(), ::std::declval<_Encoding>(), ::std::declval<_Output>(),
			::std::declval<_Handler>(), ::std::declval<_State&>()));

		// transcode
		template <typename _Input, typename _FromEncoding, typename _Output, typename _ToEncoding,
			typename _FromHandler, typename _ToHandler, typename _FromState, typename _ToState>
		using __detect_adl_text_transcode
			= decltype(text_transcode(text_tag<remove_cvref_t<_FromEncoding>, remove_cvref_t<_ToEncoding>> {},
			     ::std::declval<_Input>(), ::std::declval<_FromEncoding>(), ::std::declval<_Output>(),
			     ::std::declval<_ToEncoding>(), ::std::declval<_FromHandler>(), ::std::declval<_ToHandler>(),
			     ::std::declval<_FromState&>(), ::std::declval<_ToState&>()));

		template <typename _Input, typename _FromEncoding, typename _Output, typename _ToEncoding,
			typename _FromHandler, typename _ToHandler, typename _FromState, typename _ToState>
		using __detect_adl_internal_text_transcode
			= decltype(__text_transcode(text_tag<remove_cvref_t<_FromEncoding>, remove_cvref_t<_ToEncoding>> {},
			     ::std::declval<_Input>(), ::std::declval<_FromEncoding>(), ::std::declval<_Output>(),
			     ::std::declval<_ToEncoding>(), ::std::declval<_FromHandler>(), ::std::declval<_ToHandler>(),
			     ::std::declval<_FromState&>(), ::std::declval<_ToState&>()));

		template <typename _Input, typename _FromEncoding, typename _Output, typename _ToEncoding,
			typename _FromHandler, typename _ToHandler, typename _FromState, typename _ToState>
		using __detect_adl_internal_text_transcode_one
			= decltype(__text_transcode_one(text_tag<remove_cvref_t<_FromEncoding>, remove_cvref_t<_ToEncoding>> {},
			     ::std::declval<_Input>(), ::std::declval<_FromEncoding>(), ::std::declval<_Output>(),
			     ::std::declval<_ToEncoding>(), ::std::declval<_FromHandler>(), ::std::declval<_ToHandler>(),
			     ::std::declval<_FromState&>(), ::std::declval<_ToState&>()));

		template <typename _Input, typename _FromEncoding, typename _Output, typename _ToEncoding,
			typename _FromHandler, typename _ToHandler, typename _FromState, typename _ToState>
		using __detect_adl_text_transcode_one
			= decltype(text_transcode_one(text_tag<remove_cvref_t<_FromEncoding>, remove_cvref_t<_ToEncoding>> {},
			     ::std::declval<_Input>(), ::std::declval<_FromEncoding>(), ::std::declval<_Output>(),
			     ::std::declval<_ToEncoding>(), ::std::declval<_FromHandler>(), ::std::declval<_ToHandler>(),
			     ::std::declval<_FromState&>(), ::std::declval<_ToState&>()));

		template <typename _Handler, typename _Encoding, typename _Result, typename _Progress>
		using __detect_callable_handler = decltype(::std::declval<_Handler>()(
			::std::declval<const _Encoding&>(), ::std::declval<_Result>(), ::std::declval<_Progress>()));

		template <typename _Encoding, typename = void>
		struct __range_category {
			using type = void;
		};

		template <typename _Encoding>
		struct __range_category<_Encoding, ::std::void_t<typename remove_cvref_t<_Encoding>::range_category>> {
			using type = typename remove_cvref_t<_Encoding>::range_category;
		};

		template <typename _Encoding, typename = void>
		struct __decode_range_category : public __range_category<_Encoding> { };

		template <typename _Encoding>
		struct __decode_range_category<_Encoding,
			::std::void_t<typename remove_cvref_t<_Encoding>::decode_range_category>> {
			using type = typename remove_cvref_t<_Encoding>::decode_range_category;
		};

		template <typename _Encoding, typename = void>
		struct __encode_range_category : public __range_category<_Encoding> { };

		template <typename _Encoding>
		struct __encode_range_category<_Encoding,
			::std::void_t<typename remove_cvref_t<_Encoding>::encode_range_category>> {
			using type = typename remove_cvref_t<_Encoding>::encode_range_category;
		};

		template <typename _Encoding>
		using __decode_range_category_t = typename __decode_range_category<_Encoding>::type;

		template <typename _Encoding>
		using __encode_range_category_t = typename __encode_range_category<_Encoding>::type;

		template <typename _Encoding>
		inline constexpr bool __is_decode_range_category_output_v
			= ::std::is_base_of_v<__decode_range_category_t<_Encoding>, ::std::output_iterator_tag>;

		template <typename _Encoding>
		inline constexpr bool __is_encode_range_category_output_v
			= ::std::is_base_of_v<__encode_range_category_t<_Encoding>, ::std::output_iterator_tag>;

		template <typename _Encoding>
		inline constexpr bool __is_encode_range_category_contiguous_v
			= ::std::is_base_of_v<__encode_range_category_t<_Encoding>, ::ztd::contiguous_iterator_tag>;

		template <typename _Encoding>
		inline constexpr bool __is_decode_range_category_contiguous_v
			= ::std::is_base_of_v<__decode_range_category_t<_Encoding>, ::ztd::contiguous_iterator_tag>;

	} // namespace __txt_detail
	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_DETAIL_ENCODING_RANGE_HPP
