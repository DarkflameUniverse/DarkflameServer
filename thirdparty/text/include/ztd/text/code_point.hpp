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

#ifndef ZTD_TEXT_CODE_POINT_HPP
#define ZTD_TEXT_CODE_POINT_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/unicode_code_point.hpp>
#include <ztd/text/type_traits.hpp>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	//////
	/// @addtogroup ztd_text_properties Property and Trait Helpers
	/// @{
	/////

	//////
	/// @brief Retrieves the code point type for the given type.
	template <typename _Type>
	class code_point {
	public:
		//////
		/// @brief The code point type for the given encoding type. If it does not exist, @c
		/// ztd::text::unicode_code_point is assumed.

		using type = typename remove_cvref_t<_Type>::code_point;
	};

	//////
	/// @brief A typename alias for ztd::text::code_point.
	template <typename _Type>
	using code_point_t = typename code_point<_Type>::type;

	//////
	/// @brief Gets the maximum number of code points that can be produced by an encoding during a decode operation,
	/// suitable for initializing a automatic storage duration ("stack-allocated") buffer.
	//////
	template <typename _Type>
	inline static constexpr ::std::size_t max_code_points_v = _Type::max_code_points;

	//////
	/// @}
	/////

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_CODE_POINT_HPP
