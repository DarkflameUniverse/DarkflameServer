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

#ifndef ZTD_TEXT_IS_CODE_POINTS_REPLACEABLE_HPP
#define ZTD_TEXT_IS_CODE_POINTS_REPLACEABLE_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/code_point.hpp>

#include <ztd/text/type_traits.hpp>

#include <type_traits>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	namespace __txt_detail {

		template <typename _Type, typename... _Args>
		using __detect_is_code_points_maybe_replaceable
			= decltype(::std::declval<const _Type&>().maybe_replacement_code_points(::std::declval<_Args>()...));

		template <typename _Type, typename... _Args>
		using __detect_is_code_points_replaceable
			= decltype(::std::declval<const _Type&>().replacement_code_points(::std::declval<_Args>()...));
	} // namespace __txt_detail

	//////
	/// @brief Checks whether the given encoding type returns a maybe-replacement range of code points.
	///
	/// @tparam _Type The type to check for the proper function call.
	///
	/// @remarks The `value` boolean is true if the given `_Type` has a function named @c
	/// maybe_replacement_code_points() on it that can be called from a `const` -qualified `_Type` object which
	/// returns a `std::optional` containing a contiguous view of code points.
	//////
	template <typename _Type, typename... _Args>
	class is_code_points_maybe_replaceable
	: public is_detected<__txt_detail::__detect_is_code_points_maybe_replaceable, _Type, _Args...> { };

	//////
	/// @brief A @c \::value alias for ztd::text::is_code_points_maybe_replaceable
	template <typename _Type, typename... _Args>
	inline constexpr bool is_code_points_maybe_replaceable_v
		= is_code_points_maybe_replaceable<_Type, _Args...>::value;

	//////
	/// @brief Checks whether the given encoding type returns a maybe-replacement range of code points.
	///
	/// @tparam _Type The type to check for the proper function call.
	///
	/// @remarks The `value` boolean is true if the given `_Type` has a function named @c
	/// replacement_code_points() on it that can be called from a `const` -qualified `_Type` object which returns a
	/// contiguous view of code points.
	//////
	template <typename _Type, typename... _Args>
	class is_code_points_replaceable
	: public is_detected<__txt_detail::__detect_is_code_points_replaceable, _Type, _Args...> { };

	//////
	/// @brief A @c \::value alias for ztd::text::is_code_points_replaceable
	template <typename _Type, typename... _Args>
	inline constexpr bool is_code_points_replaceable_v = is_code_points_replaceable<_Type, _Args...>::value;

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_IS_CODE_POINT_UNIT_REPLACEABLE_HPP
