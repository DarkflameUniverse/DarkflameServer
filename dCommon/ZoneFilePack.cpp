#include "ZoneFilePack.h"

#include <cstring>

#include "Sd0.h"
#include "ZCompression.h"

namespace {
	using namespace ZoneFilePack;

	constexpr std::string_view SD0_HEADER("sd0\x01\xff", 5);
	// A chunk's compressed bytes are never much more than what it inflates to
	constexpr uint32_t MAX_COMPRESSED_CHUNK = Sd0::MAX_UNCOMPRESSED_CHUNK_SIZE + 1024;

	struct Reader {
		std::string_view bytes;
		size_t at = 0;

		size_t Left() const { return bytes.size() - at; }
		template<typename T>
		bool Read(T& value) {
			if (Left() < sizeof(T)) return false;
			std::memcpy(&value, bytes.data() + at, sizeof(T));
			at += sizeof(T);
			return true;
		}
		bool Take(size_t count, std::string_view& out) {
			if (Left() < count) return false;
			out = bytes.substr(at, count);
			at += count;
			return true;
		}
	};

	eError ReadEntry(Reader& reader, const Limits& limits, uint64_t& total, Entry& entry) {
		uint32_t nameBytes = 0, dataBytes = 0;
		if (!reader.Read(nameBytes) || !reader.Read(dataBytes)) return eError::TRUNCATED;
		std::string_view name, data;
		if (!reader.Take(nameBytes, name) || !reader.Take(dataBytes, data)) return eError::TRUNCATED;
		// UTF-16LE with its null: at least the null, an even length
		if (nameBytes < 2 || nameBytes % 2 != 0 || nameBytes > 2 * 1024) return eError::BAD_NAME;
		std::string text;
		for (size_t i = 0; i + 1 < name.size(); i += 2) {
			const auto low = static_cast<unsigned char>(name[i]);
			const auto high = static_cast<unsigned char>(name[i + 1]);
			const bool last = i + 2 == name.size();
			if (last) {
				if (low != 0 || high != 0) return eError::BAD_NAME;
				break;
			}
			if (high != 0 || low == 0) return eError::BAD_NAME;
			text += static_cast<char>(low);
		}
		const auto base = BaseName(text);
		if (!base) return eError::BAD_NAME;
		entry.name = *base;
		const auto room = limits.maxTotalBytes > total ? limits.maxTotalBytes - total : 0;
		const auto error = Inflate(data, std::min(limits.maxFileBytes, room), entry.data);
		total += entry.data.size();
		return error;
	}

	void WriteEntry(std::string& out, const Entry& entry) {
		const auto data = Sd0::Compress(entry.data);
		const auto nameBytes = static_cast<uint32_t>((entry.name.size() + 1) * 2);
		const auto dataBytes = static_cast<uint32_t>(data.size());
		out.append(reinterpret_cast<const char*>(&nameBytes), sizeof(nameBytes));
		out.append(reinterpret_cast<const char*>(&dataBytes), sizeof(dataBytes));
		for (const char c : entry.name) {
			out += c;
			out += '\0';
		}
		out.append(2, '\0');
		out += data;
	}
}

namespace ZoneFilePack {
	const char* Describe(eError error) {
		switch (error) {
		case eError::NONE: return "nothing";
		case eError::TRUNCATED: return "a length in it runs past its end, or bytes are left over";
		case eError::BAD_NAME: return "a file name in it can't be used";
		case eError::NOT_SD0: return "a file in it is not sd0";
		case eError::BAD_CHUNK: return "a file in it has a chunk that can't be inflated or is too large";
		case eError::TOO_LARGE: return "it is larger than this server takes";
		case eError::INCOMPLETE: return "a whole map needs its zone, its terrain and every scene";
		}
		return "unknown";
	}

	std::optional<std::string> BaseName(std::string_view name) {
		const auto slash = name.find_last_of("/\\");
		const auto base = slash == std::string_view::npos ? name : name.substr(slash + 1);
		if (base.empty() || base == "." || base == ".." || base.size() > 128) return std::nullopt;
		for (const char c : base) {
			const auto byte = static_cast<unsigned char>(c);
			if (byte < 0x20 || byte >= 0x7F || std::string_view(":*?\"<>|").find(c) != std::string_view::npos) return std::nullopt;
		}
		if (base.front() == ' ' || base.back() == ' ' || base.back() == '.') return std::nullopt;
		return std::string(base);
	}

