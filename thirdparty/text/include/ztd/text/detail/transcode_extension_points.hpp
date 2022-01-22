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

#ifndef ZTD_TEXT_DETAIL_TRANSCODE_EXTENSION_POINTS_HPP
#define ZTD_TEXT_DETAIL_TRANSCODE_EXTENSION_POINTS_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/encode_result.hpp>
#include <ztd/text/decode_result.hpp>
#include <ztd/text/transcode_result.hpp>
#include <ztd/text/state.hpp>
#include <ztd/text/code_point.hpp>
#include <ztd/text/code_unit.hpp>
#include <ztd/text/text_tag.hpp>



#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	// These optimizations can only be applied when the encodings are identical AND both
	// is_ignorable_error_handler_v<FromErrorHandler> and is_ignorable_error_handler_v<ToErrorHandler> are true. In the
	// other case, at the very least a decode needs to be performed. If it's successful, THEN we memcpy the used bits
	// from the input directly to the output. This relies on the operation being symmetric for decode and encode,
	// which is not EXACTLY a requirement, but it's worth doing anyhow.
#if 0
	template <typename _Input, typename _Output, typename _FromEncoding, typename _ToEncoding,
		typename _FromErrorHandler, typename _ToErrorHandler,
		::std::enable_if_t<::std::is_same_v<remove_cvref_t<_FromEncoding>,
		     remove_cvref_t<_ToEncoding>>>* = nullptr>
	constexpr auto __text_transcode_one(
		text_tag<remove_cvref_t<_FromEncoding>, remove_cvref_t<_ToEncoding>>,
		_Input&& __input, _FromEncoding&& __from_encoding, _Output&& __output, _ToEncoding&& __to_encoding,
		_FromErrorHandler&& __from_error_handler, _ToErrorHandler&& __to_error_handler,
		decode_state_t<remove_cvref_t<_FromEncoding>>& __from_state,
		encode_state_t<remove_cvref_t<_ToEncoding>>& __to_state) {
		using _FromState = decode_state_t<remove_cvref_t<_FromEncoding>>;
		using _ToState   = encode_state_t<remove_cvref_t<_ToEncoding>>;
		__txt_detail::__copy(ranges::ranges_adl::adl_begin(__input), ranges::ranges_adl::adl_end(__input),
			ranges::ranges_adl::adl_begin(__output), ranges::ranges_adl::adl_end(__output));
		return transcode_result<_Input, _Output, _FromState, _ToState>(::std::forward<_Input>(__input),
			::std::forward<_Output>(__output), __from_state, __to_state, encoding_error::ok);
	}

	template <typename _Input, typename _Output, typename _FromEncoding, typename _ToEncoding,
		typename _FromErrorHandler, typename _ToErrorHandler,
		::std::enable_if_t<::std::is_same_v<remove_cvref_t<_FromEncoding>,
		     remove_cvref_t<_ToEncoding>>>* = nullptr>
	constexpr auto __text_transcode(
		text_tag<remove_cvref_t<_FromEncoding>, remove_cvref_t<_ToEncoding>>,
		_Input&& __input, _FromEncoding&& __from_encoding, _Output&& __output, _ToEncoding&& __to_encoding,
		_FromErrorHandler&& __from_error_handler, _ToErrorHandler&& __to_error_handler,
		decode_state_t<remove_cvref_t<_FromEncoding>>& __from_state,
		encode_state_t<remove_cvref_t<_ToEncoding>>& __to_state) {
		using _FromState = decode_state_t<remove_cvref_t<_FromEncoding>>;
		using _ToState   = encode_state_t<remove_cvref_t<_ToEncoding>>;
		__txt_detail::__copy(ranges::ranges_adl::adl_begin(__input), ranges::ranges_adl::adl_end(__input),
			ranges::ranges_adl::adl_begin(__output), ranges::ranges_adl::adl_end(__output));
		return transcode_result<_Input, _Output, _FromState, _ToState>(::std::forward<_Input>(__input),
			::std::forward<_Output>(__output), __from_state, __to_state, encoding_error::ok);
	}

#endif

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_DETAIL_TRANSCODE_EXTENSION_POINTS_HPP
