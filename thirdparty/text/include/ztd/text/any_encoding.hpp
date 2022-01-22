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

#ifndef ZTD_TEXT_ANY_ENCODING_HPP
#define ZTD_TEXT_ANY_ENCODING_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/encoding_scheme.hpp>
#include <ztd/text/encode_result.hpp>
#include <ztd/text/decode_result.hpp>
#include <ztd/text/validate_result.hpp>
#include <ztd/text/count_result.hpp>
#include <ztd/text/state.hpp>
#include <ztd/text/is_code_points_replaceable.hpp>
#include <ztd/text/is_code_units_replaceable.hpp>
#include <ztd/text/is_unicode_encoding.hpp>
#include <ztd/text/code_point.hpp>
#include <ztd/text/state.hpp>
#include <ztd/text/default_encoding.hpp>
#include <ztd/text/encode.hpp>
#include <ztd/text/decode.hpp>
#include <ztd/text/validate_decodable_as.hpp>
#include <ztd/text/validate_encodable_as.hpp>
#include <ztd/text/count_as_encoded.hpp>
#include <ztd/text/count_as_decoded.hpp>
#include <ztd/text/text_tag.hpp>
#include <ztd/text/type_traits.hpp>
#include <ztd/text/detail/transcode_one.hpp>

#include <ztd/idk/ebco.hpp>
#include <ztd/ranges/range.hpp>
#include <ztd/ranges/adl.hpp>
#include <ztd/idk/span.hpp>

