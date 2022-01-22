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

#ifndef ZTD_TEXT_DETAIL_ENCODING_NAME_HPP
#define ZTD_TEXT_DETAIL_ENCODING_NAME_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/utf8.hpp>
#include <ztd/text/utf16.hpp>
#include <ztd/text/utf32.hpp>
#include <ztd/text/encoding_scheme.hpp>
#include <ztd/text/ascii.hpp>
#include <ztd/text/no_encoding.hpp>
#include <ztd/text/iconv_names.hpp>

#include <ztd/idk/encoding_name.hpp>
#include <ztd/idk/type_traits.hpp>

#include <string_view>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	namespace __txt_detail {

		template <typename _CharType, __idk_detail::__encoding_id _Id>
		constexpr auto __select_encoding() {
			if constexpr (_Id == __idk_detail::__encoding_id::__utf8) {
				return basic_utf8<_CharType> {};
			}
			else if constexpr (_Id == __idk_detail::__encoding_id::__mutf8) {
				return basic_mutf8<_CharType> {};
			}
			else if constexpr (_Id == __idk_detail::__encoding_id::__wtf8) {
				return basic_wtf8<_CharType> {};
			}
			else if constexpr (_Id == __idk_detail::__encoding_id::__utf16) {
				return basic_utf16<_CharType> {};
			}
			else if constexpr (_Id == __idk_detail::__encoding_id::__utf16le) {
				return basic_utf16_le<_CharType> {};
			}
			else if constexpr (_Id == __idk_detail::__encoding_id::__utf16be) {
				return basic_utf16_be<_CharType> {};
			}
			else if constexpr (_Id == __idk_detail::__encoding_id::__utf32) {
				return basic_utf32<_CharType> {};
			}
			else if constexpr (_Id == __idk_detail::__encoding_id::__utf32le) {
				return basic_utf32_le<_CharType> {};
			}
			else if constexpr (_Id == __idk_detail::__encoding_id::__utf32be) {
				return basic_utf32_be<_CharType> {};
			}
			else if constexpr (_Id == __idk_detail::__encoding_id::__ascii) {
				return basic_ascii<_CharType> {};
			}
			else {
				return basic_no_encoding<_CharType, unicode_code_point> {};
			}
		}

		template <typename _CodePoint>
		c_string_view __platform_utf_name() {
			constexpr ::std::size_t __bits = sizeof(_CodePoint) * CHAR_BIT;
			if constexpr (__bits <= 8) {
				return iconv_utf8_name;
			}
			else if constexpr (__bits <= 16) {
				return iconv_utf16_name;
			}
			else if constexpr (__bits <= 32) {
				return iconv_utf32_name;
			}
			else {
				static_assert(always_false_v<_CodePoint>,
					"[ztd.text] There is no good known default platform encoding for the given bit size in this "
					"platform. File a bug and tell us about your needs!");
				return iconv_locale_name;
			}
		}

	} // namespace __txt_detail

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_DETAIL_ENCODING_NAME_HPP
