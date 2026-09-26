#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "tinyxml2.h"
#include "eMissionState.h"
#include "eMissionTaskType.h"

/**
 * A character's missions in its saved XML, read and changed the way MissionComponent and Mission save them:
 * <mis><done><m state id cct (completions) cts (last completed)/></done>
 * <cur><m state o (journal order, missions only) id><sv v (task progress)/>[<sv v (collected/visited id)/>...]...</m></cur></mis>
 * A mission done before and taken again (dailies) is in both. Pure (no database or game data) so it can be unit tested.
 */
namespace MissionXml {
	// What the game data says about a mission, as far as saving it goes
	struct Definition {
		bool isMission{}; // false: an achievement
		bool repeatable{};
		std::vector<eMissionTaskType> tasks; // in task order
	};

	struct Task {
		uint32_t progress{};
		std::vector<uint32_t> uniques; // collected (zone << 8 | collectible id) or visited ids, for those task types
	};

	struct Entry {
		uint32_t id{};
		eMissionState state{ eMissionState::UNKNOWN };
		uint32_t completions{};
		uint32_t completedAt{};
		bool current{}; // in <cur>: accepted and not finished (again)
		uint32_t order{};
		std::vector<Task> tasks;

		bool Done() const { return completions > 0 || state == eMissionState::COMPLETE; }
	};

	namespace Detail {
		inline bool HasUniques(eMissionTaskType type) {
			return type == eMissionTaskType::COLLECTION || type == eMissionTaskType::VISIT_PROPERTY;
		}

		// Only the <mis> section is parsed, so reading every character stays cheap
		inline bool ParseSection(std::string_view xml, tinyxml2::XMLDocument& doc) {
			const auto start = xml.find("<mis");
			if (start == std::string_view::npos) return false;
			auto end = xml.find("</mis>", start);
			if (end != std::string_view::npos) end += 6;
			else if ((end = xml.find("/>", start)) != std::string_view::npos) end += 2; // <mis/>
			else return false;
			const std::string section(xml.substr(start, end - start));
			return doc.Parse(section.c_str(), section.size()) == tinyxml2::XML_SUCCESS;
		}

		inline tinyxml2::XMLElement* Child(tinyxml2::XMLElement* parent, const char* name, bool create) {
			auto* child = parent->FirstChildElement(name);
			if (!child && create) child = parent->InsertNewChildElement(name);
			return child;
		}

		inline tinyxml2::XMLElement* Find(tinyxml2::XMLElement* list, uint32_t id) {
			for (auto* m = list ? list->FirstChildElement("m") : nullptr; m; m = m->NextSiblingElement("m")) {
				if (m->UnsignedAttribute("id") == id) return m;
			}
			return nullptr;
		}
	}

	/**
	 * Every mission in the saved data. `definition` gives a mission's task types (for reading collected ids after a
	 * task's progress), or nullptr for one the game data doesn't have.
	 */
	template<typename Lookup>
	std::map<uint32_t, Entry> Read(std::string_view xml, Lookup&& definition) {
		std::map<uint32_t, Entry> entries;
		tinyxml2::XMLDocument doc;
		if (!Detail::ParseSection(xml, doc)) return entries;
		auto* mis = doc.FirstChildElement("mis");
		if (!mis) return entries;
		for (auto* m = mis->FirstChildElement("done") ? mis->FirstChildElement("done")->FirstChildElement("m") : nullptr; m; m = m->NextSiblingElement("m")) {
			auto& entry = entries[m->UnsignedAttribute("id")];
			entry.id = m->UnsignedAttribute("id");
			entry.state = static_cast<eMissionState>(m->IntAttribute("state", static_cast<int>(eMissionState::COMPLETE)));
			entry.completions = m->UnsignedAttribute("cct");
			entry.completedAt = m->UnsignedAttribute("cts");
		}
		for (auto* m = mis->FirstChildElement("cur") ? mis->FirstChildElement("cur")->FirstChildElement("m") : nullptr; m; m = m->NextSiblingElement("m")) {
			auto& entry = entries[m->UnsignedAttribute("id")];
			entry.id = m->UnsignedAttribute("id");
			entry.current = true;
			if (m->Attribute("state")) entry.state = static_cast<eMissionState>(m->IntAttribute("state", -1));
			entry.order = m->UnsignedAttribute("o");
			const Definition* info = definition(entry.id);
			// As Mission::LoadFromXmlCur: each task's progress, then for collection and property tasks that many ids
			size_t index = 0;
			for (auto* sv = m->FirstChildElement("sv"); sv; index++) {
				if (info && index >= info->tasks.size()) break;
				Task task;
				task.progress = sv->UnsignedAttribute("v");
				sv = sv->NextSiblingElement("sv");
				if (info && Detail::HasUniques(info->tasks[index])) {
					for (auto left = task.progress; sv && left > 0; left--) {
						task.uniques.push_back(sv->UnsignedAttribute("v"));
						sv = sv->NextSiblingElement("sv");
					}
				}
				entry.tasks.push_back(std::move(task));
			}
		}
		return entries;
	}

