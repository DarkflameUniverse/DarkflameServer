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

#ifndef ZTD_TEXT_PASS_HANDLER_HPP
#define ZTD_TEXT_PASS_HANDLER_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/detail/pass_through_handler.hpp>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	//////
	/// @brief An error handler that tells an encoding that it will pass through any errors, without doing any
	/// adjustment, correction or checking. Does not imply it is ignorable, unlike ztd::text::assume_valid_handler_t
	/// which can invoke UB if an error occurs.
	//////
	class pass_handler_t : public __txt_detail::__pass_through_handler_with<false> { };

	//////
	/// @brief An instance of pass_handler_t for ease of use.
	inline constexpr pass_handler_t pass_handler = {};

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text


#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_PASS_HANDLER_HPP
