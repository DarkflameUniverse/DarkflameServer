



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

#ifndef ZTD_TEXT_DETAIL_CAST_HPP
#define ZTD_TEXT_DETAIL_CAST_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/type_traits.hpp>

#include <utility>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_
	namespace __txt_detail {
		enum class __match_alignment { no, yes };

		template <typename _To, __match_alignment __require_aligned = __match_alignment::no, typename _From>
		constexpr decltype(auto) static_cast_if_lossless(_From&& __from) {
			if constexpr ((sizeof(remove_cvref_t<_To>) == sizeof(remove_cvref_t<_From>))
				&& ((__require_aligned == __match_alignment::no)
				     || (alignof(remove_cvref_t<_To>) == alignof(remove_cvref_t<_From>)))) {
				// explicitly cast, since we know it's of the same size/alignment
				// (e.g., unsigned char -> std::byte should work, but it requires a cast!)
				return static_cast<_To>(__from);
			}
			else {
				// let it warn/error for weird conversions
				// (e.g., short -> char8_t should give a narrowing conversion warning)
				return ::std::forward<_From>(__from);
			}
		}
	} // namespace __txt_detail

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_DETAIL_CAST_HPP
