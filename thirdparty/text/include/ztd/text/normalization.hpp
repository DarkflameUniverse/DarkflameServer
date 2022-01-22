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

#ifndef ZTD_TEXT_NORMALIZATION_HPP
#define ZTD_TEXT_NORMALIZATION_HPP

#include <ztd/text/version.hpp>

#include <ztd/prologue.hpp>

namespace ztd { namespace text {
	ZTD_TEXT_INLINE_ABI_NAMESPACE_OPEN_I_

	namespace __txt_impl {
		//////
		/// @internal
		///
		/// @brief The Form C Normalization (Composed), which can be configured to be compatible or not using the @p
		/// _IsCompatibility template boolean parameter.
		///
		/// @tparam _IsCompatibility Whether or not this Normalization Form is NFKC (Compatibility Composed), or NFC
		/// (Canonical Composed).
		///
		/// @remarks From from Unicode Technical Report #5.
		//////
		template <bool _IsCompatibility>
		struct __nfc { };

		//////
		/// @internal
		///
		/// @brief The Form D Normalization (Decomposed), which can be configured to be compatible or not using the @p
		/// _IsCompatibility template boolean parameter.
		///
		/// @tparam _IsCompatibility Whether or not this Normalization Form is NFKD (Compatibility Decomposed), or NFD
		/// (Canonical Decomposed).
		///
		/// @remarks From from Unicode Technical Report #5.
		//////
		template <bool _IsCompatibility>
		struct __nfd { };
	} // namespace __txt_impl


	//////
	/// @brief The Normalization Form C (Canonical, Composed, from Unicode Technical Report #5).
	class nfc : public __txt_impl::__nfc<false> { };
	//////
	/// @brief The Normalization Form D (Canonical, Decomposed, from Unicode Technical Report #5).
	class nfd : public __txt_impl::__nfd<false> { };
	//////
	/// @brief The Normalization Form KC (Compatibility, Composed, from Unicode Technical Report #5).
	class nfkc : public __txt_impl::__nfc<true> { };
	//////
	/// @brief The Normalization Form KD (Compatibility, Decomposed, from Unicode Technical Report #5).
	class nfkd : public __txt_impl::__nfd<true> { };
	//////
	/// @brief The Normalization Form C, Stream-Safe (Fast Composed, Contiguous, from Unicode Technical Report #5).
	class fcc { };

	ZTD_TEXT_INLINE_ABI_NAMESPACE_CLOSE_I_
}} // namespace ztd::text

#include <ztd/epilogue.hpp>

#endif // ZTD_TEXT_NORMALIZATION_HPP
