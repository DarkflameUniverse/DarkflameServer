#ifndef __PACKETJSON__H__
#define __PACKETJSON__H__

#include <chrono>
#include <cstdint>
#include <string>
#include <type_traits>

#include "BitStream.h"
#include "BitStreamUtils.h"
#include "dCommonVars.h"
#include "Stamps.h"
#include "GeneralUtils.h"
#include "json.hpp"
#include "LDFFormat.h"
#include "magic_enum.hpp"
#include "NiPoint3.h"
#include "NiQuaternion.h"

/**
 * How a packet struct's members are shown in the capture viewer: one ToJson overload per member type. The member
 * lists are generated from the structs (tools/gen_game_message_fields.py); a member type without an overload here (or
 * in the file that includes the generated list) fails the build. Bring them in with `using PacketJson::ToJson;`.
 */
namespace PacketJson {
	using json = nlohmann::json;

	inline std::string Hex(std::string_view bytes) {
		static constexpr char digits[] = "0123456789abcdef";
		std::string out;
		out.reserve(bytes.size() * 2);
		for (const unsigned char c : bytes) {
			out += digits[c >> 4];
			out += digits[c & 15];
		}
		return out;
	}

	inline json ToJson(bool value) { return value; }
	inline json ToJson(float value) { return value; }
	inline json ToJson(double value) { return value; }
	// 64-bit values (object IDs) as strings: they don't fit in a JavaScript number
	inline json ToJson(int64_t value) { return std::to_string(value); }
	inline json ToJson(uint64_t value) { return std::to_string(value); }
	inline json ToJson(int32_t value) { return value; }
	inline json ToJson(uint32_t value) { return value; }
	inline json ToJson(int16_t value) { return value; }
	inline json ToJson(uint16_t value) { return value; }
	inline json ToJson(uint8_t value) { return value; }
	inline json ToJson(int8_t value) { return value; }
	inline json ToJson(char value) { return static_cast<int>(value); }
	inline json ToJson(const NiPoint3& point) { return json::array({ point.x, point.y, point.z }); }
	inline json ToJson(const NiQuaternion& rotation) { return json::array({ rotation.x, rotation.y, rotation.z, rotation.w }); }
	inline json ToJson(const std::u16string& text) { return GeneralUtils::UTF16ToWTF8(text); }

	// Text, or its bytes in hex when it isn't text (skill behavior streams are std::string too)
	inline json ToJson(const std::string& text) {
		for (const unsigned char c : text) {
			if (c < 0x20 && c != '\n' && c != '\r' && c != '\t') return json{ {"hex", Hex(text)} };
		}
		try {
			(void)json(text).dump(); // throws when it isn't UTF-8
		} catch (const json::exception&) {
			return json{ {"hex", Hex(text)} };
		}
		return text;
	}

	// Fixed-size strings: the text up to the first null
	// (Templates, so nothing converts to them: they have implicit constructors from a size)
	template<typename T> requires std::is_same_v<T, LUString>
	json ToJson(const T& text) { return ToJson(text.string.substr(0, text.string.find('\0'))); }
	template<typename T> requires std::is_same_v<T, LUWString>
	json ToJson(const T& text) { return ToJson(text.string.substr(0, text.string.find(u'\0'))); }

	inline json ToJson(const RakNet::BitStream& stream) {
		return json{ {"hex", Hex(std::string_view(reinterpret_cast<const char*>(stream.GetData()), stream.GetNumberOfBytesUsed()))} };
	}

	inline json ToJson(const LwoNameValue& config) {
		json out = json::array();
		for (const auto& [key, value] : config.values) {
			if (value) out.push_back(value->GetString());
		}
		return out;
	}

	inline json ToJson(const json& value) { return value; }

	inline json ToJson(const LWOZONEID& zone) {
		return json{ {"mapID", zone.GetMapID()}, {"instanceID", zone.GetInstanceID()}, {"cloneID", zone.GetCloneID()} };
	}

	inline json ToJson(const Stamps& stamps) {
		json out = json::array();
		for (const auto& stamp : stamps.list) {
			const auto name = magic_enum::enum_name(stamp.type);
			out.push_back({ {"type", name.empty() ? std::to_string(static_cast<int>(stamp.type)) : std::string(name)}, {"value", stamp.value}, {"timestamp", std::to_string(stamp.timestamp)} });
		}
		return out;
	}

	template<typename Clock, typename Duration>
	json ToJson(const std::chrono::time_point<Clock, Duration>& time) {
		return std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count());
	}

	// Enums by name, with the number when it has none
	template<typename E> requires std::is_enum_v<E>
	json ToJson(E value) {
		const auto name = magic_enum::enum_name(value);
		const auto number = static_cast<std::underlying_type_t<E>>(value);
		if (name.empty()) return number;
		return std::string(name) + " (" + std::to_string(number) + ")";
	}
}

#endif  //!__PACKETJSON__H__