	eError Inflate(std::string_view sd0, uint64_t maxBytes, std::string& out) {
		out.clear();
		if (!sd0.starts_with(SD0_HEADER)) return eError::NOT_SD0;
		Reader reader{ sd0, SD0_HEADER.size() };
		std::string chunk(Sd0::MAX_UNCOMPRESSED_CHUNK_SIZE, '\0');
		while (reader.Left() > 0) {
			uint32_t size = 0;
			std::string_view compressed;
			if (!reader.Read(size)) return eError::TRUNCATED;
			if (size > MAX_COMPRESSED_CHUNK) return eError::BAD_CHUNK;
			if (!reader.Take(size, compressed)) return eError::TRUNCATED;
			int32_t zlibError = 0;
			// A chunk that inflates to more than the room it is given fails here
			const auto inflated = ZCompression::Decompress(reinterpret_cast<const uint8_t*>(compressed.data()), size,
				reinterpret_cast<uint8_t*>(chunk.data()), static_cast<uint32_t>(chunk.size()), zlibError);
			if (inflated < 0 || zlibError < 0) return eError::BAD_CHUNK;
			if (out.size() + static_cast<uint64_t>(inflated) > maxBytes) return eError::TOO_LARGE;
			out.append(chunk.data(), static_cast<size_t>(inflated));
		}
		return eError::NONE;
	}

	eError Unpack(eFileType type, std::string_view body, const Limits& limits, Files& out) {
		out = {};
		Reader reader{ body };
		uint64_t total = 0;
		const auto entry = [&](Entry& target) { return ReadEntry(reader, limits, total, target); };
		if (type != eFileType::WHOLE_MAP) {
			Entry single;
			if (const auto error = entry(single); error != eError::NONE) return error;
			if (reader.Left() != 0) return eError::TRUNCATED;
			switch (type) {
			case eFileType::SCENE: out.scenes.push_back(std::move(single)); break;
			case eFileType::TERRAIN: out.terrain = std::move(single); break;
			case eFileType::ZONE: out.zone = std::move(single); break;
			case eFileType::TRIGGERS: out.triggers.push_back(std::move(single)); break;
			default: return eError::TRUNCATED;
			}
			return eError::NONE;
		}

		out.zone.emplace();
		if (const auto error = entry(*out.zone); error != eError::NONE) return error;
		out.terrain.emplace();
		if (const auto error = entry(*out.terrain); error != eError::NONE) return error;
		for (auto* list : { &out.scenes, &out.triggers }) {
			uint16_t count = 0;
			if (!reader.Read(count)) return eError::TRUNCATED;
			if (out.Count() + count > limits.maxFiles) return eError::TOO_LARGE;
			for (uint16_t i = 0; i < count; ++i) {
				if (const auto error = entry(list->emplace_back()); error != eError::NONE) return error;
			}
		}
		if (reader.Left() != 0) return eError::TRUNCATED;
		// As the client takes a whole map: only with a zone, a terrain and every scene having data
		if (out.zone->data.empty() || out.terrain->data.empty()) return eError::INCOMPLETE;
		for (const auto& scene : out.scenes) if (scene.data.empty()) return eError::INCOMPLETE;
		return eError::NONE;
	}

	std::string Pack(eFileType type, const Files& files) {
		std::string out;
		switch (type) {
		case eFileType::SCENE: if (!files.scenes.empty()) WriteEntry(out, files.scenes.front()); return out;
		case eFileType::TERRAIN: if (files.terrain) WriteEntry(out, *files.terrain); return out;
		case eFileType::ZONE: if (files.zone) WriteEntry(out, *files.zone); return out;
		case eFileType::TRIGGERS: if (!files.triggers.empty()) WriteEntry(out, files.triggers.front()); return out;
		case eFileType::WHOLE_MAP: break;
		}
		if (!files.zone || !files.terrain) return out;
		WriteEntry(out, *files.zone);
		WriteEntry(out, *files.terrain);
		for (const auto* list : { &files.scenes, &files.triggers }) {
			const auto count = static_cast<uint16_t>(list->size());
			out.append(reinterpret_cast<const char*>(&count), sizeof(count));
			for (const auto& entry : *list) WriteEntry(out, entry);
		}
		return out;
	}
}
