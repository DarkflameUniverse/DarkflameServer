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

#ifndef ZTD_TEXT_TEXT_HPP
#define ZTD_TEXT_TEXT_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/basic_text.hpp>
#include <ztd/idk/charN_t.hpp>
#include <ztd/text/encoding.hpp>

#include <string>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	//////
	/// @brief A container for storing text in the locale, runtime-based encoding.
	using text = basic_text<execution_t>;
	//////
	/// @brief A container for storing text in the locale, runtime-based wide encoding.
	using wtext = basic_text<wide_execution_t>;
	//////
	/// @brief A container for storing text in the string literal_t encoding.
	using ltext = basic_text<literal_t>;
	//////
	/// @brief A container for storing text in the wide string literal_t encoding.
	using wltext = basic_text<wide_literal_t>;
	//////
	/// @brief A container for storing text in the UTF-8 encoding.
	using u8text = basic_text<utf8_t>;
	//////
	/// @brief A container for storing text in the UTF-16 encoding.
	using u16text = basic_text<utf16_t>;
	//////
	/// @brief A container for storing text in the UTF-32 encoding.
	using u32text = basic_text<utf32_t>;

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_TEXT_HPP
