#include "MultipartFile.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace {
	using namespace MultipartFile;

	std::string Lower(std::string_view text) {
		std::string out(text);
		std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return out;
	}

	// The value of name="..." in a header line; nothing is unescaped (a name is checked as it was sent)
	std::optional<std::string> Parameter(std::string_view header, std::string_view name) {
		const auto lower = Lower(header);
		const auto key = std::string(name) + "=\"";
		size_t pos = 0;
		while ((pos = lower.find(key, pos)) != std::string::npos) {
			// A whole parameter name: "name=" must not match inside "filename="
			if (pos == 0 || lower[pos - 1] == ' ' || lower[pos - 1] == ';' || lower[pos - 1] == '\t') {
				const auto start = pos + key.size();
				const auto end = header.find('"', start);
				if (end == std::string_view::npos) return std::nullopt;
				return std::string(header.substr(start, end - start));
			}
			pos += key.size();
		}
		return std::nullopt;
	}

	bool ParseHeaders(std::string_view headers, Part& part) {
		size_t start = 0;
		bool found = false;
		while (start < headers.size()) {
			const auto end = std::min(headers.find("\r\n", start), headers.size());
			const auto line = headers.substr(start, end - start);
			if (Lower(line.substr(0, 20)).starts_with("content-disposition:")) {
				const auto name = Parameter(line, "name");
				if (!name) return false;
				part.name = *name;
				if (const auto filename = Parameter(line, "filename")) {
					part.filename = *filename;
					part.isFile = true;
				}
				found = true;
			}
			start = end + 2;
		}
		return found;
	}
}

namespace MultipartFile {
	std::optional<std::string> Boundary(std::string_view contentType) {
		const auto lower = Lower(contentType);
		if (!lower.starts_with("multipart/form-data")) return std::nullopt;
		const auto at = lower.find("boundary=");
		if (at == std::string::npos) return std::nullopt;
		auto value = contentType.substr(at + 9);
		value = value.substr(0, value.find(';'));
		while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
		if (value.size() >= 2 && value.front() == '"' && value.back() == '"') value = value.substr(1, value.size() - 2);
		// RFC 2046: 1 to 70 characters
		if (value.empty() || value.size() > 70 || value.find_first_of("\r\n\"") != std::string_view::npos) return std::nullopt;
		return std::string(value);
	}

	eError Split(const std::filesystem::path& body, const std::string& boundary, const std::filesystem::path& outDir, const Limits& limits, std::vector<Part>& parts) {
		std::ifstream in(body, std::ios::binary);
		if (!in) return eError::IO;
		const std::string delimiter = "\r\n--" + boundary;
		// The first boundary has no line break before it
		std::string buffer = "\r\n";
		std::vector<char> chunk(256 * 1024);
		bool ended = false;
		const auto fill = [&] {
			if (ended) return false;
			in.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
			const auto got = in.gcount();
			if (got <= 0) { ended = true; return false; }
			buffer.append(chunk.data(), static_cast<size_t>(got));
			return true;
		};

		enum class eState { PREAMBLE, AFTER_BOUNDARY, HEADERS, DATA, DONE } state = eState::PREAMBLE;
		Part part;
		std::ofstream out;
		const auto take = [&](std::string_view bytes) -> eError {
			if (state != eState::DATA || bytes.empty()) return eError::NONE;
			if (!part.isFile) {
				if (part.value.size() + bytes.size() > limits.maxFieldBytes) return eError::FIELD_TOO_LARGE;
				part.value.append(bytes);
				return eError::NONE;
			}
			part.bytes += bytes.size();
			if (part.tooLarge) return eError::NONE;
			if (part.bytes > limits.maxFileBytes) {
				part.tooLarge = true;
				return eError::NONE;
			}
			out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
			return out ? eError::NONE : eError::IO;
		};

		while (state != eState::DONE) {
			if (state == eState::PREAMBLE || state == eState::DATA) {
				const auto at = buffer.find(delimiter);
				if (at == std::string::npos) {
					// Everything but a tail that could be the start of a boundary is the part's
					const auto keep = std::min(buffer.size(), delimiter.size() - 1);
					if (const auto error = take(std::string_view(buffer).substr(0, buffer.size() - keep)); error != eError::NONE) return error;
					buffer.erase(0, buffer.size() - keep);
					if (!fill()) return eError::MALFORMED;
					continue;
				}
				if (const auto error = take(std::string_view(buffer).substr(0, at)); error != eError::NONE) return error;
				buffer.erase(0, at + delimiter.size());
				if (state == eState::DATA) {
					if (part.isFile) {
						out.flush();
						if (!out) return eError::IO;
						out.close();
					}
					parts.push_back(std::move(part));
					part = {};
				}
				state = eState::AFTER_BOUNDARY;
			} else if (state == eState::AFTER_BOUNDARY) {
				if (buffer.size() < 2) {
					if (!fill()) return eError::MALFORMED;
					continue;
				}
				if (buffer.starts_with("--")) state = eState::DONE;
				else if (buffer.starts_with("\r\n")) {
					buffer.erase(0, 2);
					state = eState::HEADERS;
				} else return eError::MALFORMED;
			} else if (state == eState::HEADERS) {
				const auto end = buffer.find("\r\n\r\n");
				if (end == std::string::npos) {
					if (buffer.size() > limits.maxHeaderBytes || !fill()) return eError::MALFORMED;
					continue;
				}
				if (end > limits.maxHeaderBytes) return eError::MALFORMED;
				if (parts.size() >= limits.maxParts) return eError::TOO_MANY_PARTS;
				if (!ParseHeaders(std::string_view(buffer).substr(0, end), part)) return eError::MALFORMED;
				buffer.erase(0, end + 4);
				if (part.isFile) {
					part.file = outDir / ("part-" + std::to_string(parts.size()));
					out.open(part.file, std::ios::binary | std::ios::trunc);
					if (!out) return eError::IO;
				}
				state = eState::DATA;
			}
		}
		return in.bad() ? eError::IO : eError::NONE;
	}
}
