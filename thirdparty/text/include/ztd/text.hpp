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

#ifndef ZTD_TEXT_HPP
#define ZTD_TEXT_HPP

#include <ztd/text/version.hpp>

#include <ztd/text/forward.hpp>
#include <ztd/idk/charN_t.hpp>
#include <ztd/text/encoding.hpp>
#include <ztd/text/c_string_view.hpp>

#include <ztd/text/encode.hpp>
#include <ztd/text/decode.hpp>
#include <ztd/text/transcode.hpp>
#include <ztd/text/transcode_one.hpp>
#include <ztd/text/count_as_encoded.hpp>
#include <ztd/text/count_as_decoded.hpp>
#include <ztd/text/count_as_transcoded.hpp>
#include <ztd/text/validate_decodable_as.hpp>
#include <ztd/text/validate_encodable_as.hpp>
#include <ztd/text/validate_transcodable_as.hpp>

#include <ztd/text/encode_view.hpp>
#include <ztd/text/decode_view.hpp>
#include <ztd/text/transcode_view.hpp>

#include <ztd/text/text_view.hpp>
#include <ztd/text/text.hpp>

#endif // ZTD_TEXT_HPP
