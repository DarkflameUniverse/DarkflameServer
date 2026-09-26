#include "EventParts.h"

#include <algorithm>
#include <cctype>

#include "magic_enum.hpp"

namespace {
	std::string Text(const nlohmann::json& json, const char* key) {
		return json.contains(key) && json[key].is_string() ? json[key].get<std::string>() : "";
	}
}

std::string EventParts::KindName(eKind kind) {
	std::string name(magic_enum::enum_name(kind));
	for (auto& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return name;
}

std::optional<EventParts::eKind> EventParts::KindOf(const std::string& name) {
	return magic_enum::enum_cast<eKind>(name, magic_enum::case_insensitive);
}

std::optional<std::vector<EventParts::Part>> EventParts::Parse(const nlohmann::json& json, std::string& error) {
	if (!json.is_array()) { error = "The parts are a list"; return std::nullopt; }
	std::vector<Part> parts;
	for (const auto& item : json) {
		if (!item.is_object()) { error = "Each part is an object"; return std::nullopt; }
		const auto kind = item.contains("kind") && item["kind"].is_string() ? KindOf(item["kind"].get<std::string>()) : std::nullopt;
		if (!kind) {
			std::string kinds;
			for (const auto value : magic_enum::enum_values<eKind>()) kinds += (kinds.empty() ? "" : ", ") + KindName(value);
			error = "A part's kind is one of " + kinds;
			return std::nullopt;
		}
		Part part{ .kind = *kind };
		if (item.contains("config") && item["config"].is_object()) part.config = item["config"];
		// true, or 1 as the migrations from before wrote it
		if (item.contains("applied")) part.applied = item["applied"].is_boolean() ? item["applied"].get<bool>() : item["applied"].is_number() && item["applied"].get<double>() != 0;
		if (item.contains("state") && item["state"].is_object()) part.state = item["state"];
		if (item.contains("status") && item["status"].is_string()) part.status = item["status"].get<std::string>();
		parts.push_back(std::move(part));
	}
	return parts;
}

std::optional<std::vector<EventParts::Part>> EventParts::Parse(const std::string& text, std::string& error) {
	if (text.find_first_not_of(" \t\r\n") == std::string::npos) return std::vector<Part>{};
	const auto json = nlohmann::json::parse(text, nullptr, false);
	if (json.is_discarded()) { error = "The parts aren't valid JSON"; return std::nullopt; }
	return Parse(json, error);
}

nlohmann::json EventParts::ToJson(const Part& part) {
	return { {"kind", KindName(part.kind)}, {"config", part.config}, {"applied", part.applied}, {"state", part.state}, {"status", part.status} };
}

std::string EventParts::Write(const std::vector<Part>& parts) {
	nlohmann::json json = nlohmann::json::array();
	for (const auto& part : parts) json.push_back(ToJson(part));
	return json.dump();
}

EventParts::eStep EventParts::StepFor(const Part& part, bool on) {
	if (on) return part.applied ? eStep::CONTINUE : eStep::START;
	return part.applied ? eStep::END : eStep::NONE;
}

bool EventParts::Momentary(eKind kind) {
	return kind == eKind::ANNOUNCEMENT || kind == eKind::RESTART;
}

EventParts::Reconciled EventParts::Reconcile(const std::vector<Part>& before, std::vector<Part> after) {
	Reconciled result;
	std::vector<bool> used(before.size()), matched(after.size());
	const auto take = [&](Part& part, size_t i) {
		used[i] = true;
		part.applied = before[i].applied;
		part.state = before[i].state;
		part.status = before[i].status;
	};
	for (auto& part : after) {
		part.applied = false;
		part.state = nlohmann::json::object();
		part.status.clear();
	}
	// The same part, then a changed momentary one in its place
	for (size_t a = 0; a < after.size(); a++) {
		for (size_t i = 0; i < before.size() && !matched[a]; i++) {
			if (!used[i] && before[i].kind == after[a].kind && before[i].config == after[a].config) { take(after[a], i); matched[a] = true; }
		}
	}
	for (size_t a = 0; a < after.size(); a++) {
		if (matched[a] || !Momentary(after[a].kind)) continue;
		for (size_t i = 0; i < before.size() && !matched[a]; i++) {
			if (!used[i] && before[i].kind == after[a].kind) { take(after[a], i); matched[a] = true; }
		}
	}
	for (size_t i = 0; i < before.size(); i++) {
		if (!used[i] && before[i].applied) result.retired.push_back(before[i]);
	}
	result.parts = std::move(after);
	return result;
}

std::vector<VanityEvents::Changes> EventParts::VanityChanges(const std::string& eventName, const std::string& text) {
	std::vector<VanityEvents::Changes> changes;
	std::string error;
	const auto parts = Parse(text, error);
	if (!parts) return changes;
	for (const auto& part : *parts) {
		if (part.kind != eKind::VANITY) continue;
		const auto& config = part.config;
		VanityEvents::Changes change{ .name = eventName };
		change.file = Text(config, "file");
		// Names one per line, or a list
		if (config.contains("removals") && config["removals"].is_array()) {
			for (const auto& name : config["removals"]) if (name.is_string()) change.removals += name.get<std::string>() + "\n";
		} else {
			change.removals = Text(config, "removals");
		}
		if (config.contains("fileSwitches")) change.fileSwitches = config["fileSwitches"].is_string() ? config["fileSwitches"].get<std::string>() : config["fileSwitches"].dump();
		changes.push_back(std::move(change));
	}
	return changes;
}
