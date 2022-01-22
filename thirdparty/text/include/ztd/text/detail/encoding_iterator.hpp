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

#ifndef ZTD_TEXT_DETAIL_ENCODING_ITERATOR_HPP
#define ZTD_TEXT_DETAIL_ENCODING_ITERATOR_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/transcode_result.hpp>
#include <ztd/text/error_handler.hpp>
#include <ztd/text/state.hpp>
#include <ztd/text/is_ignorable_error_handler.hpp>
#include <ztd/text/type_traits.hpp>
#include <ztd/text/detail/encoding_iterator_storage.hpp>
#include <ztd/text/detail/encoding_range.hpp>
#include <ztd/text/detail/transcode_one.hpp>

#include <ztd/idk/ebco.hpp>
#include <ztd/ranges/unbounded.hpp>
#include <ztd/ranges/subrange.hpp>
#include <ztd/ranges/default_sentinel.hpp>
#include <ztd/ranges/adl.hpp>
#include <ztd/idk/span.hpp>
#include <ztd/ranges/blackhole_iterator.hpp>

#include <array>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_
	namespace __txt_detail {

		template <typename _Encoding, typename _Input, typename _ErrorHandler>
		inline constexpr bool __is_encoding_with_error_handler_never_returns_error_v = false;

		using __encoding_sentinel_t = ranges::default_sentinel_t;

		template <__transaction _EncodeOrDecode, typename _Derived, typename _Encoding, typename _Range,
			typename _ErrorHandler, typename _State>
		class __encoding_iterator
		: private ebco<remove_cvref_t<_Encoding>, 0>,
		  private ebco<::std::remove_reference_t<_ErrorHandler>, 1>,
		  private __state_storage<remove_cvref_t<unwrap_t<_Encoding>>, remove_cvref_t<_State>>,
		  private __cursor_cache<(_EncodeOrDecode == __transaction::__decode
			                              ? max_code_points_v<remove_cvref_t<unwrap_t<_Encoding>>>
			                              : max_code_units_v<remove_cvref_t<unwrap_t<_Encoding>>>),
			  ranges::is_range_input_or_output_range_v<remove_cvref_t<unwrap_t<_Range>>>>,
		  private __error_cache<(_EncodeOrDecode == __transaction::__decode
			       ? decode_error_handler_always_returns_ok_v<remove_cvref_t<unwrap_t<_Encoding>>,
			            remove_cvref_t<unwrap_t<_ErrorHandler>>>
			       : encode_error_handler_always_returns_ok_v<remove_cvref_t<unwrap_t<_Encoding>>,
			            remove_cvref_t<unwrap_t<_ErrorHandler>>>)>,
		  private ebco<_Range, 2> {
		private:
			using _URange                                    = remove_cvref_t<unwrap_t<_Range>>;
			using _UEncoding                                 = remove_cvref_t<unwrap_t<_Encoding>>;
			using _UErrorHandler                             = remove_cvref_t<unwrap_t<_ErrorHandler>>;
			using _UState                                    = remove_cvref_t<unwrap_t<_State>>;
			using _BaseIterator                              = ranges::range_iterator_t<_URange>;
			inline static constexpr ::std::size_t _MaxValues = (_EncodeOrDecode == __transaction::__decode
				     ? max_code_points_v<remove_cvref_t<unwrap_t<_Encoding>>>
				     : max_code_units_v<remove_cvref_t<unwrap_t<_Encoding>>>);
			inline static constexpr bool _IsSingleValueType  = _MaxValues == 1;
			inline static constexpr bool _IsInputOrOutput    = ranges::is_range_input_or_output_range_v<_URange>;
			inline static constexpr bool _IsCursorless       = _IsSingleValueType && !_IsInputOrOutput;
			inline static constexpr bool _IsErrorless        = _EncodeOrDecode == __transaction::__decode
				       ? decode_error_handler_always_returns_ok_v<_UEncoding, _UErrorHandler>
				       : encode_error_handler_always_returns_ok_v<_UEncoding, _UErrorHandler>;
			using __base_cursor_cache_t                      = __cursor_cache<_MaxValues, _IsInputOrOutput>;
			using __base_cursor_cache_size_t                 = typename __base_cursor_cache_t::_SizeType;
			using __base_error_cache_t                       = __error_cache<_IsErrorless>;
			using __base_encoding_t                          = ebco<remove_cvref_t<_Encoding>, 0>;
			using __base_error_handler_t                     = ebco<remove_cvref_t<_ErrorHandler>, 1>;
			using __base_range_t                             = ebco<_Range, 2>;
			using __base_state_t = __state_storage<remove_cvref_t<_Encoding>, remove_cvref_t<_State>>;

			inline static constexpr bool _IsBackwards
				= is_detected_v<__detect_object_encode_one_backwards, _UEncoding, _URange, _UErrorHandler, _UState>;

		public:
			//////
			/// @brief The underlying range type.

			using range_type = _Range;
			//////
			/// @brief The base iterator type.

			using iterator = _BaseIterator;
			//////
			/// @brief The encoding type used for transformations.

			using encoding_type = _Encoding;
			//////
			/// @brief The error handler when an encode operation fails.

			using error_handler_type = _ErrorHandler;
			//////
			/// @brief The state type used for encode operations.

			using state_type = remove_cvref_t<_State>;
			//////
			/// @brief The strength of the iterator category, as defined in relation to the base.

			using iterator_category = ::std::conditional_t<
				ranges::is_iterator_concept_or_better_v<::std::bidirectional_iterator_tag, _BaseIterator>,
				::std::conditional_t<_IsBackwards, ::std::bidirectional_iterator_tag, ::std::forward_iterator_tag>,
				ranges::iterator_category_t<_BaseIterator>>;
			//////
			/// @brief The strength of the iterator concept, as defined in relation to the base.

			using iterator_concept = ::std::conditional_t<
				ranges::is_iterator_concept_or_better_v<::std::bidirectional_iterator_tag, _BaseIterator>,
				::std::conditional_t<_IsBackwards, ::std::bidirectional_iterator_tag, ::std::forward_iterator_tag>,
				ranges::iterator_concept_t<_BaseIterator>>;
			//////
			/// @brief The object type that gets output on every dereference.

			using value_type = ::std::conditional_t<_EncodeOrDecode == __transaction::__encode,
				code_unit_t<_Encoding>, code_point_t<_Encoding>>;
			//////
			/// @brief A pointer type to the value_type.

			using pointer = value_type*;
			//////
			/// @brief The value returned from derefencing the iterator.
			///
			/// @remarks This is a proxy iterator, so the `reference` is a non-reference `value_type.`
			//////
			using reference = value_type;
			//////
			/// @brief The type returned when two of these pointers are subtracted from one another.
			///
			/// @remarks It's not a very useful type...
			//////
			using difference_type = ranges::iterator_difference_type_t<_BaseIterator>;

			constexpr __encoding_iterator() noexcept(::std::is_nothrow_default_constructible_v<
				encoding_type>&& ::std::is_nothrow_default_constructible_v<error_handler_type>&& ::std::
				     is_nothrow_default_constructible_v<range_type>&& noexcept(
				          __base_state_t(::std::declval<encoding_type&>())))
			: __base_encoding_t()
			, __base_error_handler_t()
			, __base_state_t(this->encoding())
			, __base_cursor_cache_t()
			, __base_error_cache_t()
			, __base_range_t()
			, _M_cache() {
			}

			constexpr __encoding_iterator(const __encoding_iterator&) = default;
			constexpr __encoding_iterator(__encoding_iterator&&)      = default;

			template <typename _ArgRange,
				::std::enable_if_t<!::std::is_same_v<remove_cvref_t<_ArgRange>, __encoding_iterator>>* = nullptr>
			constexpr __encoding_iterator(_ArgRange&& __range) noexcept(
				noexcept(__encoding_iterator(::std::declval<range_type>(), ::std::declval<encoding_type>(),
				     ::std::declval<error_handler_type>())))
			: __encoding_iterator(::std::forward<_ArgRange>(__range), encoding_type {}, error_handler_type {}) {
			}

			constexpr __encoding_iterator(range_type __range, encoding_type __encoding) noexcept(
				noexcept(__encoding_iterator(::std::declval<range_type>(), ::std::declval<encoding_type>(),
				     ::std::declval<error_handler_type>())))
			: __encoding_iterator(::std::move(__range), ::std::move(__encoding), error_handler_type {}) {
			}

			constexpr __encoding_iterator(range_type __range, error_handler_type __error_handler) noexcept(
				noexcept(__encoding_iterator(::std::declval<range_type>(), ::std::declval<encoding_type>(),
				     ::std::declval<error_handler_type>())))
			: __encoding_iterator(::std::move(__range), encoding_type {}, ::std::move(__error_handler)) {
			}

			constexpr __encoding_iterator(range_type __range, encoding_type __encoding,
				error_handler_type
				     __error_handler) noexcept(::std::is_nothrow_move_constructible_v<encoding_type>&& ::std::
				     is_nothrow_move_constructible_v<error_handler_type>&& ::std::is_nothrow_move_constructible_v<
				          range_type>&& noexcept(__base_state_t(::std::declval<encoding_type&>())))
			: __base_encoding_t(::std::move(__encoding))
			, __base_error_handler_t(::std::move(__error_handler))
			, __base_state_t(this->encoding())
			, __base_cursor_cache_t()
			, __base_error_cache_t()
			, __base_range_t(::std::move(__range))
			, _M_cache() {
				this->_M_read_one();
			}

			constexpr __encoding_iterator(range_type __range, encoding_type __encoding,
				error_handler_type __error_handler,
				state_type __state) noexcept(::std::is_nothrow_move_constructible_v<encoding_type>&& ::std::
				     is_nothrow_move_constructible_v<error_handler_type>&& ::std::is_nothrow_move_constructible_v<
				          range_type>&& noexcept(__base_state_t(::std::declval<encoding_type&>(),
				          ::std::declval<state_type>())))
			: __base_encoding_t(::std::move(__encoding))
			, __base_error_handler_t(::std::move(__error_handler))
			, __base_state_t(this->encoding(), ::std::move(__state))
			, __base_cursor_cache_t()
			, __base_error_cache_t()
			, __base_range_t(::std::move(__range))
			, _M_cache() {
				this->_M_read_one();
			}

			// assignment
			constexpr __encoding_iterator& operator=(const __encoding_iterator&) = default;
			constexpr __encoding_iterator& operator=(__encoding_iterator&&) = default;

			//////
			/// @brief The encoding object.
			///
			/// @returns A const l-value reference to the encoding object used to construct this iterator.
			//////
			constexpr const encoding_type& encoding() const noexcept {
				return this->__base_encoding_t::get_value();
			}

			//////
			/// @brief The encoding object.
			///
			/// @returns An l-value reference to the encoding object used to construct this iterator.
			//////
			constexpr encoding_type& encoding() noexcept {
				return this->__base_encoding_t::get_value();
			}

			//////
			/// @brief The state object.
			///
			/// @returns A const l-value reference to the state object used to construct this iterator.
			//////
			constexpr const state_type& state() const noexcept {
				return this->__base_state_t::_M_get_state();
			}

			//////
			/// @brief The state object.
			///
			/// @returns An l-value reference to the state object used to construct this iterator.
			//////
			constexpr state_type& state() noexcept {
				return this->__base_state_t::_M_get_state();
			}

			//////
			/// @brief The error handler object.
			///
			/// @returns A const l-value reference to the error handler used to construct this iterator.
			//////
			constexpr const error_handler_type& handler() const& noexcept {
				return this->__base_error_handler_t::get_value();
			}

			//////
			/// @brief The error handler object.
			///
			/// @returns An l-value reference to the error handler used to construct this iterator.
			//////
			constexpr error_handler_type& handler() & noexcept {
				return this->__base_error_handler_t::get_value();
			}

			//////
			/// @brief The error handler object.
			///
			/// @returns An l-value reference to the error handler used to construct this iterator.
			//////
			constexpr error_handler_type&& handler() && noexcept {
				return ::std::move(this->__base_error_handler_t::get_value());
			}

			//////
			/// @brief The input range used to construct this object.
			///
			/// @returns A const l-value reference to the input range used to construct this iterator.
			//////
			constexpr range_type range() const& noexcept(::std::is_nothrow_move_constructible_v<range_type>) {
				return this->__base_range_t::get_value();
			}

			//////
			/// @brief The input range used to construct this object.

			constexpr range_type range() & noexcept(::std::is_copy_constructible_v<range_type>
				     ? ::std::is_nothrow_copy_constructible_v<range_type>
				     : ::std::is_nothrow_move_constructible_v<range_type>) {
				if constexpr (::std::is_copy_constructible_v<range_type>) {
					return this->__base_range_t::get_value();
				}
				else {
					return ::std::move(this->__base_range_t::get_value());
				}
			}

			//////
			/// @brief The input range used to construct this object.
			///
			/// @returns An r-value reference to the input range used to construct this iterator.
			//////
			constexpr range_type range() && noexcept(::std::is_nothrow_move_constructible_v<range_type>) {
				return ::std::move(this->__base_range_t::get_value());
			}

			//////
			/// @brief Returns whether the last read operation had an encoding error or not.
			///
			/// @returns The ztd::text::encoding_error that occurred. This can be ztd::text::encoding_error::ok for
			/// an operation that went just fine.
			///
			/// @remarks If the error handler is identified as an error handler that, if given a suitably sized
			/// buffer, will never return an error. This is the case with specific encoding operations with
			/// ztd::text::replacement_handler_t, or ztd::text::throw_handler_t.
			//////
			constexpr encoding_error error_code() const noexcept {
				if constexpr (_IsErrorless) {
					return encoding_error::ok;
				}
				else {
					return this->__base_error_cache_t::_M_error_code;
				}
			}

			//////
			/// @brief Increment a copy of the iterator.
			///
			/// @returns A copy to the incremented iterator.
			//////
			constexpr _Derived operator++(int) {
				_Derived __copy = this->_M_derived();
				++__copy;
				return __copy;
			}

			//////
			/// @brief Increment the iterator.
			///
			/// @returns A reference to *this, after incrementing the iterator.
			//////
			constexpr _Derived& operator++() {
				if constexpr (_IsSingleValueType) {
					this->_M_read_one();
				}
				else {
					++this->__base_cursor_cache_t::_M_position;
					if (this->__base_cursor_cache_t::_M_position == this->__base_cursor_cache_t::_M_size) {
						this->_M_read_one();
					}
				}
				return this->_M_derived();
			}

			//////
			/// @brief Dereference the iterator.
			///
			/// @remarks This is a proxy iterator, and therefore only returns a const value_type& object and not a
			/// non-const reference object. This is deliberately to work around the standard not allowing stand-alone
			/// proxy iterators that do not return reference types. Encoding iterators are only readable, not
			/// writable.
			//////
			constexpr reference operator*() const noexcept {
				if constexpr (_IsSingleValueType) {
					return this->_M_cache[0];
				}
				else {
					return this->_M_cache[this->__base_cursor_cache_t::_M_position];
				}
			}

			// observers: comparison

			//////
			/// @brief Compares whether or not this iterator has truly reached the end.

			friend constexpr bool operator==(const _Derived& __it, const __encoding_sentinel_t&) {
				if constexpr (_IsCursorless || (_IsInputOrOutput && _IsSingleValueType)) {
					return __it._M_base_is_empty()
						&& static_cast<__base_cursor_cache_size_t>(__txt_detail::_CursorlessSizeSentinel)
						== __it.__base_cursor_cache_t::_M_size;
				}
				else {
					return __it._M_base_is_empty()
						&& __it.__base_cursor_cache_t::_M_position == __it.__base_cursor_cache_t::_M_size;
				}
			}

			//////
			/// @brief Compares whether or not this iterator has truly reached the end.

			friend constexpr bool operator==(const __encoding_sentinel_t& __sen, const _Derived& __it) {
				return __it == __sen;
			}

			//////
			/// @brief Compares whether or not this iterator has truly reached the end.

			friend constexpr bool operator!=(const _Derived& __it, const __encoding_sentinel_t&) {
				if constexpr (_IsCursorless || (_IsInputOrOutput && _IsSingleValueType)) {
					return !__it._M_base_is_empty()
						|| static_cast<__base_cursor_cache_size_t>(__txt_detail::_CursorlessSizeSentinel)
						!= __it.__base_cursor_cache_t::_M_size;
				}
				else {
					return !__it._M_base_is_empty()
						|| __it.__base_cursor_cache_t::_M_position != __it.__base_cursor_cache_t::_M_size;
				}
			}

			//////
			/// @brief Compares whether or not this iterator has truly reached the end.

			friend constexpr bool operator!=(const __encoding_sentinel_t& __sen, const _Derived& __it) noexcept {
				return __it != __sen;
			}

		private:
			constexpr bool _M_base_is_empty() const noexcept {
				if constexpr (is_detected_v<ranges::detect_adl_empty, _Range>) {
					return ranges::ranges_adl::adl_empty(this->__base_range_t::get_value());
				}
				else {
					return ranges::ranges_adl::adl_begin(this->__base_range_t::get_value())
						== ranges::ranges_adl::adl_end(this->__base_range_t::get_value());
				}
			}

			constexpr void _M_read_one() {
				if (this->_M_base_is_empty()) {
					if constexpr (_IsCursorless || (_IsSingleValueType && _IsInputOrOutput)) {
						this->__base_cursor_cache_t::_M_size
							= static_cast<__base_cursor_cache_size_t>(_CursorlessSizeSentinel);
					}
					else {
						this->__base_cursor_cache_t::_M_size
							= static_cast<__base_cursor_cache_size_t>(this->_M_cache.size());
						this->__base_cursor_cache_t::_M_position
							= static_cast<__base_cursor_cache_size_t>(this->_M_cache.size());
					}
					return;
				}
				auto& __this_input_range = this->_M_range();
				auto __this_cache_begin  = this->_M_cache.data();
				[[maybe_unused]] decltype(__this_cache_begin) __this_cache_end {};
				::ztd::span<value_type, _MaxValues> __cache_view(this->_M_cache);
				if constexpr (_IsInputOrOutput) {
					auto __result = __basic_encode_or_decode_one<__consume::__no, _EncodeOrDecode>(
						::std::move(__this_input_range), this->encoding(), __cache_view, this->handler(),
						this->state());
					__this_cache_end = idk_adl::adl_to_address(ranges::ranges_adl::adl_begin(__result.output));
					if constexpr (!_IsErrorless) {
						this->__base_error_cache_t::_M_error_code = __result.error_code;
					}
					this->__base_range_t::get_value() = ::std::move(__result.input);
				}
				else {
					auto __result = __basic_encode_or_decode_one<__consume::__no, _EncodeOrDecode>(
						::std::move(__this_input_range), this->encoding(), __cache_view, this->handler(),
						this->state());
					__this_cache_end = idk_adl::adl_to_address(ranges::ranges_adl::adl_begin(__result.output));
					if constexpr (!_IsErrorless) {
						this->__base_error_cache_t::_M_error_code = __result.error_code;
					}
					this->__base_range_t::get_value() = ::std::move(__result.input);
				}
				if constexpr (!_IsSingleValueType) {
					__base_cursor_cache_size_t __data_size
						= static_cast<__base_cursor_cache_size_t>(__this_cache_end - __this_cache_begin);
					ZTD_TEXT_ASSERT_MESSAGE_I_("size of produced value can never be bigger thanthe cache",
						static_cast<::std::size_t>(__data_size) <= this->_M_cache.size());
					this->__base_cursor_cache_t::_M_position = static_cast<__base_cursor_cache_size_t>(0);
					this->__base_cursor_cache_t::_M_size     = __data_size;
				}
			}

			constexpr _Derived& _M_derived() noexcept {
				return static_cast<_Derived&>(*this);
			}

			constexpr const _Derived& _M_derived() const noexcept {
				return static_cast<const _Derived&>(*this);
			}

			constexpr _URange& _M_range() noexcept {
				return this->__base_range_t::get_value();
			}

			constexpr const _URange& _M_range() const noexcept {
				return this->__base_range_t::get_value();
			}

			::std::array<value_type, _MaxValues> _M_cache;
		};

	} // namespace __txt_detail
	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_DETAIL_ENCODING_ITERATOR_HPP
