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

#ifndef ZTD_TEXT_ASSUME_VALID_HANDLER_HPP
#define ZTD_TEXT_ASSUME_VALID_HANDLER_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/assert.hpp>

#if ZTD_IS_ON(ZTD_TEXT_ASSUME_VALID_HANDLER_TRAPS_ON_INVOCATION_I_)
#include <cstdlib>
#endif

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	//////
	/// @brief An error handler that tells an encoding that it will pass through any errors, without doing any
	/// adjustment, correction or checking.
	///
	/// @remarks This error handler is useful in conjunction with a ztd::text::ranges::unbounded_view for the fastest
	/// possible encoding and decoding in a general sense. However: IT IS ALSO EXTREMELY DANGEROUS AND CAN INVOKE
	/// UNDEFINED BEHAVIOR IF YOUR TEXT IS, IN FACT, MESSED UP. PLEASE DO NOT USE THIS WITHOUT A GOOD REASON!
	//////
	class assume_valid_handler_t {
	public:
		//////
		/// @brief A type that is true when calling code can not call this function and ignore it, and false when
		/// it cannot ignore it. See ztd::text::assume_valid_handler_t for details.
		//////
		using assume_valid = ::std::integral_constant<bool,
#if ZTD_IS_ON(ZTD_TEXT_ASSUME_VALID_HANDLER_TRAPS_ON_INVOCATION_I_)
			true
#else
			false
#endif
			>;

		//////
		/// @brief A handler for either decode or encode results that simply passes the result type back through
		/// with no changes made.
		///
		/// @param[in] __result The current state of the encode operation to pass through.
		//////
		template <typename _Encoding, typename _Result, typename _InputProgress, typename _OutputProgress>
		constexpr auto operator()(
			const _Encoding&, _Result __result, const _InputProgress&, const _OutputProgress&) const {
#if ZTD_IS_ON(ZTD_TEXT_ASSUME_VALID_HANDLER_TRAPS_ON_INVOCATION_I_)
			ZTD_TEXT_ASSERT_MESSAGE_I_(
				"You have invoked the ztd::text::assume_valid handler, and tripped undefined behavior. This means "
				"you violated the covenant between you, the compiler, this library, and every piece of code that "
				"depends on you. It is highly suggested that ztd::text::assume_valid_handler_t is not used except "
				"for the most extremely secure cases. If you cannot properly grok your threat model or what "
				"may or may not be external data sources, please do not use this type!",
				false);
			::std::abort();
#endif
			return __result;
		}
	};

	//////
	/// @brief An instance of the assume_valid_handler_t type for ease of use.
	inline constexpr assume_valid_handler_t assume_valid_handler = {};

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text


#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_ASSUME_VALID_HANDLER_HPP
