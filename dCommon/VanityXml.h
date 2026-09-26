#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/**
 * The vanity NPC files (build/vanity/*.xml), read by the world servers (VanityUtilities) and edited by the dashboard:
 *
 *   <files><file name="dev-tribute.xml" enabled="1"/></files>          (root.xml: which other files load)
 *   <objects><object name="..." lot="2279">
 *     <equipment>6802, 2519</equipment>
 *     <phrases><phrase>...</phrase></phrases>
 *     <config><key>custom_script_client=0:scripts\...</key></config>  (LDF: key=type:value)
 *     <locations><location zone="1200" x y z rw rx ry rz [chance] [scale]/></locations>
 *   </object></objects>
 *
 * Pure (text in, text out) so it can be unit tested.
 */
namespace VanityXml {
	struct Location {
		uint32_t zone{};
		float x{}, y{}, z{};
		float rw{ 1 }, rx{}, ry{}, rz{};
		std::optional<float> chance; // 0-1: how likely the NPC appears here
		std::optional<float> scale;
	};

	struct Object {
		std::string name;
		int32_t lot{ -1 }; // LOT_NULL when the file's lot isn't a number
		std::vector<int32_t> equipment;
		std::vector<std::string> phrases;
		std::vector<std::string> config; // LDF lines, as written
		std::vector<Location> locations; // only the complete ones
	};

	struct FileEntry {
		std::string name;
		bool enabled{};
	};

	struct Document {
		std::vector<FileEntry> files; // only root.xml has these, normally
		std::vector<Object> objects;
		std::vector<std::string> warnings; // parts that were skipped, e.g. a location without a position
	};

	// The file's includes and objects, or nullopt and error if it isn't XML
	std::optional<Document> Read(const std::string& xml, std::string& error);

	std::string Write(const Document& document);
}
