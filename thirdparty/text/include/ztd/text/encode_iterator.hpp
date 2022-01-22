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

#ifndef ZTD_TEXT_ENCODE_ITERATOR_HPP
#define ZTD_TEXT_ENCODE_ITERATOR_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/error_handler.hpp>
#include <ztd/text/state.hpp>

#include <ztd/text/detail/encoding_iterator.hpp>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	//////
	/// @brief The encode sentinel to use as the `end` value for a ztd::text::encode_iterator.
	using encode_sentinel_t = __txt_detail::__encoding_sentinel_t;

	//////
	/// @brief An iterator over an iterator of code points, presenting the code points as code units. Uses the @p
	/// _Encoding specified to do so.
	///
	/// @tparam _Encoding The encoding to read the underlying range of code points as.
	/// @tparam _Range The range of input that will be fed into the _FromEncoding's decode operation.
	/// @tparam _ErrorHandler The error handler for any encode-step failures.
	/// @tparam _State The state type to use for the encode operations to intermediate code points.
	///
	/// @remarks This type produces proxies as their reference type, and are only readable, not writable iterators. The
	/// iterator presents code units one at a time, regardless of how many code units are output by one decode
	/// operation. This means if, for example, one (1) UTF-32 code point becomes four (4) UTF-8 code units, it will
	/// present each code unit one at a time. If you are looking to explicitly know what a single encode operation maps
	/// into as far as number of code points to code units (and vice-versa), you will have to use lower-level
	/// interfaces.
	//////
	template <typename _Encoding, typename _Range, typename _ErrorHandler = default_handler_t,
		typename _State = encode_state_t<_Encoding>>
	class encode_iterator
	: public __txt_detail::__encoding_iterator<__txt_detail::__transaction::__encode,
		  encode_iterator<_Encoding, _Range, _ErrorHandler, _State>, _Encoding, _Range, _ErrorHandler, _State> {
	private:
		using __iterator_base_it = __txt_detail::__encoding_iterator<__txt_detail::__transaction::__encode,
			encode_iterator<_Encoding, _Range, _ErrorHandler, _State>, _Encoding, _Range, _ErrorHandler, _State>;

	public:
		//////
		/// @brief The underlying range type.
		using range_type = typename __iterator_base_it::range_type;
		//////
		/// @brief The base iterator type.
		using iterator = typename __iterator_base_it::iterator;
		//////
		/// @brief The encoding type used for transformations.
		using encoding_type = typename __iterator_base_it::encoding_type;
		//////
		/// @brief The error handler when an encode operation fails.
		using error_handler_type = typename __iterator_base_it::error_handler_type;
		//////
		/// @brief The state type used for encode operations.
		using state_type = typename __iterator_base_it::state_type;
		//////
		/// @brief The strength of the iterator category, as defined in relation to the base.
		using iterator_category = typename __iterator_base_it::iterator_category;
		//////
		/// @brief The strength of the iterator concept, as defined in relation to the base.
		using iterator_concept = typename __iterator_base_it::iterator_concept;
		//////
		/// @brief The object type that gets output on every dereference.
		using value_type = typename __iterator_base_it::value_type;
		//////
		/// @brief A pointer type to the value_type.
		using pointer = typename __iterator_base_it::pointer;
		//////
		/// @brief The value returned from derefencing the iterator.
		///
		/// @remarks This is a proxy iterator, so the `reference` is a non-reference `value_type.`
		//////
		using reference = typename __iterator_base_it::value_type;
		//////
		/// @brief The type returned when two of these pointers are subtracted from one another.
		///
		/// @remarks It's not a very useful type...
		//////
		using difference_type = typename __iterator_base_it::difference_type;

		//////
		/// @brief Default constructor. Defaulted.
		constexpr encode_iterator() = default;

		//////
		/// @brief Copy constructor. Defaulted.
		constexpr encode_iterator(const encode_iterator&) = default;
		//////
		/// @brief Move constructor. Defaulted.
		constexpr encode_iterator(encode_iterator&&) = default;

		//////
		/// @brief Constructs a ztd::text::decode_iterator from the explicitly given `__range`.
		///
		/// @param[in] __range The range value that will be read from.
		///
		/// @remarks Each argument is moved/forwarded in.
		//////
		template <typename _ArgRange,
			::std::enable_if_t<!::std::is_same_v<remove_cvref_t<_ArgRange>, encode_iterator>>* = nullptr>
		constexpr encode_iterator(_ArgRange&& __range) noexcept(
			::std::is_nothrow_constructible_v<__iterator_base_it, range_type>)
		: __iterator_base_it(::std::forward<_ArgRange>(__range)) {
		}

		//////
		/// @brief Constructs a ztd::text::encode_iterator from the explicitly given `__range`, and `__encoding`.
		///
		/// @param[in] __range The range value that will be read from.
		/// @param[in] __encoding The encoding object to use.
		///
		/// @remarks Each argument is moved in.
		//////
		constexpr encode_iterator(range_type __range, encoding_type __encoding) noexcept(
			::std::is_nothrow_constructible_v<__iterator_base_it, range_type, encoding_type>)
		: __iterator_base_it(::std::move(__range), ::std::move(__encoding), error_handler_type {}) {
		}

		//////
		/// @brief Constructs a ztd::text::encode_iterator from the explicitly given `__range`, and @p
		/// __error_handler.
		///
		/// @param[in] __range The range value that will be read from.
		/// @param[in] __error_handler The error handler to use for reporting errors.
		///
		/// @remarks Each argument is moved in.
		//////
		constexpr encode_iterator(range_type __range, error_handler_type __error_handler) noexcept(
			::std::is_nothrow_constructible_v<__iterator_base_it, range_type, error_handler_type>)
		: __iterator_base_it(::std::move(__range), encoding_type {}, ::std::move(__error_handler)) {
		}

		//////
		/// @brief Constructs a ztd::text::encode_iterator from the explicitly given `__range`, `__encoding`, and
		/// `__error_handler`.
		///
		/// @param[in] __range The range value that will be read from.
		/// @param[in] __encoding The encoding object to use.
		/// @param[in] __error_handler The error handler to use for reporting errors.
		///
		/// @remarks Each argument is moved in.
		//////
		constexpr encode_iterator(range_type __range, encoding_type __encoding,
			error_handler_type __error_handler) noexcept(::std::is_nothrow_constructible_v<__iterator_base_it,
			range_type, encoding_type, error_handler_type>)
		: __iterator_base_it(::std::move(__range), ::std::move(__encoding), ::std::move(__error_handler)) {
		}

		//////
		/// @brief Constructs a ztd::text::encode_iterator from the explicitly given `__range`, `__encoding`, @p
		/// __error_handler and `__state`.
		///
		/// @param[in] __range The range value that will be read from.
		/// @param[in] __encoding The encoding object to use.
		/// @param[in] __error_handler The error handler to use for reporting errors.
		/// @param[in] __state The current state.
		///
		/// @remarks Each argument is moved in.
		//////
		constexpr encode_iterator(range_type __range, encoding_type __encoding, error_handler_type __error_handler,
			state_type __state) noexcept(::std::is_nothrow_constructible_v<__iterator_base_it, range_type,
			encoding_type, error_handler_type, state_type>)
		: __iterator_base_it(
			::std::move(__range), ::std::move(__encoding), ::std::move(__error_handler), ::std::move(__state)) {
		}

		//////
		/// @brief Copy assignment operator. Defaulted.
		constexpr encode_iterator& operator=(const encode_iterator&) = default;
		//////
		/// @brief Move assignment operator. Defaulted.
		constexpr encode_iterator& operator=(encode_iterator&&) = default;
	};

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_ENCODE_ITERATOR_HPP
