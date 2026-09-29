#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "json.hpp"

/**
 * The messages between the UGC server and its LU Toolbox worker (dlu_toolbox_worker.py, running in Blender): one JSON
 * object per line, each with "dlutb": VERSION. Pure, so the framing is tested without Blender.
 */
namespace UgcToolboxProtocol {
	inline constexpr int VERSION = 1;
	// A line longer than this is not a message (the worker's replies are a few hundred bytes)
	inline constexpr size_t MAX_LINE = 1024 * 1024;

	// A message as its line: compact JSON with "dlutb" set, and a newline
	std::string Frame(nlohmann::json message);

	// The request to make one model: `input` the LXFML file, `output` the .nif to write, `lods` the brickprimitives
	// levels to import (LU Toolbox's importLOD0..3)
	nlohmann::json MakeRequest(uint64_t id, const std::string& input, const std::string& output, const std::vector<uint32_t>& lods);

	/**
	 * Splits what is read from the worker into lines, however it arrives (parts of lines, many at once). A line longer
	 * than MAX_LINE is dropped up to its newline and counted in Overflows.
	 */
	class LineReader {
	public:
		void Feed(std::string_view data);
		// The next whole line without its newline (and a '\r' before it), or nullopt until one has arrived
		std::optional<std::string> Next();
		size_t Pending() const { return m_Buffer.size() - m_Start; }
		size_t Overflows() const { return m_Overflows; }

	private:
		std::string m_Buffer;
		size_t m_Start{};
		bool m_Skipping{}; // inside a line that was too long
		size_t m_Overflows{};
	};

	// A line as a message: a JSON object with this "dlutb" version; nullopt for anything else (blank lines too)
	std::optional<nlohmann::json> Parse(std::string_view line);

	enum class eType { READY, FAILED, DONE, PONG, OTHER };
	eType TypeOf(const nlohmann::json& message);

	// A "done" reply: whether it made the model, why not, and its steps' times in milliseconds
	struct Done {
		uint64_t id{};
		bool ok{};
		std::string error;
		std::vector<std::pair<std::string, double>> ms; // reset, import, process, bake, export
	};
	std::optional<Done> ParseDone(const nlohmann::json& message);
}
