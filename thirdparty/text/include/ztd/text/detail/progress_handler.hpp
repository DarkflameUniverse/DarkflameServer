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

#ifndef ZTD_TEXT_DETAIL_PROGRESS_HANDLER_HPP
#define ZTD_TEXT_DETAIL_PROGRESS_HANDLER_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/code_unit.hpp>
#include <ztd/text/code_point.hpp>
#include <ztd/text/type_traits.hpp>
#include <ztd/text/decode_result.hpp>
#include <ztd/text/encode_result.hpp>

#include <ztd/ranges/algorithm.hpp>
#include <ztd/idk/ebco.hpp>

#include <utility>
#include <array>
#include <cstddef>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	namespace __txt_detail {

		template <typename _AssumeValid, typename _DesiredEncoding>
		class __progress_handler {
		private:
			using _CodePoint = code_point_t<_DesiredEncoding>;
			using _CodeUnit  = code_unit_t<_DesiredEncoding>;

		public:
			using assume_valid = ::std::integral_constant<bool, _AssumeValid::value>;

			constexpr __progress_handler() noexcept
			: _M_code_points(), _M_code_points_size(), _M_code_units(), _M_code_units_size() {
			}

			constexpr ::ztd::span<const _CodePoint> _M_code_points_progress() const noexcept {
				return ::ztd::span<const _CodePoint>(this->_M_code_points.data(), this->_M_code_points_size);
			}

			constexpr ::ztd::span<const _CodeUnit> _M_code_units_progress() const noexcept {
				return ::ztd::span<const _CodeUnit>(this->_M_code_units.data(), this->_M_code_units_size);
			}

			constexpr ::ztd::span<_CodePoint> _M_code_points_progress() noexcept {
				return ::ztd::span<_CodePoint>(this->_M_code_points.data(), this->_M_code_points_size);
			}

			constexpr ::ztd::span<_CodeUnit> _M_code_units_progress() noexcept {
				return ::ztd::span<_CodeUnit>(this->_M_code_units.data(), this->_M_code_units_size);
			}

			constexpr ::std::size_t _M_code_points_progress_size() const noexcept {
				return this->_M_code_points_size;
			}

			constexpr ::std::size_t _M_code_units_progress_size() const noexcept {
				return this->_M_code_units_size;
			}

			template <typename _Encoding, typename _Result, typename _InputProgress, typename _OutputProgress>
			constexpr auto operator()(const _Encoding&, _Result __result, const _InputProgress& __input_progress,
				const _OutputProgress& __output_progress) noexcept {
				if constexpr (is_specialization_of_v<remove_cvref_t<_Result>, decode_result>) {
					this->_M_code_units_size = ranges::ranges_adl::adl_size(__input_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__input_progress),
						this->_M_code_units_size, this->_M_code_units.data());
					this->_M_code_points_size = ranges::ranges_adl::adl_size(__output_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__output_progress),
						this->_M_code_points_size, this->_M_code_points.data());
				}
				else {
					this->_M_code_points_size = ranges::ranges_adl::adl_size(__input_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__input_progress),
						this->_M_code_points_size, this->_M_code_points.data());
					this->_M_code_units_size = ranges::ranges_adl::adl_size(__output_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__output_progress),
						this->_M_code_units_size, this->_M_code_units.data());
				}
				return __result;
			}

			constexpr void clear() noexcept {
				this->_M_code_points_size = 0;
				this->_M_code_units_size  = 0;
			}

		private:
			::std::array<_CodePoint, max_code_points_v<_DesiredEncoding>> _M_code_points;
			::std::size_t _M_code_points_size;
			::std::array<_CodeUnit, max_code_units_v<_DesiredEncoding>> _M_code_units;
			::std::size_t _M_code_units_size;
		};

		template <typename _AssumeValid, typename _ErrorHandler, typename _Encoding>
		class __forwarding_progress_handler : private ebco<_Encoding&, 0>, private ebco<_ErrorHandler&, 1> {
		private:
			using _CodePoint             = code_point_t<remove_cvref_t<_Encoding>>;
			using _CodeUnit              = code_unit_t<remove_cvref_t<_Encoding>>;
			using __encoding_base_t      = ebco<_Encoding&, 0>;
			using __error_handler_base_t = ebco<_ErrorHandler&, 1>;

		public:
			using assume_valid = ::std::integral_constant<bool, _AssumeValid::value>;

			constexpr __forwarding_progress_handler(_Encoding& __encoding, _ErrorHandler& __error_handler) noexcept
			: __encoding_base_t(__encoding)
			, __error_handler_base_t(__error_handler)
			, _M_code_points()
			, _M_code_points_size()
			, _M_code_units()
			, _M_code_units_size() {
			}

			constexpr ::ztd::span<const _CodePoint> _M_code_points_progress() const noexcept {
				return ::ztd::span<const _CodePoint>(this->_M_code_points.data(), this->_M_code_points_size);
			}

			constexpr ::ztd::span<const _CodeUnit> _M_code_units_progress() const noexcept {
				return ::ztd::span<const _CodeUnit>(this->_M_code_units.data(), this->_M_code_units_size);
			}

			constexpr ::ztd::span<_CodePoint> _M_code_points_progress() noexcept {
				return ::ztd::span<_CodePoint>(this->_M_code_points.data(), this->_M_code_points_size);
			}

			constexpr ::ztd::span<_CodeUnit> _M_code_units_progress() noexcept {
				return ::ztd::span<_CodeUnit>(this->_M_code_units.data(), this->_M_code_units_size);
			}

			constexpr ::std::size_t _M_code_points_progress_size() const noexcept {
				return this->_M_code_points_size;
			}

			constexpr ::std::size_t _M_code_units_progress_size() const noexcept {
				return this->_M_code_units_size;
			}

			template <typename _SomeEncoding, typename _Result, typename _InputProgress, typename _OutputProgress>
			constexpr auto operator()(const _SomeEncoding&, _Result&& __result, _InputProgress&& __input_progress,
				_OutputProgress&& __output_progress) & // clang-format hack
				noexcept(noexcept(this->__error_handler_base_t::get_value()(this->__encoding_base_t::get_value(),
				     ::std::forward<_Result>(__result), ::std::forward<_InputProgress>(__input_progress),
				     ::std::forward<_OutputProgress>(__output_progress)))) {
				__forwarding_progress_handler& __non_const_self = *this;
				if constexpr (is_specialization_of_v<remove_cvref_t<_Result>, decode_result>) {
					__non_const_self._M_code_units_size = ranges::ranges_adl::adl_size(__input_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__input_progress),
						__non_const_self._M_code_units_size, __non_const_self._M_code_units.data());
					__non_const_self._M_code_points_size = ranges::ranges_adl::adl_size(__output_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__output_progress),
						__non_const_self._M_code_points_size, __non_const_self._M_code_points.data());
				}
				else {
					__non_const_self._M_code_points_size = ranges::ranges_adl::adl_size(__input_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__input_progress),
						__non_const_self._M_code_points_size, __non_const_self._M_code_points.data());
					__non_const_self._M_code_units_size = ranges::ranges_adl::adl_size(__output_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__output_progress),
						__non_const_self._M_code_units_size, __non_const_self._M_code_units.data());
				}
				return this->__error_handler_base_t::get_value()(this->__encoding_base_t::get_value(),
					::std::forward<_Result>(__result), ::std::forward<_InputProgress>(__input_progress),
					::std::forward<_OutputProgress>(__output_progress));
			}

			template <typename _SomeEncoding, typename _Result, typename _InputProgress, typename _OutputProgress>
			constexpr auto operator()(const _SomeEncoding&, _Result&& __result, _InputProgress&& __input_progress,
				_OutputProgress&& __output_progress) const& // clang-format hack
				noexcept(noexcept(this->__error_handler_base_t::get_value()(this->__encoding_base_t::get_value(),
				     ::std::forward<_Result>(__result), ::std::forward<_InputProgress>(__input_progress),
				     ::std::forward<_OutputProgress>(__output_progress)))) {
				__forwarding_progress_handler& __non_const_self = const_cast<__forwarding_progress_handler&>(*this);
				if constexpr (is_specialization_of_v<remove_cvref_t<_Result>, decode_result>) {
					__non_const_self._M_code_units_size = ranges::ranges_adl::adl_size(__input_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__input_progress),
						__non_const_self._M_code_units_size, __non_const_self._M_code_units.data());
					__non_const_self._M_code_points_size = ranges::ranges_adl::adl_size(__output_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__output_progress),
						__non_const_self._M_code_points_size, __non_const_self._M_code_points.data());
				}
				else {
					__non_const_self._M_code_points_size = ranges::ranges_adl::adl_size(__input_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__input_progress),
						__non_const_self._M_code_points_size, __non_const_self._M_code_points.data());
					__non_const_self._M_code_units_size = ranges::ranges_adl::adl_size(__output_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__output_progress),
						__non_const_self._M_code_units_size, __non_const_self._M_code_units.data());
				}
				return this->__error_handler_base_t::get_value()(this->__encoding_base_t::get_value(),
					::std::forward<_Result>(__result), ::std::forward<_InputProgress>(__input_progress),
					::std::forward<_OutputProgress>(__output_progress));
			}

			template <typename _SomeEncoding, typename _Result, typename _InputProgress, typename _OutputProgress>
			constexpr auto operator()(const _SomeEncoding&, _Result&& __result, _InputProgress&& __input_progress,
				_OutputProgress&& __output_progress) && // clang-format hack
				noexcept(noexcept(this->__error_handler_base_t::get_value()(this->__encoding_base_t::get_value(),
				     ::std::forward<_Result>(__result), ::std::forward<_InputProgress>(__input_progress),
				     ::std::forward<_OutputProgress>(__output_progress)))) {
				__forwarding_progress_handler& __non_const_self = *this;
				if constexpr (is_specialization_of_v<remove_cvref_t<_Result>, decode_result>) {
					__non_const_self._M_code_units_size = ranges::ranges_adl::adl_size(__input_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__input_progress),
						__non_const_self._M_code_units_size, __non_const_self._M_code_units.data());
					__non_const_self._M_code_points_size = ranges::ranges_adl::adl_size(__output_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__output_progress),
						__non_const_self._M_code_points_size, __non_const_self._M_code_points.data());
				}
				else {
					__non_const_self._M_code_points_size = ranges::ranges_adl::adl_size(__input_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__input_progress),
						__non_const_self._M_code_points_size, __non_const_self._M_code_points.data());
					__non_const_self._M_code_units_size = ranges::ranges_adl::adl_size(__output_progress);
					ranges::__rng_detail::__copy_n_unsafe(ranges::ranges_adl::adl_cbegin(__output_progress),
						__non_const_self._M_code_units_size, __non_const_self._M_code_units.data());
				}
				return this->__error_handler_base_t::get_value()(this->__encoding_base_t::get_value(),
					::std::forward<_Result>(__result), ::std::forward<_InputProgress>(__input_progress),
					::std::forward<_OutputProgress>(__output_progress));
			}

			constexpr void clear() noexcept {
				this->_M_code_points_size = 0;
				this->_M_code_units_size  = 0;
			}

		private:
			mutable ::std::array<_CodePoint, max_code_points_v<remove_cvref_t<_Encoding>>> _M_code_points;
			mutable ::std::size_t _M_code_points_size;
			mutable ::std::array<_CodeUnit, max_code_units_v<remove_cvref_t<_Encoding>>> _M_code_units;
			mutable ::std::size_t _M_code_units_size;
		};
	} // namespace __txt_detail

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text


#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_DETAIL_PROGRESS_HANDLER_HPP
