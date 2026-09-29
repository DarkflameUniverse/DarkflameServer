#include "UgcToolboxProtocol.h"

namespace UgcToolboxProtocol {
	std::string Frame(nlohmann::json message) {
		message["dlutb"] = VERSION;
		// dump() escapes control characters, so a message never has a newline of its own
		return message.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace) + "\n";
	}

	nlohmann::json MakeRequest(uint64_t id, const std::string& input, const std::string& output, const std::vector<uint32_t>& lods) {
		return { { "cmd", "make" }, { "id", id }, { "input", input }, { "output", output }, { "lods", lods } };
	}

	void LineReader::Feed(std::string_view data) {
		// Drop what was read already before growing
		if (m_Start > 0 && m_Start >= m_Buffer.size() / 2) {
			m_Buffer.erase(0, m_Start);
			m_Start = 0;
		}
		m_Buffer.append(data);
	}

	std::optional<std::string> LineReader::Next() {
		while (true) {
			const auto end = m_Buffer.find('\n', m_Start);
			if (end == std::string::npos) {
				// A line longer than a message can be: dropped up to its newline
				if (Pending() > MAX_LINE) {
					if (!m_Skipping) m_Overflows++;
					m_Skipping = true;
					m_Buffer.clear();
					m_Start = 0;
				}
				return std::nullopt;
			}
			const auto start = m_Start;
			m_Start = end + 1;
			if (m_Skipping || end - start > MAX_LINE) {
				if (!m_Skipping) m_Overflows++;
				m_Skipping = false;
				continue;
			}
			auto line = m_Buffer.substr(start, end - start);
			if (!line.empty() && line.back() == '\r') line.pop_back();
			return line;
		}
	}

	std::optional<nlohmann::json> Parse(std::string_view line) {
		auto parsed = nlohmann::json::parse(line, nullptr, false);
		if (!parsed.is_object()) return std::nullopt;
		const auto version = parsed.find("dlutb");
		if (version == parsed.end() || !version->is_number_integer() || version->get<int>() != VERSION) return std::nullopt;
		return parsed;
	}

	eType TypeOf(const nlohmann::json& message) {
		const auto type = message.value("type", std::string());
		if (type == "ready") return eType::READY;
		if (type == "failed") return eType::FAILED;
		if (type == "done") return eType::DONE;
		if (type == "pong") return eType::PONG;
		return eType::OTHER;
	}

	std::optional<Done> ParseDone(const nlohmann::json& message) {
		if (TypeOf(message) != eType::DONE) return std::nullopt;
		const auto id = message.find("id");
		if (id == message.end() || !id->is_number_unsigned()) return std::nullopt;
		Done done;
		done.id = id->get<uint64_t>();
		done.ok = message.value("ok", false);
		done.error = message.contains("error") && message["error"].is_string() ? message["error"].get<std::string>() : std::string();
		if (!done.ok && done.error.empty()) done.error = "the worker gave no reason";
		if (const auto ms = message.find("ms"); ms != message.end() && ms->is_object()) {
			for (const auto& [name, value] : ms->items()) {
				if (value.is_number()) done.ms.emplace_back(name, value.get<double>());
			}
		}
		return done;
	}
}
