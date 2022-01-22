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

#ifndef ZTD_TEXT_BASIC_TEXT_HPP
#define ZTD_TEXT_BASIC_TEXT_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/basic_text_view.hpp>
#include <ztd/text/error_handler.hpp>
#include <ztd/text/normalization.hpp>
#include <ztd/text/code_unit.hpp>

#include <string>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	//////
	/// @brief A wrapper (container adapter) that takes the given `_Encoding` type and `_NormalizationForm` type and
	/// imposes it over the given chosen `_Container` storage for the purposes of allowing users to examine the text.
	///
	/// @tparam _Encoding The encoding to store any input and presented text as.
	/// @tparam _NormalizationForm The normalization form to impose on the stored text's sequences.
	/// @tparam _Container The container type that will be stored within this ztd::text::basic_text using the code
	/// units from the `_Encoding` type.
	/// @tparam _ErrorHandler The default error handler to use for any and all operations on text. Generally, most
	/// operations will provide room to override this.
	//////
	template <typename _Encoding, typename _NormalizationForm = nfkc,
		typename _Container = ::std::basic_string<code_unit_t<_Encoding>>, typename _ErrorHandler = default_handler_t>
	class basic_text : private basic_text_view<_Encoding, _NormalizationForm, _Container, _ErrorHandler> {
	private:
		using __base_t = basic_text_view<_Encoding, _NormalizationForm, _Container, _ErrorHandler>;

	public:
		//////
		/// @brief The type that this view is wrapping.
		using range_type = typename __base_t::range_type;
		//////
		/// @brief The encoding type that this view is using to interpret the underlying sequence of code units.
		using encoding_type = typename __base_t::encoding_type;
		//////
		/// @brief The encoding type that this view is using to interpret the underlying sequence of code units.
		using state_type = typename __base_t::state_type;
		//////
		/// @brief The normalization form type this view is imposing on top of the encoded sequence.
		using normalization_type = typename __base_t::normalization_type;
		//////
		/// @brief The error handling type used by default for any problems in conversions.
		using error_handler_type = typename __base_t::error_handler_type;

		using __base_t::code_points;

		using __base_t::base;
	};

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_BASIC_TEXT_HPP
