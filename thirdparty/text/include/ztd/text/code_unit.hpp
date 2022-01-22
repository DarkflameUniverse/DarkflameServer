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

#ifndef ZTD_TEXT_CODE_UNIT_HPP
#define ZTD_TEXT_CODE_UNIT_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/type_traits.hpp>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_


	//////
	/// @addtogroup ztd_text_properties Property and Trait Helpers
	///
	/// @brief Type-trait like properties and helpers, with `_t` ( @c \::type ) and `_v` ( @c \::value ).
	///
	/// @remarks These should be the go-to way to reference necessary type and constant static data out of encoding
	/// objects when not interfacing with the well-defined, guaranteed members. For example, directly accessing the @c
	/// obj::code_unit type is not viable for many encoding types and it is best to go through here.
	/// @{
	/////

	//////
	/// @brief Retrieves the code unit type for the given type.
	template <typename _Type>
	class code_unit {
	public:
		//////
		/// @brief The code unit type for the encoding type.
		using type = typename remove_cvref_t<_Type>::code_unit;
	};

	//////
	/// @brief A typename alias for ztd::text::code_unit.
	template <typename _Type>
	using code_unit_t = typename code_unit<_Type>::type;

	//////
	/// @brief Gets the maximum number of code units that can be produced by an encoding during an encode operation,
	/// suitable for initializing a automatic storage duration ("stack-allocated") buffer.
	//////
	template <typename _Type>
	inline static constexpr ::std::size_t max_code_units_v = _Type::max_code_units;

	//////
	/// @}
	/////

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_CODE_UNIT_HPP