#include <cstdint>
#include <cstddef>
#include <utility>
#include <functional>
#include <memory>
#include <cassert>
#include <optional>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {

	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	namespace __txt_detail {
		inline constexpr ::std::size_t __default_max_code_points_any_encoding = 32;
		inline constexpr ::std::size_t __default_max_code_units_any_encoding  = 128;
	} // namespace __txt_detail

	//////
	/// @addtogroup ztd_text_encodings Encodings
	/// @{
	//////

	//////
	/// @brief An encoding class which has the given encode output and input, as well as the decode input and output
	/// ranges, provided as fixed types alongside the maximum number of code units put in and pushed out.
	///
	/// @tparam _EncodeCodeUnits The output of `encode_one` and related operations.
	/// @tparam _EncodeCodePoints The input of `encode_one` and related operations.
	/// @tparam _DecodeCodeUnits The input of `decode_one` and related operations.
	/// @tparam _DecodeCodePoints The output of `decode_one` and related operations.
	/// @tparam _MaxCodeUnits The maximum number of code units that can be output through a given operation. Directly
	/// related to the maximum_code_units inline constexpr variable definition.
	/// @tparam _MaxCodePoints The maximum number of code points that can be output through a given operation. Directly
	/// related to the maximum_code_points inline constexpr variable definition.
	///
	/// @remarks This class is generally interacted with by using its derivate class, ztd::text::any_byte_encoding, and
	/// its convenience alias, ztd::text::any_encoding. This class's use is recommended only for power users who have
	/// encoding ranges that cannot be interacted with through `ztd::span` and therefore need other ways. We
	/// are looking into ways to produce a ranges::subrange<any_iterator> as a completely generic range to aid those
	/// individuals who do not want to deal in just `ztd::span` s.
	//////
	template <typename _EncodeCodeUnits, typename _EncodeCodePoints, typename _DecodeCodeUnits,
		typename _DecodeCodePoints, ::std::size_t _MaxCodeUnits = __txt_detail::__default_max_code_units_any_encoding,
		::std::size_t _MaxCodePoints = __txt_detail::__default_max_code_points_any_encoding>
	class any_encoding_with {
	public:
		class any_encode_state;
		class any_decode_state;

	public:
		//////
		/// @brief The state that can be used between calls to `decode.`
		///
		/// @remarks This is an opaque struct with no members. It follows the "encoding-dependent state" model, which
		/// means it has a constructor that takes an ztd::text::any_encoding_with so it can properly initialize its
		/// state.
		//////
		using decode_state = any_decode_state;
		//////
		/// @brief The state that can be used between calls to `encode.`
		///
		/// @remarks This is an opaque struct with no members. It follows the "encoding-dependent state" model, which
		/// means it has a constructor that takes an ztd::text::any_encoding_with so it can properly initialize its
		/// state.
		//////
		using encode_state = any_encode_state;
		//////
		/// @brief The individual units that result from an encode operation or are used as input to a decode
		/// operation.
		//////
		using code_unit = ranges::range_value_type_t<_EncodeCodeUnits>;
		//////
		/// @brief The individual units that result from a decode operation or as used as input to an encode
		/// operation.
		//////
		using code_point = ranges::range_value_type_t<_DecodeCodePoints>;
		//////
		/// @brief Whether or not the encode operation can process all forms of input into code point values.
		///
		/// @remarks This is always going to be false because this is a type-erased encoding; this value is determined
		/// by a runtime decision, which means that the most conservative and truthful answer is selected for this
		/// property.
		//////
		using is_encode_injective = ::std::false_type;
		//////
		/// @brief Whether or not the decode operation can process all forms of input into code point values.
		///
		/// @remarks This is always going to be false because this is a type-erased encoding; this value is determined
		/// by a runtime decision, which means that the most conservative and truthful answer is selected for this
		/// property.
		//////
		using is_decode_injective = ::std::false_type;
		//////
		/// @brief The maximum number of code points a single complete operation of decoding can produce. This is
		/// 1 for all Unicode Transformation Format (UTF) encodings.
		//////
		static inline constexpr ::std::size_t max_code_points = _MaxCodePoints;
		//////
		/// @brief The maximum code units a single complete operation of encoding can produce.
		static inline constexpr ::std::size_t max_code_units = _MaxCodeUnits;

	private:
		using __decode_result                = decode_result<_DecodeCodeUnits, _DecodeCodePoints, decode_state>;
		using __encode_result                = encode_result<_EncodeCodePoints, _EncodeCodeUnits, encode_state>;
		using __count_as_decoded_result      = count_result<_DecodeCodeUnits, decode_state>;
		using __count_as_encoded_result      = count_result<_EncodeCodePoints, encode_state>;
		using __validate_decodable_as_result = validate_result<_DecodeCodeUnits, decode_state>;
		using __validate_encodable_as_result = validate_result<_EncodeCodePoints, encode_state>;
		using __decode_error_handler = ::std::function<__decode_result(const any_encoding_with&, __decode_result,
			const ::ztd::span<const code_unit>&, const ::ztd::span<const code_point>&)>;
		using __encode_error_handler = ::std::function<__encode_result(const any_encoding_with&, __encode_result,
			const ::ztd::span<const code_point>&, const ::ztd::span<const code_unit>&)>;
		using __count_as_decoded_error_handler = ::std::function<__count_as_decoded_result(const any_encoding_with&,
			__count_as_decoded_result, const ::ztd::span<const code_point>&, const ::ztd::span<const code_unit>&)>;
		using __count_as_encoded_error_handler = ::std::function<__count_as_encoded_result(const any_encoding_with&,
			__count_as_encoded_result, const ::ztd::span<const code_unit>&, const ::ztd::span<const code_point>&)>;

		struct __erased_state {
			virtual ~__erased_state() {
			}
		};

		struct __erased {
			virtual bool __contains_unicode_encoding() const noexcept = 0;
			virtual ::std::optional<::ztd::span<const code_point>>
			__maybe_replacement_code_points() const noexcept = 0;
			virtual ::std::optional<::ztd::span<const code_unit>>
			__maybe_replacement_code_units() const noexcept = 0;

			virtual __decode_result __decode_one(const any_encoding_with& __self, _DecodeCodeUnits __input,
				_DecodeCodePoints __output, __decode_error_handler __error_handler,
				decode_state& __state) const                                                                    = 0;
			virtual __encode_result __encode_one(const any_encoding_with& __self, _EncodeCodePoints __input,
				_EncodeCodeUnits __output, __encode_error_handler __error_handler, encode_state& __state) const = 0;
			virtual __validate_decodable_as_result __validate_decodable_as_one(
				const any_encoding_with& __self, _DecodeCodeUnits __input, decode_state& __state) const = 0;
			virtual __validate_encodable_as_result __validate_encodable_as_one(
				const any_encoding_with& __self, _EncodeCodePoints __input, encode_state& __state) const = 0;
			virtual __count_as_encoded_result __count_as_encoded_one(const any_encoding_with& __self,
				_EncodeCodePoints __input, __count_as_encoded_error_handler __error_handler,
				encode_state& __state) const                                                             = 0;
			virtual __count_as_decoded_result __count_as_decoded_one(const any_encoding_with& __self,
				_DecodeCodeUnits __input, __count_as_decoded_error_handler __error_handler,
				decode_state& __state) const                                                             = 0;

			virtual __decode_result __decode(const any_encoding_with& __self, _DecodeCodeUnits __input,
				_DecodeCodePoints __output, __decode_error_handler __error_handler,
				decode_state& __state) const                                                                    = 0;
			virtual __encode_result __encode(const any_encoding_with& __self, _EncodeCodePoints __input,
				_EncodeCodeUnits __output, __encode_error_handler __error_handler, encode_state& __state) const = 0;
			virtual __validate_decodable_as_result __validate_decodable_as(
				const any_encoding_with& __self, _DecodeCodeUnits __input, decode_state& __state) const = 0;
			virtual __validate_encodable_as_result __validate_encodable_as(
				const any_encoding_with& __self, _EncodeCodePoints __input, encode_state& __state) const = 0;
			virtual __count_as_encoded_result __count_as_encoded(const any_encoding_with& __self,
				_EncodeCodePoints __input, __count_as_encoded_error_handler __error_handler,
				encode_state& __state) const                                                             = 0;
			virtual __count_as_decoded_result __count_as_decoded(const any_encoding_with& __self,
				_DecodeCodeUnits __input, __count_as_decoded_error_handler __error_handler,
				decode_state& __state) const                                                             = 0;

			virtual ::std::unique_ptr<__erased_state> __create_encode_state() const = 0;
			virtual ::std::unique_ptr<__erased_state> __create_decode_state() const = 0;

			virtual ~__erased() {
			}
		};

	public:
		//////
		/// @brief The state for any encoding's decode state.
		class any_decode_state {
		public:
			//////
			/// @brief Creates a state properly initialized from the stored encoding.
			any_decode_state(const any_encoding_with& __encoding)
			: _M_state(__encoding._M_storage->__create_decode_state()) {
			}

			//////
			/// @brief You cannot copy construct an any_decode_state.
			any_decode_state(const any_decode_state&) = delete;

			//////
			/// @brief You cannot copy assign an any_decode_state.
			any_decode_state& operator=(const any_decode_state&) = delete;

			//////
			/// @brief Move constructs an any_decode_state.
			any_decode_state(any_decode_state&&) = default;

			//////
			/// @brief Move assigns an any_decode_state.
			any_decode_state& operator=(any_decode_state&&) = default;

			__erased_state* _M_get_erased_state() const noexcept {
				return _M_state.get();
			}

		private:
			template <typename>
			friend struct __typed;

			::std::unique_ptr<__erased_state> _M_state;
		};

		//////
		/// @brief The state for any encoding's encode state.
		class any_encode_state {
		public:
			//////
			/// @brief Creates a state properly initialized from the stored encoding.
			any_encode_state(const any_encoding_with& __encoding)
			: _M_state(__encoding._M_storage->__create_encode_state()) {
			}

			//////
			/// @brief You cannot copy construct an any_encode_state.
			any_encode_state(const any_encode_state&) = delete;

			//////
			/// @brief You cannot copy assign an any_encode_state.
			any_encode_state& operator=(const any_encode_state&) = delete;

			//////
			/// @brief Move constructs an any_encode_state.
			any_encode_state(any_encode_state&&) = default;

			//////
			/// @brief Move assigns an any_encode_state.
			any_encode_state& operator=(any_encode_state&&) = default;

			__erased_state* _M_get_erased_state() const noexcept {
				return _M_state.get();
			}

		private:
			/* broken in Clang/GCC */
			template <typename>
			friend struct __typed;

			::std::unique_ptr<__erased_state> _M_state;
		};

	private:
		template <typename _State>
		struct __typed_state : public __erased_state, public ebco<_State, 0> {
		private:
			using __base_t = ebco<_State, 0>;

		public:
			using __base_t::__base_t;
		};

		template <typename _Encoding>
		struct __typed : private ebco<_Encoding, 0>, public __erased {
		private:
			// static_assert(max_code_points_v<_Encoding> <= _MaxCodePoints,
			//	"encoding must have less than or equal to the number of max potential output code points");
			// static_assert(max_code_units_v<_Encoding> <= _MaxCodeUnits,
			//	"encoding must have less than or equal to the number of max potential output code units");

			using __real_decode_state = decode_state_t<_Encoding>;
			using __real_encode_state = encode_state_t<_Encoding>;
			using __encoding_t        = _Encoding;
			using __base_t            = ebco<_Encoding, 0>;

		public:
			template <typename _ArgEncoding,
				::std::enable_if_t<!::std::is_same_v<remove_cvref_t<_ArgEncoding>, __typed>>* = nullptr>
			__typed(_ArgEncoding&& __encoding) : __base_t(::std::forward<_ArgEncoding>(__encoding)) {
			}

			__typed(const __typed&) = default;
			__typed(__typed&&)      = default;
			__typed& operator=(const __typed&) = default;
			__typed& operator=(__typed&&) = default;

			virtual ::std::optional<::ztd::span<const code_point>>
			__maybe_replacement_code_points() const noexcept override {
				if constexpr (is_code_points_replaceable_v<_Encoding>) {
					using __inner_type                = ::ztd::span<const code_point>;
					const _Encoding& __real_encoding  = this->_M_get_encoding();
					decltype(auto) __real_replacement = __real_encoding.replacement_code_points();
					return __inner_type(ranges::ranges_adl::adl_data(__real_replacement),
						ranges::ranges_adl::adl_size(__real_replacement));
				}
				else if constexpr (is_code_points_maybe_replaceable_v<_Encoding>) {
					const _Encoding& __real_encoding = this->_M_get_encoding();
					return __real_encoding.maybe_replacement_code_points();
				}
				else {
					return ::std::nullopt;
				}
			}

			virtual ::std::optional<::ztd::span<const code_unit>>
			__maybe_replacement_code_units() const noexcept override {
				if constexpr (is_code_units_replaceable_v<_Encoding>) {
					using __inner_type                = ::ztd::span<const code_unit>;
					const _Encoding& __real_encoding  = this->_M_get_encoding();
					decltype(auto) __real_replacement = __real_encoding.replacement_code_units();
					return __inner_type(ranges::ranges_adl::adl_data(__real_replacement),
						ranges::ranges_adl::adl_size(__real_replacement));
				}
				else if constexpr (is_code_units_maybe_replaceable_v<_Encoding>) {
					const _Encoding& __real_encoding = this->_M_get_encoding();
					return __real_encoding.maybe_replacement_code_units();
				}
				else {
					return ::std::nullopt;
				}
			}

			virtual bool __contains_unicode_encoding() const noexcept override {
				const auto& __real_encoding = this->_M_get_encoding();
				return ::ztd::text::contains_unicode_encoding(__real_encoding);
			}

			// modifiers: single operations
			virtual __decode_result __decode_one(const any_encoding_with& __self, _DecodeCodeUnits __input,
				_DecodeCodePoints __output, __decode_error_handler __error_handler,
				decode_state& __state) const override {
				__real_decode_state& __actual_state = this->_M_get_state(__state);
				__txt_detail::__progress_handler<::std::false_type, any_encoding_with> __pass_handler {};
				auto& __encoding  = this->_M_get_encoding();
				auto __raw_result = __encoding.decode_one(
					::std::move(__input), ::std::move(__output), __pass_handler, __actual_state);
				if (__raw_result.error_code != encoding_error::ok) {
					return __error_handler(__self,
						__decode_result(::std::move(__raw_result.input), ::std::move(__raw_result.output),
						     __state, __raw_result.error_code, __raw_result.handled_errors),
						__pass_handler._M_code_units_progress(), __pass_handler._M_code_points_progress());
				}
				return __decode_result(::std::move(__raw_result.input), ::std::move(__raw_result.output), __state,
					__raw_result.error_code, __raw_result.handled_errors);
			}

			virtual __encode_result __encode_one(const any_encoding_with& __self, _EncodeCodePoints __input,
				_EncodeCodeUnits __output, __encode_error_handler __error_handler,
				encode_state& __state) const override {
				__real_encode_state& __actual_state = this->_M_get_state(__state);
				__txt_detail::__progress_handler<::std::false_type, any_encoding_with> __pass_handler {};
				auto& __encoding  = this->_M_get_encoding();
				auto __raw_result = __encoding.encode_one(
					::std::move(__input), ::std::move(__output), __pass_handler, __actual_state);
				if (__raw_result.error_code != encoding_error::ok) {
					return __error_handler(__self,
						__encode_result(::std::move(__raw_result.input), ::std::move(__raw_result.output),
						     __state, __raw_result.error_code, __raw_result.handled_errors),
						__pass_handler._M_code_points_progress(), __pass_handler._M_code_units_progress());
				}
				return __encode_result(::std::move(__raw_result.input), ::std::move(__raw_result.output), __state,
					__raw_result.error_code, __raw_result.handled_errors);
			}

			virtual __validate_decodable_as_result __validate_decodable_as_one(
				const any_encoding_with&, _DecodeCodeUnits __input, decode_state& __state) const override {
				__real_decode_state& __actual_state = this->_M_get_state(__state);
				auto& __encoding                    = this->_M_get_encoding();
				if constexpr (is_detected_v<__txt_detail::__detect_adl_text_validate_decodable_as_one, _Encoding,
					              _DecodeCodePoints, __real_decode_state>) {
					auto __raw_result
						= text_validate_decodable_as_one(__encoding, ::std::move(__input), __actual_state);
					return __validate_decodable_as_result(
						::std::move(__raw_result.input), __raw_result.valid, __state);
				}
				else if constexpr (is_detected_v<__txt_detail::__detect_adl_internal_text_validate_decodable_as_one,
					                   _Encoding, _DecodeCodePoints, __real_decode_state>) {
					auto __raw_result
						= __text_validate_decodable_as_one(__encoding, ::std::move(__input), __actual_state);
					return __validate_decodable_as_result(
						::std::move(__raw_result.input), __raw_result.valid, __state);
				}
				else {
					__real_encode_state __encode_state = make_encode_state(__encoding);
					auto __raw_result                  = __txt_detail::__basic_validate_decodable_as_one(
                              ::std::move(__input), __encoding, __actual_state, __encode_state);
					return __validate_decodable_as_result(
						::std::move(__raw_result.input), __raw_result.valid, __state);
				}
			}

			virtual __validate_encodable_as_result __validate_encodable_as_one(
				const any_encoding_with&, _EncodeCodePoints __input, encode_state& __state) const override {
				__real_encode_state& __actual_state = this->_M_get_state(__state);
				auto& __encoding                    = this->_M_get_encoding();
				if constexpr (is_detected_v<__txt_detail::__detect_adl_text_validate_encodable_as_one, _Encoding,
					              _EncodeCodePoints, __real_encode_state>) {
					auto __raw_result
						= text_validate_encodable_as_one(__encoding, ::std::move(__input), __actual_state);
					return __validate_encodable_as_result(
						::std::move(__raw_result.input), __raw_result.valid, __state);
				}
				else if constexpr (is_detected_v<__txt_detail::__detect_adl_internal_text_validate_encodable_as_one,
					                   _Encoding, _EncodeCodePoints, __real_encode_state>) {
					auto __raw_result
						= __text_validate_encodable_as_one(__encoding, ::std::move(__input), __actual_state);
					return __validate_encodable_as_result(
						::std::move(__raw_result.input), __raw_result.valid, __state);
				}
				else {
					__real_decode_state __decode_state = make_decode_state(__encoding);
					auto __raw_result                  = __txt_detail::__basic_validate_encodable_as_one(
                              ::std::move(__input), __encoding, __actual_state, __decode_state);
					return __validate_encodable_as_result(
						::std::move(__raw_result.input), __raw_result.valid, __state);
				}
			}

			virtual __count_as_decoded_result __count_as_decoded_one(const any_encoding_with& __self,
				_DecodeCodeUnits __input, __count_as_decoded_error_handler __error_handler,
				decode_state& __state) const override {
				__real_decode_state& __actual_state = this->_M_get_state(__state);
				__txt_detail::__progress_handler<::std::false_type, any_encoding_with> __pass_handler {};
				auto& __encoding = this->_M_get_encoding();
				if constexpr (is_detected_v<__txt_detail::__detect_adl_internal_text_count_as_decoded_one,
					              _Encoding, _DecodeCodeUnits, decltype(__pass_handler), __real_decode_state>) {
					auto __raw_result = text_count_as_decoded_one(
						__encoding, ::std::move(__input), __pass_handler, __actual_state);
					if (__raw_result.error_code != encoding_error::ok) {
						return __error_handler(__self,
							__count_as_decoded_result(::std::move(__raw_result.input), __raw_result.count,
							     __state, __raw_result.error_code, __raw_result.handled_errors),
							__pass_handler._M_code_points_progress(), __pass_handler._M_code_units_progress());
					}
					return __count_as_decoded_result(::std::move(__raw_result.input), __raw_result.count, __state,
						__raw_result.error_code, __raw_result.handled_errors);
				}
				else if constexpr (is_detected_v<__txt_detail::__detect_adl_internal_text_count_as_decoded_one,
					                   _Encoding, _DecodeCodeUnits, decltype(__pass_handler),
					                   __real_decode_state>) {
					auto __raw_result = __text_count_as_decoded_one(
						__encoding, ::std::move(__input), __pass_handler, __actual_state);
					if (__raw_result.error_code != encoding_error::ok) {
						return __error_handler(__self,
							__count_as_decoded_result(::std::move(__raw_result.input), __raw_result.count,
							     __state, __raw_result.error_code, __raw_result.handled_errors),
							__pass_handler._M_code_points_progress(), __pass_handler._M_code_units_progress());
					}
					return __count_as_decoded_result(::std::move(__raw_result.input), __raw_result.count, __state,
						__raw_result.error_code, __raw_result.handled_errors);
				}
				else {
					auto __raw_result = __txt_detail::__basic_count_as_decoded_one(
						::std::move(__input), __encoding, __pass_handler, __actual_state);
					if (__raw_result.error_code != encoding_error::ok) {
						return __error_handler(__self,
							__count_as_decoded_result(::std::move(__raw_result.input), __raw_result.count,
							     __state, __raw_result.error_code, __raw_result.handled_errors),
							__pass_handler._M_code_points_progress(), __pass_handler._M_code_units_progress());
					}
					return __count_as_decoded_result(::std::move(__raw_result.input), __raw_result.count, __state,
						__raw_result.error_code, __raw_result.handled_errors);
				}
			}

			virtual __count_as_encoded_result __count_as_encoded_one(const any_encoding_with& __self,
				_EncodeCodePoints __input, __count_as_encoded_error_handler __error_handler,
				encode_state& __state) const override {
				__real_encode_state& __actual_state = this->_M_get_state(__state);
				__txt_detail::__progress_handler<::std::false_type, any_encoding_with> __pass_handler {};
				auto& __encoding = this->_M_get_encoding();
				if constexpr (is_detected_v<__txt_detail::__detect_adl_text_count_as_encoded_one, _Encoding,
					              _EncodeCodePoints, decltype(__pass_handler), __real_encode_state>) {
					auto __raw_result = text_count_as_encoded_one(
						__encoding, ::std::move(__input), __pass_handler, __actual_state);
					if (__raw_result.error_code != encoding_error::ok) {
						return __error_handler(__self,
							__count_as_encoded_result(::std::move(__raw_result.input),
							     ::std::move(__raw_result.output), __state, __raw_result.error_code,
							     __raw_result.handled_errors),
							__pass_handler._M_code_units_progress(), __pass_handler._M_code_points_progress());
					}
					return __count_as_encoded_result(::std::move(__raw_result.input), __raw_result.count, __state,
						__raw_result.error_code, __raw_result.handled_errors);
				}
				else if constexpr (is_detected_v<__txt_detail::__detect_adl_internal_text_count_as_encoded_one,
					                   _Encoding, _EncodeCodePoints, decltype(__pass_handler),
					                   __real_encode_state>) {
					auto __raw_result = __text_count_as_encoded_one(
						__encoding, ::std::move(__input), __pass_handler, __actual_state);
					if (__raw_result.error_code != encoding_error::ok) {
						return __error_handler(__self,
							__count_as_encoded_result(::std::move(__raw_result.input), __raw_result.count,
							     __state, __raw_result.error_code, __raw_result.handled_errors),
							__pass_handler._M_code_units_progress(), __pass_handler._M_code_points_progress());
					}
					return __count_as_encoded_result(::std::move(__raw_result.input), __raw_result.count, __state,
						__raw_result.error_code, __raw_result.handled_errors);
				}
				else {
					auto __raw_result = __txt_detail::__basic_count_as_encoded_one(
						::std::move(__input), __encoding, __pass_handler, __actual_state);
					if (__raw_result.error_code != encoding_error::ok) {
						return __error_handler(__self,
							__count_as_encoded_result(::std::move(__raw_result.input), __raw_result.count,
							     __state, __raw_result.error_code, __raw_result.handled_errors),
							__pass_handler._M_code_units_progress(), __pass_handler._M_code_points_progress());
					}
					return __count_as_encoded_result(::std::move(__raw_result.input), __raw_result.count, __state);
				}
			}

			// modifiers: bulk
			virtual __decode_result __decode(const any_encoding_with& __self, _DecodeCodeUnits __input,
				_DecodeCodePoints __output, __decode_error_handler __error_handler,
				decode_state& __state) const override {
				__real_decode_state& __actual_state = this->_M_get_state(__state);
				__txt_detail::__progress_handler<::std::false_type, any_encoding_with> __pass_handler {};
				auto& __encoding  = this->_M_get_encoding();
				auto __raw_result = ::ztd::text::decode_into(
					::std::move(__input), __encoding, ::std::move(__output), __pass_handler, __actual_state);
				if (__raw_result.error_code != encoding_error::ok) {
					return __error_handler(__self,
						__decode_result(::std::move(__raw_result.input), ::std::move(__raw_result.output),
						     __state, __raw_result.error_code, __raw_result.handled_errors),
						__pass_handler._M_code_units_progress(), __pass_handler._M_code_points_progress());
				}
				return __decode_result(::std::move(__raw_result.input), ::std::move(__raw_result.output), __state,
					__raw_result.error_code, __raw_result.handled_errors);
			}

			virtual __encode_result __encode(const any_encoding_with& __self, _EncodeCodePoints __input,
				_EncodeCodeUnits __output, __encode_error_handler __error_handler,
				encode_state& __state) const override {
				__real_encode_state& __actual_state = this->_M_get_state(__state);
				__txt_detail::__progress_handler<::std::false_type, any_encoding_with> __pass_handler {};
				auto& __encoding  = this->_M_get_encoding();
				auto __raw_result = ::ztd::text::encode_into(
					::std::move(__input), __encoding, ::std::move(__output), __pass_handler, __actual_state);
				if (__raw_result.error_code != encoding_error::ok) {
					return __error_handler(__self,
						__encode_result(::std::move(__raw_result.input), ::std::move(__raw_result.output),
						     __state, __raw_result.error_code, __raw_result.handled_errors),
						__pass_handler._M_code_points_progress(), __pass_handler._M_code_units_progress());
				}
				return __encode_result(::std::move(__raw_result.input), ::std::move(__raw_result.output), __state,
					__raw_result.error_code, __raw_result.handled_errors);
			}

			virtual __validate_decodable_as_result __validate_decodable_as(
				const any_encoding_with&, _DecodeCodeUnits __input, decode_state& __state) const override {
				__real_decode_state& __actual_state = this->_M_get_state(__state);
				auto& __encoding                    = this->_M_get_encoding();
				auto __raw_result
					= ::ztd::text::validate_decodable_as(::std::move(__input), __encoding, __actual_state);
				return __validate_decodable_as_result(::std::move(__raw_result.input), __raw_result.valid, __state);
			}

			virtual __validate_encodable_as_result __validate_encodable_as(
				const any_encoding_with&, _EncodeCodePoints __input, encode_state& __state) const override {
				__real_encode_state& __actual_state = this->_M_get_state(__state);
				auto& __encoding                    = this->_M_get_encoding();
				auto __raw_result
					= ::ztd::text::validate_encodable_as(::std::move(__input), __encoding, __actual_state);
				return __validate_encodable_as_result(::std::move(__raw_result.input), __raw_result.valid, __state);
			}

			virtual __count_as_decoded_result __count_as_decoded(const any_encoding_with& __self,
				_DecodeCodeUnits __input, __count_as_decoded_error_handler __error_handler,
				decode_state& __state) const override {
				__real_decode_state& __actual_state = this->_M_get_state(__state);
				__txt_detail::__progress_handler<::std::false_type, any_encoding_with> __pass_handler {};
				auto& __encoding  = this->_M_get_encoding();
				auto __raw_result = ::ztd::text::count_as_decoded(
					::std::move(__input), __encoding, __pass_handler, __actual_state);
				if (__raw_result.error_code != encoding_error::ok) {
					return __error_handler(__self,
						__count_as_decoded_result(::std::move(__raw_result.input), __raw_result.count, __state,
						     __raw_result.error_code, __raw_result.handled_errors),
						__pass_handler._M_code_points_progress(), __pass_handler._M_code_units_progress());
				}
				return __count_as_decoded_result(::std::move(__raw_result.input), __raw_result.count, __state,
					__raw_result.error_code, __raw_result.handled_errors);
			}

			virtual __count_as_encoded_result __count_as_encoded(const any_encoding_with& __self,
				_EncodeCodePoints __input, __count_as_encoded_error_handler __error_handler,
				encode_state& __state) const override {
				__real_encode_state& __actual_state = this->_M_get_state(__state);
				__txt_detail::__progress_handler<::std::false_type, any_encoding_with> __pass_handler {};
				auto& __encoding  = this->_M_get_encoding();
				auto __raw_result = ::ztd::text::count_as_encoded(
					::std::move(__input), __encoding, __pass_handler, __actual_state);
				if (__raw_result.error_code != encoding_error::ok) {
					return __error_handler(__self,
						__count_as_encoded_result(::std::move(__raw_result.input), __raw_result.count, __state,
						     __raw_result.error_code, __raw_result.handled_errors),
						__pass_handler._M_code_units_progress(), __pass_handler._M_code_points_progress());
				}
				return __count_as_encoded_result(::std::move(__raw_result.input), __raw_result.count, __state,
					__raw_result.error_code, __raw_result.handled_errors);
			}

			virtual ::std::unique_ptr<__erased_state> __create_encode_state() const override {
				auto& __encoding = this->_M_get_encoding();
				return ::std::make_unique<__typed_state<__real_encode_state>>(make_encode_state(__encoding));
			}

			virtual ::std::unique_ptr<__erased_state> __create_decode_state() const override {
				auto& __encoding = this->_M_get_encoding();
				return ::std::make_unique<__typed_state<__real_decode_state>>(make_decode_state(__encoding));
			}

		private:
			const __encoding_t& _M_get_encoding() const {
				return this->__base_t::get_value();
			}

			__real_encode_state& _M_get_state(encode_state& __state) const {
				__erased_state* __erased_ptr = __state._M_get_erased_state();
				__typed_state<__real_encode_state>* __typed_ptr
					= static_cast<__typed_state<__real_encode_state>*>(__erased_ptr);
				return __typed_ptr->get_value();
			}

			__real_decode_state& _M_get_state(decode_state& __state) const {
				__erased_state* __erased_ptr = __state._M_get_erased_state();
				__typed_state<__real_decode_state>* __typed_ptr
					= static_cast<__typed_state<__real_decode_state>*>(__erased_ptr);
				return __typed_ptr->get_value();
			}
		};

		::std::unique_ptr<__erased> _M_storage;

	public:
		//////
		/// @brief Cannot default-construct a ztd::text::any_encoding_with object.
		any_encoding_with() = delete;

		//////
		/// @brief Constructs a ztd::text::any_encoding_with with the encoding object and any additional arguments.
		///
		/// @tparam _Encoding The Encoding specified by the first argument.
		///
		/// @param[in] __encoding The encoding object that informs the ztd::text::any_encoding_with what encoding
		/// object to store.
		/// @param[in] __args Any additional arguments used to construct the encoding in the erased storage.
		//////
		template <typename _Encoding, typename... _Args,
			::std::enable_if_t<
			     !::std::is_same_v<_Encoding,
			          any_encoding_with> && !is_specialization_of_v<remove_cvref_t<_Encoding>, ::std::in_place_type_t>>* = nullptr>
		any_encoding_with(_Encoding&& __encoding, _Args&&... __args)
		: any_encoding_with(::std::in_place_type<remove_cvref_t<_Encoding>>, ::std::forward<_Encoding>(__encoding),
			::std::forward<_Args>(__args)...) {
		}

		//////
		/// @brief Constructs a ztd::text::any_encoding_with with the encoding type specified in the `__tag`
		/// argument.
		///
		/// @tparam _Encoding The Encoding specified by the `__tag` argument.
		///
		/// @param[in] __tag The type marker that informs the ztd::text::any_encoding_with what encoding object to
		/// store.
		/// @param[in] __args Any additional arguments used to construct the encoding in the erased storage.
		//////
		template <typename _Encoding, typename... _Args>
		any_encoding_with(::std::in_place_type_t<_Encoding> __tag, _Args&&... __args)
		: _M_storage(::std::make_unique<__typed<_Encoding>>(::std::forward<_Args>(__args)...)) {
			(void)__tag;
		}

		//////
		/// @brief Cannot copy-construct a ztd::text::any_encoding_with object.
		any_encoding_with(const any_encoding_with&) = delete;

		//////
		/// @brief Cannot copy-assign a ztd::text::any_encoding_with object.
		any_encoding_with& operator=(const any_encoding_with&) = delete;

		//////
		/// @brief Move-constructs a ztd::text::any_encoding_with from the provided r-value reference.
		///
		/// @remarks This leaves the passed-in r-value reference without an encoding object. Calling any function on a
		/// moved-fron ztd::text::any_encoding_with, except for destruction, is a violation and invokes Undefined
		/// Behavior (generally, a crash).
		//////
		any_encoding_with(any_encoding_with&&) = default;

		//////
		/// @brief Move-assigns a ztd::text::any_encoding_with from the provided r-value reference.
		///
		/// @remarks This leaves the passed-in r-value reference without an encoding object. Calling any function on a
		/// moved-fron ztd::text::any_encoding_with, except for destruction, is a violation and invokes Undefined
		/// Behavior (generally, a crash).
		//////
		any_encoding_with& operator=(any_encoding_with&&) = default;

		//////
		/// @brief Retrieves the replacement code points for when conversions fail and
		/// ztd::text::replacement_handler_t (or equivalent) needs to make a substitution.
		///
		/// @return A `std::optional` of `ztd::span` of `const code_point`s. The returned `std::optional`
		/// value is engaged (has a value) if the stored encoding has a valid `replacement_code_points`
		/// function and it can be called. If it does not, then the library checks to see if the
		/// `maybe_replacement_code_points` function exists, and returns the `std::optional` from that type
		/// directly. If neither are present, an unengaged `std::optional` is returned.
		//////
		::std::optional<::ztd::span<const code_point>> maybe_replacement_code_points() const noexcept {
			return this->_M_storage->__maybe_replacement_code_points();
		}

		//////
		/// @brief Retrieves the replacement code units for when conversions fail and ztd::text::replacement_handler_t
		/// (or equivalent) needs to make a substitution.
		///
		/// @return A `std::optional` of `ztd::span` of `const code_unit`s. The returned `std::optional`
		/// value is engaged (has a value) if the stored encoding has a valid `replacement_code_units`
		/// function and it can be called. If it does not, then the library checks to see if the @c
		/// maybe_replacement_code_units function exists, and returns the `std::optional` from that type directly. If
		/// neither are present, an unengaged `std::optional` is returned.
		//////
		::std::optional<::ztd::span<const code_unit>> maybe_replacement_code_units() const noexcept {
			return this->_M_storage->__maybe_replacement_code_units();
		}

		//////
		/// @brief Returns whether or not the encoding stored in this ztd::text::any_encoding_with is a Unicode
		/// encoding.
		///
		/// @remarks This can be useful to know, in advance, whether or not there is a chance for lossy behavior. Even
		/// if, at compile time, various functions will demand you use an error handler, this runtime property can
		/// help you get a decent idea of just how bad and lossy this conversion might be compared to normal UTF
		/// conversion formats.
		//////
		bool contains_unicode_encoding() const noexcept {
			return this->_M_storage->__contains_unicode_encoding();
		}

		//////
		/// @brief Decodes a single complete unit of information as code points and produces a result with the
		/// input and output ranges moved past what was successfully read and written; or, produces an error and
		/// returns the input and output ranges untouched.
		///
		/// @param[in] __input The input view to read code uunits from.
		/// @param[in] __output The output view to write code points into.
		/// @param[in] __error_handler The error handler to invoke if encoding fails.
		/// @param[in, out] __state The necessary state information. For this encoding, the state is empty and means
		/// very little.
		///
		/// @returns A ztd::text::decode_result object that contains the reconstructed input range,
		/// reconstructed output range, error handler, and a reference to the passed-in state.
		///
		/// @remarks To the best ability of the implementation, the iterators will be returned untouched (e.g.,
		/// the input models at least a view and a forward_range). If it is not possible, returned ranges may be
		/// incremented even if an error occurs due to the semantics of any view that models an input_range.
		//////
		__decode_result decode_one(_DecodeCodeUnits __input, _DecodeCodePoints __output,
			__decode_error_handler __error_handler, decode_state& __state) const {
			return this->_M_storage->__decode_one(
				*this, ::std::move(__input), ::std::move(__output), ::std::move(__error_handler), __state);
		}

		//////
		/// @brief Encodes a single complete unit of information as code units and produces a result with the
		/// input and output ranges moved past what was successfully read and written; or, produces an error and
		/// returns the input and output ranges untouched.
		///
		/// @param[in] __input The input view to read code points from.
		/// @param[in] __output The output view to write code units into.
		/// @param[in] __error_handler The error handler to invoke if encoding fails.
		/// @param[in, out] __state The necessary state information. For this encoding, the state is empty and means
		/// very little.
		///
		/// @returns A ztd::text::encode_result object that contains the reconstructed input range,
		/// reconstructed output range, error handler, and a reference to the passed-in state.
		///
		/// @remarks To the best ability of the implementation, the iterators will be returned untouched (e.g.,
		/// the input models at least a view and a forward_range). If it is not possible, returned ranges may be
		/// incremented even if an error occurs due to the semantics of any view that models an input_range.
		//////
		__encode_result encode_one(_EncodeCodePoints __input, _EncodeCodeUnits __output,
			__encode_error_handler __error_handler, encode_state& __state) const {
			return this->_M_storage->__encode_one(
				*this, ::std::move(__input), ::std::move(__output), ::std::move(__error_handler), __state);
		}

	private:
		__validate_decodable_as_result __validate_decodable_as_one(
			_DecodeCodeUnits __input, decode_state& __state) const {
			return this->_M_storage->__validate_decodable_as_one(*this, ::std::move(__input), __state);
		}

		__validate_encodable_as_result __validate_encodable_as_one(
			_EncodeCodePoints __input, encode_state& __state) const {
			return this->_M_storage->__validate_encodable_as_one(*this, ::std::move(__input), __state);
		}

		__count_as_encoded_result __count_as_encoded_one(_EncodeCodePoints __input,
			__count_as_encoded_error_handler __error_handler, encode_state& __state) const {
			return this->_M_storage->__count_as_encoded_one(
				*this, ::std::move(__input), ::std::move(__error_handler), __state);
		}

		__count_as_decoded_result __count_as_decoded_one(
			_DecodeCodeUnits __input, __encode_error_handler __error_handler, decode_state& __state) const {
			return this->_M_storage->__count_as_decoded_one(
				*this, ::std::move(__input), ::std::move(__error_handler), __state);
		}

		__decode_result __decode(_DecodeCodeUnits __input, _DecodeCodePoints __output,
			__decode_error_handler __error_handler, decode_state& __state) const {
			return this->_M_storage->__decode(
				*this, ::std::move(__input), ::std::move(__output), ::std::move(__error_handler), __state);
		}

		__encode_result __encode(_EncodeCodePoints __input, _EncodeCodeUnits __output,
			__encode_error_handler __error_handler, encode_state& __state) const {
			return this->_M_storage->__encode(
				*this, ::std::move(__input), ::std::move(__output), ::std::move(__error_handler), __state);
		}

		__validate_decodable_as_result __validate_decodable_as(
			_DecodeCodeUnits __input, __decode_error_handler __error_handler, decode_state& __state) const {
			return this->_M_storage->__validate_decodable_as(
				*this, ::std::move(__input), ::std::move(__error_handler), __state);
		}

		__validate_encodable_as_result __validate_encodable_as(
			_EncodeCodePoints __input, __encode_error_handler __error_handler, encode_state& __state) const {
			return this->_M_storage->__validate_encodable_as(
				*this, ::std::move(__input), ::std::move(__error_handler), __state);
		}

		__count_as_encoded_result __count_as_encoded(
			_EncodeCodePoints __input, __decode_error_handler __error_handler, encode_state& __state) const {
			return this->_M_storage->__count_as_encoded(
				*this, ::std::move(__input), ::std::move(__error_handler), __state);
		}

		__count_as_decoded_result __count_as_decoded(
			_DecodeCodeUnits __input, __encode_error_handler __error_handler, decode_state& __state) const {
			return this->_M_storage->__count_as_decoded(
				*this, ::std::move(__input), ::std::move(__error_handler), __state);
		}


		//////
		/// @internal
		///
		/// @brief Extension point hooks for the implementation-side only.
		//////
		template <typename _Output, typename _ErrorHandler, typename _State>
		constexpr friend auto __text_decode(text_tag<any_encoding_with>, _DecodeCodeUnits __input,
			type_identity_t<const any_encoding_with&> __encoding, _Output&& __output,
			_ErrorHandler&& __error_handler, _State& __state) {
			return __encoding.__decode(::std::move(__input), ::std::forward<_Output>(__output),
				::std::forward<_ErrorHandler>(__error_handler), __state);
		}

		//////
		/// @internal
		///
		/// @brief Extension point hooks for the implementation-side only.
		//////
		template <typename _Output, typename _ErrorHandler, typename _State>
		constexpr friend auto __text_encode(text_tag<any_encoding_with>, _EncodeCodePoints __input,
			type_identity_t<const any_encoding_with&> __encoding, _Output&& __output,
			_ErrorHandler&& __error_handler, _State& __state) {
			return __encoding.__encode(::std::move(__input), ::std::forward<_Output>(__output),
				::std::forward<_ErrorHandler>(__error_handler), __state);
		}

		//////
		/// @internal
		///
		/// @brief Extension point hooks for the implementation-side only.
		//////
		template <typename _EncodeState>
		constexpr friend auto __text_validate_encodable_as_one(text_tag<any_encoding_with>, _DecodeCodeUnits __input,
			type_identity_t<const any_encoding_with&> __encoding, _EncodeState& __state) {
			return __encoding.__validate_encodable_as_one(::std::move(__input), __state);
		}

		//////
		/// @internal
		///
		/// @brief Extension point hooks for the implementation-side only.
		//////
		template <typename _DecodeState>
		constexpr friend auto __text_validate_decodable_as_one(text_tag<any_encoding_with>, _EncodeCodePoints __input,
			type_identity_t<const any_encoding_with&> __encoding, _DecodeState& __state) {
			return __encoding.__validate_decodable_as_one(::std::move(__input), __state);
		}

		//////
		/// @internal
		///
		/// @brief Extension point hooks for the implementation-side only.
		//////
		template <typename _EncodeState>
		constexpr friend auto __text_validate_encodable_as(text_tag<any_encoding_with>, _DecodeCodeUnits __input,
			type_identity_t<const any_encoding_with&> __encoding, _EncodeState& __state) {
			return __encoding.__validate_encodable_as(::std::move(__input), __state);
		}

		//////
		/// @internal
		///
		/// @brief Extension point hooks for the implementation-side only.
		//////
		template <typename _Input, typename _DecodeState>
		constexpr friend auto __text_validate_decodable_as(text_tag<any_encoding_with>, _EncodeCodePoints __input,
			type_identity_t<const any_encoding_with&> __encoding, _DecodeState& __state) {
			return __encoding.__validate_decodable_as(::std::move(__input), __state);
		}

		//////
		/// @internal
		///
		/// @brief Extension point hooks for the implementation-side only.
		//////
		template <typename _ErrorHandler, typename _State>
		constexpr friend auto __text_count_as_encoded_one(text_tag<any_encoding_with>, _EncodeCodePoints __input,
			type_identity_t<const any_encoding_with&> __encoding, _ErrorHandler&& __error_handler, _State& __state) {
			return __encoding.__count_as_encoded_one(
				::std::move(__input), ::std::forward<_ErrorHandler>(__error_handler), __state);
		}

		//////
		/// @internal
		///
		/// @brief Extension point hooks for the implementation-side only.
		//////
		template <typename _ErrorHandler, typename _State>
		constexpr friend auto __text_count_as_decoded_one(text_tag<any_encoding_with>, _DecodeCodeUnits __input,
			type_identity_t<const any_encoding_with&> __encoding, _ErrorHandler&& __error_handler, _State& __state) {
			return __encoding.__count_as_decoded_one(
				::std::move(__input), ::std::forward<_ErrorHandler>(__error_handler), __state);
		}

		//////
		/// @internal
		///
		/// @brief Extension point hooks for the implementation-side only.
		//////
		template <typename _ErrorHandler, typename _State>
		constexpr friend auto __text_count_as_encoded(text_tag<any_encoding_with>, _EncodeCodePoints __input,
			type_identity_t<const any_encoding_with&> __encoding, _ErrorHandler&& __error_handler, _State& __state) {
			return __encoding.__count_as_encoded(::std::move(__input), ::std::move(__error_handler), __state);
		}

		//////
		/// @internal
		///
		/// @brief Extension point hooks for the implementation-side only.
		//////
		template <typename _ErrorHandler, typename _State>
		constexpr friend auto __text_count_as_decoded(text_tag<any_encoding_with>, _DecodeCodeUnits __input,
			type_identity_t<const any_encoding_with&> __encoding, _ErrorHandler&& __error_handler, _State& __state) {
			return __encoding.__count_as_decoded(
				::std::move(__input), ::std::forward<_ErrorHandler>(__error_handler), __state);
		}
	};

	//////
	/// @brief A type-erased encoding that uses the specified code unit, code point, and input/output ranges for the
	/// various operations.
	//////
	template <typename _EncodeCodeUnit, typename _EncodeCodePoint = const unicode_code_point,
		typename _DecodeCodeUnit     = ::std::add_const_t<_EncodeCodeUnit>,
		typename _DecodeCodePoint    = ::std::remove_const_t<_EncodeCodePoint>,
		::std::size_t _MaxCodeUnits  = __txt_detail::__default_max_code_units_any_encoding,
		::std::size_t _MaxCodePoints = __txt_detail::__default_max_code_points_any_encoding>
	using any_encoding_of = any_encoding_with<::ztd::span<_EncodeCodeUnit>, ::ztd::span<_EncodeCodePoint>,
		::ztd::span<_DecodeCodeUnit>, ::ztd::span<_DecodeCodePoint>, _MaxCodeUnits, _MaxCodePoints>;

	//////
	/// @brief An encoding type that wraps up other encodings to specifically traffic in the given `_Byte` type
	/// provided, which is typically set to `std::byte` .
	///
	/// @tparam _Byte The byte type to use. Typically, this is either <tt>unsigned char</tt> or `std::byte` .
	///
	/// @remarks This type traffics solely in `std::span` s, which for most people is fine. Others may want to
	/// interface with different iterator types (e.g., from a custom Rope implementation or other). For those, one must
	/// first create ranges that can operate with those iterators, then use them themselves. (It's not an ideal process
	/// at the moment, and we are looking to make this experience better.) It is recommended to use the provided
	/// ztd::text::any_encoding type definition instead of accessing this directly, unless you have a reason for using
	/// a different byte type (e.g., interfacing with legacy APIs).
	//////
	template <typename _Byte, typename _CodePoint = unicode_code_point>
	class any_byte_encoding : public any_encoding_of<_Byte, const _CodePoint, const _Byte, _CodePoint> {
	private:
		using __base_t = any_encoding_of<_Byte, const _CodePoint, const _Byte, _CodePoint>;

	public:
		//////
		/// @brief Cannot default-construct a ztd::text::any_byte_encoding object.
		any_byte_encoding() = delete;

		//////
		/// @brief Constructs a ztd::text::any_byte_encoding with the encoding object and any additional arguments.
		///
		/// @param[in] __encoding The encoding object that informs the ztd::text::any_byte_encoding what encoding
		/// object to store.
		/// @param[in] __args Any additional arguments used to construct the encoding in the erased storage.
		///
		/// @remarks If the provided encoding does not have a byte code_unit type, it is wrapped in an
		/// ztd::text::encoding_scheme first.
		//////
		template <typename _Encoding, typename... _Args,
			::std::enable_if_t<
			     !::std::is_same_v<_Encoding,
			          any_byte_encoding> && !::std::is_same_v<_Encoding, __base_t> && !is_specialization_of_v<remove_cvref_t<_Encoding>, ::std::in_place_type_t>>* = nullptr>
		any_byte_encoding(_Encoding&& __encoding, _Args&&... __args)
		: any_byte_encoding(::std::in_place_type<remove_cvref_t<_Encoding>>, ::std::forward<_Encoding>(__encoding),
			::std::forward<_Args>(__args)...) {
		}

		//////
		/// @brief Constructs a ztd::text::any_byte_encoding with the encoding object and any additional arguments.
		///
		/// @tparam _Encoding The Encoding specified by the text_tag argument.
		///
		/// @param[in] __args Any additional arguments used to construct the encoding in the erased storage.
		///
		/// @remarks If the provided encoding does not have a byte code_unit type, it is wrapped in an
		/// ztd::text::encoding_scheme first.
		//////
		template <typename _Encoding, typename... _Args,
			::std::enable_if_t<!::std::is_same_v<_Byte, code_unit_t<remove_cvref_t<_Encoding>>>>* = nullptr>
		any_byte_encoding(::std::in_place_type_t<_Encoding>, _Args&&... __args)
		: __base_t(::std::in_place_type_t<encoding_scheme<remove_cvref_t<_Encoding>, endian::native, _Byte>> {},
			::std::forward<_Args>(__args)...) {
		}

		//////
		/// @brief Constructs a ztd::text::any_byte_encoding with the encoding object and any additional arguments.
		///
		/// @tparam _Encoding The Encoding specified by the text_tag argument.
		///
		/// @param[in] __tag A text_tag containing the encoding type.
		/// @param[in] __args Any additional arguments used to construct the encoding in the erased storage.
		///
		/// @remarks If the provided encoding does not have a byte code_unit type, it is wrapped in an
		/// ztd::text::encoding_scheme first.
		//////
		template <typename _Encoding, typename... _Args,
			::std::enable_if_t<::std::is_same_v<_Byte, code_unit_t<remove_cvref_t<_Encoding>>>>* = nullptr>
		any_byte_encoding(::std::in_place_type_t<_Encoding> __tag, _Args&&... __args)
		: __base_t(::std::move(__tag), ::std::forward<_Args>(__args)...) {
		}

		//////
		/// @brief Cannot copy-construct a ztd::text::any_byte_encoding object.
		any_byte_encoding(const any_byte_encoding&) = delete;

		//////
		/// @brief Cannot copy-assign a ztd::text::any_byte_encoding object.
		any_byte_encoding& operator=(const any_byte_encoding&) = delete;

		//////
		/// @brief Move-constructs a ztd::text::any_byte_encoding from the provided r-value reference.
		///
		/// @remarks This leaves the passed-in r-value reference without an encoding object. Calling any function on a
		/// moved-fron ztd::text::any_byte_encoding, except for destruction, is a violation and invokes Undefined
		/// Behavior (generally, a crash).
		//////
		any_byte_encoding(any_byte_encoding&&) = default;

		//////
		/// @brief Move-assigns a ztd::text::any_byte_encoding from the provided r-value reference.
		///
		/// @remarks This leaves the passed-in r-value reference without an encoding object. Calling any function on a
		/// moved-fron ztd::text::any_byte_encoding, except for destruction, is a violation and invokes Undefined
		/// Behavior (generally, a crash).
		//////
		any_byte_encoding& operator=(any_byte_encoding&&) = default;
	};

	//////
	/// @brief The canonical erased encoding type which uses a `std::byte` as its code unit type and an @c
	/// unicode_code_point as its code point type, with spans for input and output operations.
	///
	/// @remarks If the input encoding does not match `std::byte`, it will be first wrapped in a
	/// ztd::text::encoding_scheme first.
	//////
	using any_encoding = any_byte_encoding<::std::byte>;

	//////
	/// @}

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_ANY_ENCODING_HPP
