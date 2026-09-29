#pragma once

#include <functional>
#include <string>

// Text the game stores with locale keys in it, as the client shows it
namespace LocaleText {
	/**
	 * Every %[KEY] in `text` replaced with the phrase `lookup` gives for KEY (the client's locale.xml); a key without a
	 * phrase stays as written. Mail from the game is stored this way ("%[MissionEmail_12_subjectText]"). Pure; unit tested.
	 */
	inline std::string Expand(const std::string& text, const std::function<const std::string&(const std::string&)>& lookup) {
		std::string out;
		size_t pos = 0;
		while (pos < text.size()) {
			const auto start = text.find("%[", pos);
			const auto end = start == std::string::npos ? std::string::npos : text.find(']', start + 2);
			if (end == std::string::npos) break;
			out.append(text, pos, start - pos);
			const auto& phrase = lookup(text.substr(start + 2, end - start - 2));
			if (phrase.empty()) out.append(text, start, end - start + 1);
			else out += phrase;
			pos = end + 1;
		}
		out.append(text, pos, std::string::npos);
		return out;
	}
}
