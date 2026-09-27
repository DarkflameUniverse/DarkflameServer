#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/**
 * The Prometheus text exposition format (version 0.0.4), and the checks /metrics makes before answering.
 * Pure so it can be unit tested.
 *
 * A Writer collects samples in any order and writes each metric family together, once, under its HELP and TYPE lines:
 *   Writer w;
 *   w.Add("darkflame_zone_players", "Players in a zone", "gauge", { {"zone_id", "1100"} }, 12);
 *   w.Text();
 */
namespace MetricsFormat {
	using Labels = std::vector<std::pair<std::string, std::string>>;

	// Metric and label names are [a-zA-Z_:][a-zA-Z0-9_:]* (labels without ':'); anything else becomes '_'
	inline std::string Name(std::string_view name, bool label = false) {
		std::string out(name);
		for (size_t i = 0; i < out.size(); i++) {
			const char c = out[i];
			const bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (c == ':' && !label);
			if (!letter && !(i > 0 && c >= '0' && c <= '9')) out[i] = '_';
		}
		return out.empty() ? "_" : out;
	}

	// Label values escape backslash, double quote and line feed
	inline std::string EscapeLabelValue(std::string_view value) {
		std::string out;
		out.reserve(value.size());
		for (const char c : value) {
			if (c == '\\') out += "\\\\";
			else if (c == '"') out += "\\\"";
			else if (c == '\n') out += "\\n";
			else out += c;
		}
		return out;
	}

	// HELP text escapes backslash and line feed (quotes stay as they are)
	inline std::string EscapeHelp(std::string_view help) {
		std::string out;
		out.reserve(help.size());
		for (const char c : help) {
			if (c == '\\') out += "\\\\";
			else if (c == '\n') out += "\\n";
			else out += c;
		}
		return out;
	}

	// Whole numbers without a decimal point; NaN, +Inf and -Inf as Prometheus spells them
	inline std::string Value(double value) {
		if (std::isnan(value)) return "NaN";
		if (std::isinf(value)) return value > 0 ? "+Inf" : "-Inf";
		if (value == std::floor(value) && std::fabs(value) < 9007199254740992.0) return std::to_string(static_cast<int64_t>(value));
		char buffer[32];
		const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
		return std::string(buffer, result.ptr);
	}

	class Writer {
	public:
		// type: counter or gauge. A counter's name should end in _total.
		void Add(const std::string& name, const std::string& help, const std::string& type, const Labels& labels, double value) {
			AddSample(name, name, help, type, labels, value);
		}

		/**
		 * A sample named differently from its family, for histograms: the family "x_seconds" (type histogram) holds
		 * x_seconds_bucket{le=...}, x_seconds_sum and x_seconds_count.
		 */
		void AddSample(const std::string& family, const std::string& sample, const std::string& help, const std::string& type, const Labels& labels, double value) {
			const auto metric = Name(family);
			auto it = m_Index.find(metric);
			if (it == m_Index.end()) {
				it = m_Index.emplace(metric, m_Families.size()).first;
				m_Families.push_back({ metric, help, type, {} });
			}
			std::string line = Name(sample);
			if (!labels.empty()) {
				line += '{';
				for (size_t i = 0; i < labels.size(); i++) {
					if (i) line += ',';
					line += Name(labels[i].first, true) + "=\"" + EscapeLabelValue(labels[i].second) + '"';
				}
				line += '}';
			}
			m_Families[it->second].samples.push_back(line + ' ' + Value(value));
		}

		void Add(const std::string& name, const std::string& help, const std::string& type, double value) {
			Add(name, help, type, {}, value);
		}

		// A family with no samples (e.g. no worlds running) still gets its HELP and TYPE, so dashboards know it
		void Declare(const std::string& name, const std::string& help, const std::string& type) {
			const auto metric = Name(name);
			if (m_Index.contains(metric)) return;
			m_Index.emplace(metric, m_Families.size());
			m_Families.push_back({ metric, help, type, {} });
		}

		std::string Text() const {
			std::string out;
			for (const auto& family : m_Families) {
				out += "# HELP " + family.name + ' ' + EscapeHelp(family.help) + '\n';
				out += "# TYPE " + family.name + ' ' + family.type + '\n';
				for (const auto& sample : family.samples) out += sample + '\n';
			}
			return out;
		}

	private:
		struct Family {
			std::string name;
			std::string help;
			std::string type;
			std::vector<std::string> samples;
		};
		std::vector<Family> m_Families;
		std::map<std::string, size_t> m_Index;
	};

	// Compares without stopping at the first difference, so response times don't give a secret away a byte at a time
	inline bool ConstantTimeEquals(std::string_view a, std::string_view b) {
		unsigned char difference = a.size() == b.size() ? 0 : 1;
		const size_t length = std::max(a.size(), b.size());
		for (size_t i = 0; i < length; i++) {
			const unsigned char x = i < a.size() ? static_cast<unsigned char>(a[i]) : 0;
			const unsigned char y = i < b.size() ? static_cast<unsigned char>(b[i]) : 0;
			difference |= x ^ y;
		}
		return difference == 0;
	}

	namespace Detail {
		inline std::optional<uint32_t> Ipv4(std::string_view text) {
			uint32_t address = 0;
			int parts = 0;
			while (parts < 4) {
				uint32_t part = 0;
				const auto result = std::from_chars(text.data(), text.data() + text.size(), part);
				if (result.ec != std::errc() || part > 255 || result.ptr == text.data()) return std::nullopt;
				address = (address << 8) | part;
				text.remove_prefix(result.ptr - text.data());
				parts++;
				if (parts < 4) {
					if (text.empty() || text.front() != '.') return std::nullopt;
					text.remove_prefix(1);
				}
			}
			if (!text.empty()) return std::nullopt;
			return address;
		}
	}

	/**
	 * Whether an address is in a comma separated allow-list of addresses and IPv4 ranges ("127.0.0.1, 10.0.0.0/8, ::1").
	 * An empty list allows everyone. IPv6 addresses only match exactly.
	 */
	inline bool AddressAllowed(std::string_view list, std::string_view address) {
		bool any = false;
		while (!list.empty()) {
			const auto comma = list.find(',');
			auto entry = list.substr(0, comma);
			list = comma == std::string_view::npos ? std::string_view{} : list.substr(comma + 1);
			while (!entry.empty() && entry.front() == ' ') entry.remove_prefix(1);
			while (!entry.empty() && entry.back() == ' ') entry.remove_suffix(1);
			if (entry.empty()) continue;
			any = true;
			const auto slash = entry.find('/');
			if (slash == std::string_view::npos) {
				if (entry == address) return true;
				continue;
			}
			const auto network = Detail::Ipv4(entry.substr(0, slash));
			const auto ip = Detail::Ipv4(address);
			uint32_t bits = 0;
			const auto prefix = entry.substr(slash + 1);
			const auto parsed = std::from_chars(prefix.data(), prefix.data() + prefix.size(), bits);
			if (!network || !ip || prefix.empty() || parsed.ec != std::errc() || parsed.ptr != prefix.data() + prefix.size() || bits > 32) continue;
			const uint32_t mask = bits == 0 ? 0 : ~uint32_t{ 0 } << (32 - bits);
			if ((*network & mask) == (*ip & mask)) return true;
		}
		return !any;
	}
}