	enum class eChange : uint8_t {
		COMPLETE, // as /completemission: accept if needed, then complete (no rewards: those need the player in game)
		RESET,    // as /resetmission: forget the mission, so it can be taken again from the start
		ACCEPT,   // as /addmission: take the mission even if its prerequisites aren't done
	};

	/**
	 * Apply a change to a character that is not in game, leaving the XML as MissionComponent::UpdateXml would after
	 * doing the same in game. Returns the new XML (compact, as the game stores it), or nullopt and error.
	 */
	inline std::optional<std::string> Change(const std::string& xml, uint32_t id, const Definition& definition, eChange change, uint32_t now, std::string& error) {
		tinyxml2::XMLDocument doc;
		if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS || !doc.FirstChildElement("obj")) { error = "The character's saved data can't be read"; return std::nullopt; }
		auto* obj = doc.FirstChildElement("obj");
		auto* mis = Detail::Child(obj, "mis", true);
		auto* done = Detail::Child(mis, "done", true);
		auto* cur = Detail::Child(mis, "cur", true);
		auto* doneEntry = Detail::Find(done, id);
		auto* curEntry = Detail::Find(cur, id);
		const uint32_t completions = doneEntry ? doneEntry->UnsignedAttribute("cct") : 0;
		const bool complete = doneEntry && !curEntry;

		// A new <cur> entry: accepted with no progress, at the end of the journal (MissionComponent::AcceptMission)
		const auto accept = [&]() {
			if (!curEntry) {
				curEntry = cur->InsertNewChildElement("m");
				for (size_t i = 0; i < definition.tasks.size(); i++) curEntry->InsertNewChildElement("sv")->SetAttribute("v", 0);
			}
			if (definition.isMission) {
				uint32_t last = 0;
				for (auto* m = cur->FirstChildElement("m"); m; m = m->NextSiblingElement("m")) last = std::max(last, m->UnsignedAttribute("o"));
				curEntry->SetAttribute("o", last + 1);
			}
			curEntry->SetAttribute("state", static_cast<int>(completions > 0 ? eMissionState::COMPLETE_ACTIVE : eMissionState::ACTIVE));
			curEntry->SetAttribute("id", id);
		};

		switch (change) {
		case eChange::ACCEPT:
			// Only a repeatable mission is taken again; anything else it already has stays as it is
			if ((doneEntry || curEntry) && !definition.repeatable) { error = complete ? "The character already completed this" : "The character already has this"; return std::nullopt; }
			if (curEntry && !complete) { error = "The character already has this"; return std::nullopt; }
			accept();
			break;
		case eChange::RESET:
			if (!doneEntry && !curEntry) { error = "The character doesn't have this"; return std::nullopt; }
			if (doneEntry) done->DeleteChild(doneEntry);
			if (curEntry) cur->DeleteChild(curEntry);
			break;
		case eChange::COMPLETE:
			if (complete && !definition.repeatable) { error = "The character already completed this"; return std::nullopt; }
			// Mission::Complete: one more completion now; a finished mission keeps only its <done> entry
			if (!doneEntry) doneEntry = done->InsertNewChildElement("m");
			doneEntry->SetAttribute("state", static_cast<int>(eMissionState::COMPLETE));
			doneEntry->SetAttribute("id", id);
			doneEntry->SetAttribute("cct", completions + 1);
			doneEntry->SetAttribute("cts", now);
			if (curEntry) cur->DeleteChild(curEntry);
			break;
		}

		tinyxml2::XMLPrinter printer(nullptr, true);
		doc.Print(&printer);
		return std::string(printer.CStr());
	}
}
