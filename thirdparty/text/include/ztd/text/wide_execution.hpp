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

#ifndef ZTD_TEXT_WIDE_EXECUTION_HPP
#define ZTD_TEXT_WIDE_EXECUTION_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/impl/wide_execution_windows.hpp>
#include <ztd/text/impl/wide_execution_iso10646.hpp>
#include <ztd/text/impl/wide_execution_iconv.hpp>
#include <ztd/text/impl/wide_execution_cwchar.hpp>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	//////
	/// @addtogroup ztd_text_encodings Encodings
	/// @{
	//////

	//////
	/// @brief The Encoding that represents the "Wide Execution" (wide locale-based) encoding. The wide execution
	/// encoding is typically associated with the locale, which is tied to the C standard library's setlocale function.
	///
	/// @remarks Windows uses UTF-16, unless you call the C Standard Library directly. If `ZTD_TEXT_USE_CUNEICODE` or
	/// `ZTD_TEXT_ICONV` are not defined, this object may use the C Standard Library to perform transcoding if certain
	/// platform facilities are disabled or not available. If this is the case, the C Standard Library has fundamental
	/// limitations which may treat your UTF-16 data like UCS-2, and result in broken input/output. This object uses
	/// UTF-16 directly on Windows when possible to avoid some of the platform-specific shenanigans. It will attempt to
	/// do UTF-32 conversions where possible as well, relying on C Standard definitions.
	//////
	using wide_execution_t =
#if ZTD_IS_ON(ZTD_PLATFORM_WINDOWS_I_)
		__txt_impl::__wide_execution_windows
#elif ZTD_IS_ON(ZTD_WCHAR_T_UTF32_COMPATIBLE_I_)
		__txt_impl::__wide_execution_iso10646
#elif ZTD_IS_ON(ZTD_LIBICONV_I_)
		__txt_impl::__wide_execution_iconv
#else
		__txt_impl::__wide_execution_cwchar
#endif
		;

	//////
	/// @brief An instance of the wide_execution_t type for ease of use.
	inline constexpr wide_execution_t wide_execution = {};

	//////
	/// @}
	//////

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_WIDE_EXECUTION_HPP
