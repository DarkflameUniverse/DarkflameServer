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

#ifndef ZTD_TEXT_IS_UNICODE_CODE_POINT_HPP
#define ZTD_TEXT_IS_UNICODE_CODE_POINT_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/forward.hpp>

#include <ztd/text/type_traits.hpp>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	template <typename _Type>
	class is_unicode_scalar_value : public ::std::integral_constant<bool,
		                                ::std::is_same_v<remove_cvref_t<_Type>, __txt_impl::__unicode_scalar_value>> {
	};

	template <typename _Type>
	inline constexpr bool is_unicode_scalar_value_v = is_unicode_scalar_value<_Type>::value;

	template <typename _Type>
	class is_unicode_code_point
	: public ::std::integral_constant<bool,
		  ::std::is_same_v<remove_cvref_t<_Type>,
		       char32_t> || ::std::is_same_v<remove_cvref_t<_Type>, __txt_impl::__unicode_code_point> || is_unicode_scalar_value_v<_Type>> {
	};

	template <typename _Type>
	inline constexpr bool is_unicode_code_point_v = is_unicode_code_point<_Type>::value;

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_IS_UNICODE_CODE_POINT_HPP
