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

#ifndef ZTD_TEXT_IS_SELF_SYNCHRONIZING_CODE_HPP
#define ZTD_TEXT_IS_SELF_SYNCHRONIZING_CODE_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/type_traits.hpp>

#include <type_traits>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	namespace __txt_detail {
		template <typename _Type>
		using __detect_is_self_synchronizing_code = decltype(_Type::is_self_synchronizing_code::value);

		template <typename _Encoding, typename = void>
		struct __is_self_synchronizing_code_sfinae
		: ::std::integral_constant<bool, ::std::is_empty_v<decode_state_t<_Encoding>>> { };

		template <typename _Type>
		struct __is_self_synchronizing_code_sfinae<_Type,
			::std::enable_if_t<is_detected_v<__detect_is_self_synchronizing_code, _Type>>>
		: ::std::integral_constant<bool, _Type::self_synchronizing_code::value> { };
	} // namespace __txt_detail

	template <typename _Type>
	class is_self_synchronizing_code
	: public __txt_detail::__is_self_synchronizing_code_sfinae<remove_cvref_t<_Type>> { };

	template <typename _Type>
	inline constexpr bool is_self_synchronizing_code_v = is_self_synchronizing_code<_Type>::value;


	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_IS_SELF_SYNCHRONIZING_HPP
