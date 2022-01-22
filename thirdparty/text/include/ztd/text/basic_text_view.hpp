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

#ifndef ZTD_TEXT_BASIC_TEXT_VIEW_HPP
#define ZTD_TEXT_BASIC_TEXT_VIEW_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/code_unit.hpp>
#include <ztd/text/code_point.hpp>
#include <ztd/text/error_handler.hpp>
#include <ztd/text/normalization.hpp>
#include <ztd/text/decode_view.hpp>
#include <ztd/text/state.hpp>

#include <string_view>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	//////
	/// @brief A view over a sequence of code units. The code units are expected to be of the given encoding and
	/// normalization form.
	///
	/// @tparam _Encoding The encoding to store any input and presented text as.
	/// @tparam _NormalizationForm The normalization form to impose on the stored text's sequences.
	/// @tparam _Range The range type that will be stored within this ztd::text::basic_text_view and examined using the
	/// iterators, following the `_Encoding` type decoding procedure.
	/// @tparam _ErrorHandler The default error handler to use for any and all operations on text. Generally, most
	/// operations will provide room to override this.
	///
	/// @remarks The default type for this is a basic_string_view templated on the code unit type from the encoding.
	/// The error handler is also the default careless error handler, meaning that any lossy conversions will
	/// automatically cause a compile time error.
	//////
	template <typename _Encoding, typename _NormalizationForm = nfkc,
		typename _Range        = ::std::basic_string_view<code_unit_t<_Encoding>>,
		typename _ErrorHandler = default_handler_t>
	class basic_text_view {
	public:
		//////
		/// @brief The type that this view is wrapping.
		using range_type = _Range;
		//////
		/// @brief The encoding type that this view is using to interpret the underlying sequence of code units.
		using encoding_type = _Encoding;
		//////
		/// @brief The encoding type that this view is using to interpret the underlying sequence of code units.
		using state_type = encode_state_t<_Encoding>;
		//////
		/// @brief The normalization form type this view is imposing on top of the encoded sequence.
		using normalization_type = _NormalizationForm;
		//////
		/// @brief The error handling type used by default for any problems in conversions.
		using error_handler_type = _ErrorHandler;

	private:
		template <typename, typename, typename, typename>
		friend class basic_text;

		template <typename _ViewErrorHandler = error_handler_type>
		using _CodePointView = decode_view<encoding_type, range_type, remove_cvref_t<_ViewErrorHandler>, state_type>;

		range_type _M_storage;
		encoding_type _M_encoding;
		state_type _M_state;
		normalization_type _M_normalization;
		error_handler_type _M_error_handler;

	public:
		//////
		/// @brief Returns a view over the code points of this type, decoding "on the fly"/"lazily".
		///
		/// @tparam _ViewErrorHandler The type of the passed-in error handler to use for these operations.
		///
		/// @param[in] __state The state to use for this code point view.
		/// @param[in] __error_handler The error handler to look at the code points for this code point view.
		///
		/// @remarks Moves the provided `__state` in as the "starting point".
		//////
		template <typename _ViewErrorHandler>
		constexpr _CodePointView<_ViewErrorHandler> code_points(
			state_type __state, _ViewErrorHandler&& __error_handler) const noexcept {
			return _CodePointView<_ViewErrorHandler>(this->_M_storage, this->_M_encoding,
				::std::forward<_ViewErrorHandler>(__error_handler), ::std::move(__state));
		}

		//////
		/// @brief Returns a view over the code points of this type, decoding "on the fly"/"lazily".
		///
		/// @param[in] __state The state to use for this code point view.
		///
		/// @remarks Moves the provided `__state` in as the "starting point".
		//////
		constexpr _CodePointView<> code_points(state_type __state) const noexcept {
			return _CodePointView<>(
				this->_M_storage, this->_M_encoding, this->_M_error_handler, ::std::move(__state));
		}

		//////
		/// @brief Returns a view over the code points of this type, decoding "on the fly"/"lazily".
		///
		/// @remarks Copies the stored `state` within the ztd::text::basic_text_view to perform the code point
		/// iteration process.
		//////
		constexpr _CodePointView<> code_points() const noexcept {
			return _CodePointView<>(this->_M_storage, this->_M_encoding, this->_M_error_handler, this->_M_state);
		}

		//////
		/// @brief Access the storage as an r-value reference.
		constexpr range_type&& base() && noexcept {
			return ::std::move(this->_M_storage);
		}

		//////
		/// @brief Access the storage as a const-qualified l-value reference.
		constexpr const range_type& base() const& noexcept {
			return this->_M_storage;
		}

		//////
		/// @brief Access the storage as an l-value reference.
		constexpr range_type& base() & noexcept {
			return this->_M_storage;
		}
	};

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_BASIC_TEXT_VIEW_HPP
