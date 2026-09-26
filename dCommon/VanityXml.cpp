#include "VanityXml.h"

#include <cctype>
#include <charconv>

#include "tinyxml2.h"
#include "GeneralUtils.h"

namespace {
	std::string Number(float value) {
		char buffer[32];
		const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
		return std::string(buffer, result.ptr);
	}

	std::optional<float> Float(const tinyxml2::XMLElement* element, const char* name) {
		const char* value = element->Attribute(name);
		return value ? GeneralUtils::TryParse<float>(value) : std::nullopt;
	}
}

std::optional<VanityXml::Document> VanityXml::Read(const std::string& xml, std::string& error) {
	tinyxml2::XMLDocument doc;
	if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) {
		error = std::string("Not valid XML: ") + doc.ErrorStr();
		return std::nullopt;
	}
	Document out;
	if (const auto* files = doc.FirstChildElement("files")) {
		for (const auto* file = files->FirstChildElement("file"); file; file = file->NextSiblingElement("file")) {
			const char* name = file->Attribute("name");
			const char* enabled = file->Attribute("enabled");
			if (name) out.files.push_back({ name, enabled && std::string(enabled) == "1" });
		}
	}
	if (const auto* objects = doc.FirstChildElement("objects")) {
		for (const auto* element = objects->FirstChildElement("object"); element; element = element->NextSiblingElement("object")) {
			Object object;
			object.name = element->Attribute("name") ? element->Attribute("name") : "";
			object.lot = GeneralUtils::TryParse<int32_t>(element->Attribute("lot") ? element->Attribute("lot") : "").value_or(-1);
			const auto who = "\"" + object.name + "\"";
			if (const auto* equipment = element->FirstChildElement("equipment"); equipment && equipment->GetText()) {
				for (auto item : GeneralUtils::SplitString(equipment->GetText(), ',')) {
					// remove spaces for TryParse to work
					std::erase_if(item, [](char c) { return std::isspace(static_cast<unsigned char>(c)); });
					if (const auto lot = GeneralUtils::TryParse<uint32_t>(item)) object.equipment.push_back(static_cast<int32_t>(*lot));
				}
			}
			if (const auto* phrases = element->FirstChildElement("phrases")) {
				for (const auto* phrase = phrases->FirstChildElement("phrase"); phrase; phrase = phrase->NextSiblingElement("phrase")) {
					if (phrase->GetText()) object.phrases.push_back(phrase->GetText());
					else out.warnings.push_back(who + ": an empty phrase");
				}
			}
			if (const auto* config = element->FirstChildElement("config")) {
				for (const auto* key = config->FirstChildElement("key"); key; key = key->NextSiblingElement("key")) {
					if (key->GetText()) object.config.push_back(key->GetText());
				}
			}
			if (const auto* locations = element->FirstChildElement("locations")) {
				for (const auto* l = locations->FirstChildElement("location"); l; l = l->NextSiblingElement("location")) {
					const auto zone = GeneralUtils::TryParse<uint32_t>(l->Attribute("zone") ? l->Attribute("zone") : "");
					const auto x = Float(l, "x"), y = Float(l, "y"), z = Float(l, "z");
					const auto rw = Float(l, "rw"), rx = Float(l, "rx"), ry = Float(l, "ry"), rz = Float(l, "rz");
					if (!zone || !x || !y || !z || !rw || !rx || !ry || !rz) {
						out.warnings.push_back(who + ": a location without a zone, position or rotation");
						continue;
					}
					object.locations.push_back({ *zone, *x, *y, *z, *rw, *rx, *ry, *rz, Float(l, "chance"), Float(l, "scale") });
				}
			} else {
				out.warnings.push_back(who + ": no locations");
			}
			out.objects.push_back(std::move(object));
		}
	}
	return out;
}

std::string VanityXml::Write(const Document& document) {
	tinyxml2::XMLDocument doc;
	if (!document.files.empty()) {
		auto* files = doc.NewElement("files");
		for (const auto& file : document.files) {
			auto* element = files->InsertNewChildElement("file");
			element->SetAttribute("name", file.name.c_str());
			element->SetAttribute("enabled", file.enabled ? "1" : "0");
		}
		doc.InsertEndChild(files);
	}
	if (!document.objects.empty() || document.files.empty()) {
		auto* objects = doc.NewElement("objects");
		for (const auto& object : document.objects) {
			auto* element = objects->InsertNewChildElement("object");
			element->SetAttribute("name", object.name.c_str());
			element->SetAttribute("lot", object.lot);
			if (!object.equipment.empty()) {
				std::string text;
				for (const auto lot : object.equipment) text += (text.empty() ? "" : ", ") + std::to_string(lot);
				element->InsertNewChildElement("equipment")->SetText(text.c_str());
			}
			if (!object.phrases.empty()) {
				auto* phrases = element->InsertNewChildElement("phrases");
				for (const auto& phrase : object.phrases) phrases->InsertNewChildElement("phrase")->SetText(phrase.c_str());
			}
			if (!object.config.empty()) {
				auto* config = element->InsertNewChildElement("config");
				for (const auto& key : object.config) config->InsertNewChildElement("key")->SetText(key.c_str());
			}
			auto* locations = element->InsertNewChildElement("locations");
			for (const auto& l : object.locations) {
				auto* location = locations->InsertNewChildElement("location");
				location->SetAttribute("zone", l.zone);
				for (const auto& [name, value] : { std::pair{ "x", l.x }, { "y", l.y }, { "z", l.z }, { "rw", l.rw }, { "rx", l.rx }, { "ry", l.ry }, { "rz", l.rz } }) {
					location->SetAttribute(name, Number(value).c_str());
				}
				if (l.chance) location->SetAttribute("chance", Number(*l.chance).c_str());
				if (l.scale) location->SetAttribute("scale", Number(*l.scale).c_str());
			}
		}
		doc.InsertEndChild(objects);
	}
	tinyxml2::XMLPrinter printer;
	doc.Print(&printer);
	return printer.CStr();
}
