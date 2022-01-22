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

#ifndef ZTD_TEXT_DETAIL_EXECUTION_ICONV_HPP
#define ZTD_TEXT_DETAIL_EXECUTION_ICONV_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/basic_iconv.hpp>
#include <ztd/text/iconv_names.hpp>

#if ZTD_IS_ON(ZTD_LIBICONV_I_)

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	namespace __impl {

		class __execution_iconv : public basic_iconv<char, unicode_code_point> {
		private:
			using __base_t = basic_iconv<char, unicode_code_point>;

		public:
			__execution_iconv() noexcept : __base_t(iconv_locale_name.base(), iconv_utf32_name.base()) {
			}
		};

	} // namespace __impl

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif

#endif // ZTD_TEXT_DETAIL_EXECUTION_ICONV_HPP
