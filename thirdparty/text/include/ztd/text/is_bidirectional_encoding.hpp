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

#ifndef ZTD_TEXT_IS_BIDIRECTIONAL_ENCODING
#define ZTD_TEXT_IS_BIDIRECTIONAL_ENCODING

#include <ztd/text/version.hpp>

#include <ztd/text/type_traits.hpp>
#include <ztd/idk/span.hpp>

#include <type_traits>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	namespace __txt_detail {
		template <typename _Type, typename _Input, typename _Output, typename _State, typename _ErrorHandler>
		using __detect_encode_backward
			= decltype(::std::declval<_Type>().encode_one_backward(::std::declval<_Input&>(),
			     ::std::declval<_Output&>(), ::std::declval<_ErrorHandler&>(), ::std::declval<_State&>()));

		template <typename _Type, typename _Input, typename _Output, typename _State, typename _ErrorHandler>
		using __detect_decode_backward
			= decltype(::std::declval<_Type&>().decode_one_backward(::std::declval<_Input&>(),
			     ::std::declval<_Output&>(), ::std::declval<_ErrorHandler&>(), ::std::declval<_State&>()));
	} // namespace __txt_detail

	template <typename _Type, typename _Input, typename _Output, typename _State, typename _ErrorHandler>
	class is_bidirectional_encoding
	: public ::std::integral_constant<bool,
		  is_detected_v<__txt_detail::__detect_decode_backward, _Type, _Input, _Output, _State,
		       _ErrorHandler> && is_detected_v<__txt_detail::__detect_encode_backward, _Type, _Input, _Output, _State, _ErrorHandler>> {
	};

	template <typename _Type, typename _Input, typename _Output, typename _State, typename _ErrorHandler>
	inline constexpr bool is_bidirectional_encoding_v
		= is_bidirectional_encoding<_Type, _Input, _Output, _State, _ErrorHandler>::value;

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_IS_BIDIRECTIONAL_ENCODING
