#include "CaptureProperty.h"

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <tuple>

#include "BrickByBrick.h"
#include "CaptureTools.h"
#include "GeneralUtils.h"
#include "MessageIdentifiers.h"
#include "PacketDecoder.h"
#include "PropertyMessages.h"

namespace {
	using json = nlohmann::json;
	using Record = CaptureBundle::Record;

	constexpr const char* GAME_MSG = "GAME_MSG ";

	struct Span {
		double from{};
		std::optional<double> to;
		size_t index{};
		json position;
		json rotation;
	};

	struct Model {
		LWOOBJID object{};
		LOT lot{};
		std::string spawner;
		std::string ugcId;
		json behaviors;
		std::vector<Span> spans;
		bool present{};
		bool placed{}; // a placement in the capture made it
	};

	struct Placement {
		double t{};
		size_t index{};
		json position;
		json rotation;
	};

	struct World {
		uint32_t zone{};
		uint32_t instance{};
		uint32_t clone{};
		double t{};
		bool property{};
		json info = json::array();
		json counts = json::array();
		json events = json::array();
		std::vector<Model> models;
		std::map<LWOOBJID, size_t> byObject; // object -> its entry in models (the latest, if it was made again)
		std::vector<Placement> placements;   // placed, their model not made yet
		std::map<LWOOBJID, std::pair<size_t, int32_t>> deletes; // model -> the DeleteModelFromClient (record, reason) asking for it
	};

	const json* ComponentFields(const json& fields, const char* name) {
		const auto it = fields.find("components");
		if (it == fields.end() || !it->is_array()) return nullptr;
		for (const auto& component : *it) {
			if (component.value("component", std::string{}) == name) {
				const auto found = component.find("fields");
				return found == component.end() ? nullptr : &*found;
			}
		}
		return nullptr;
	}

	// Where the object stands: the first component that says (its physics component)
	std::optional<std::pair<json, json>> Placed(const json& fields) {
		const auto it = fields.find("components");
		if (it == fields.end() || !it->is_array()) return std::nullopt;
		for (const auto& component : *it) {
			const auto found = component.find("fields");
			if (found == component.end() || !found->is_object()) continue;
			const auto position = found->find("position"), rotation = found->find("rotation");
			if (position != found->end() && position->is_array() && position->size() == 3 && rotation != found->end() && rotation->is_array() && rotation->size() == 4) {
				return std::pair{ *position, *rotation };
			}
		}
		return std::nullopt;
	}

	// A model's component: the plain one, or the mutable one (with behaviors) property and inventory models get
	const json* ModelFields(const json& fields) {
		const auto* model = ComponentFields(fields, "MUTABLE_MODEL_BEHAVIORS");
		return model ? model : ComponentFields(fields, "MODEL");
	}

	bool IsModel(const json& fields) {
		if (fields.value("lot", 0) == BrickByBrick::MODEL_OBJECT_LOT) return true;
		return ModelFields(fields) && !ComponentFields(fields, "PET");
	}

	// A config entry's value ("key=type:value")
	std::string ConfigValue(const json& fields, const std::string& key) {
		const auto found = fields.find("config");
		if (found == fields.end()) return {};
		// Compressed config shows its inflated entries under "entries"
		const json* config = &*found;
		if (config->is_object()) {
			const auto entries = config->find("entries");
			if (entries == config->end()) return {};
			config = &*entries;
		}
		if (!config->is_array()) return {};
		for (const auto& entry : *config) {
			if (!entry.is_string()) continue;
			const auto& text = entry.get_ref<const std::string&>();
			if (!text.starts_with(key + "=")) continue;
			const auto colon = text.find(':', key.size() + 1);
			return colon == std::string::npos ? std::string{} : text.substr(colon + 1);
		}
		return {};
	}

	LWOOBJID ObjectOf(const json& fields, const char* key) {
		const auto it = fields.find(key);
		if (it == fields.end()) return LWOOBJID_EMPTY;
		if (it->is_string()) return GeneralUtils::TryParse<LWOOBJID>(it->get<std::string>()).value_or(LWOOBJID_EMPTY);
		if (it->is_number_integer()) return it->get<LWOOBJID>();
		return LWOOBJID_EMPTY;
	}

	const char* DeleteReason(int32_t reason) {
		switch (static_cast<BrickByBrick::eDeleteReason>(reason)) {
		case BrickByBrick::eDeleteReason::PICKING_MODEL_UP: return "picked up";
		case BrickByBrick::eDeleteReason::RETURNING_MODEL_TO_INVENTORY: return "put away";
		case BrickByBrick::eDeleteReason::BREAKING_MODEL_APART: return "taken apart";
		default: return "removed";
		}
	}

	// DownloadPropertyData's moderation status, as the client reads it
	const char* Moderation(uint32_t status) {
		using Data = GameMessages::DownloadPropertyData;
		switch (status) {
		case Data::REJECTION_STATUS_APPROVED: return "approved";
		case Data::REJECTION_STATUS_PENDING: return "pending";
		case Data::REJECTION_STATUS_REJECTED: return "rejected";
		default: return nullptr;
		}
	}

	json SpanJson(const Span& span) {
		return { {"from", span.from}, {"to", span.to ? json(*span.to) : json(nullptr)}, {"i", span.index}, {"position", span.position}, {"rotation", span.rotation} };
	}

	json EventJson(const char* kind, double t, size_t index, const Model* model) {
		json out{ {"kind", kind}, {"t", t}, {"i", index} };
		if (model) {
			out["object"] = std::to_string(model->object);
			out["lot"] = model->lot;
		}
		return out;
	}

	// A placement the server confirmed, with the model it made
	json PlacedJson(const Placement& placement, Model& model) {
		model.placed = true;
		auto event = EventJson("placed", placement.t, placement.index, &model);
		event["position"] = placement.position;
		event["rotation"] = placement.rotation;
		event["made"] = model.spans.front().index;
		return event;
	}

	void Close(Model& model, double t) {
		if (!model.spans.empty() && !model.spans.back().to) model.spans.back().to = t;
		model.present = false;
	}
}

namespace CaptureProperty {
	json Build(const std::vector<Record>& records, int64_t startUs, const ReplicaFields& replica) {
		std::map<std::tuple<uint32_t, uint32_t, uint32_t>, World> worlds;
		const auto worldOf = [&](const PacketRecordHeader& h, double t) -> World& {
			auto [it, added] = worlds.try_emplace({ h.zoneId, h.instanceId, h.cloneId });
			if (added) {
				it->second.zone = h.zoneId;
				it->second.instance = h.instanceId;
				it->second.clone = h.cloneId;
				it->second.t = t;
			}
			return it->second;
		};

		for (size_t i = 0; i < records.size(); i++) {
			const auto& record = records[i];
			const auto& h = record.header;
			if (h.flags & PacketRecordFlags::GAP || h.source != static_cast<uint8_t>(eCaptureSource::WORLD) || record.bytes.empty()) continue;
			const double t = static_cast<double>(h.timeUs - startUs) / 1e6;
			const bool fromClient = CaptureTools::FromClient(h);

			// Replica packets: the models themselves
			const auto id = static_cast<uint8_t>(record.bytes[0]);
			if (id == ID_REPLICA_MANAGER_CONSTRUCTION || id == ID_REPLICA_MANAGER_SERIALIZE || id == ID_REPLICA_MANAGER_DESTRUCTION) {
				const auto* fields = replica ? replica(i) : nullptr;
				if (!fields || fromClient) continue;
				const auto object = ObjectOf(*fields, "objectID");
				if (!object) continue;
				if (id == ID_REPLICA_MANAGER_CONSTRUCTION) {
					if (!IsModel(*fields)) continue;
					auto& world = worldOf(h, t);
					const auto placed = Placed(*fields);
					const auto known = world.byObject.find(object);
					// Made again for another captured player on the same world: the same model
					if (known != world.byObject.end() && world.models[known->second].present) {
						auto& model = world.models[known->second];
						if (placed && !model.spans.empty() && model.spans.back().position == placed->first) continue;
						Close(model, t);
					}
					Model model;
					model.object = object;
					model.lot = fields->value("lot", 0);
					if (const auto spawner = fields->find("spawner"); spawner != fields->end() && spawner->is_string()) model.spawner = spawner->get<std::string>();
					model.ugcId = ConfigValue(*fields, "blueprintid");
					if (const auto* component = ModelFields(*fields); component && component->contains("behaviors")) model.behaviors = (*component)["behaviors"];
					model.present = true;
					if (placed) model.spans.push_back({ t, std::nullopt, i, placed->first, placed->second });
					world.byObject[object] = world.models.size();
					world.models.push_back(std::move(model));
					// The model a placement made (a live server answered first): the first one made where it said
					if (placed) {
						for (auto it = world.placements.begin(); it != world.placements.end(); ++it) {
							if (it->position != placed->first) continue;
							world.events.push_back(PlacedJson(*it, world.models.back()));
							world.placements.erase(it);
							break;
						}
					}
					continue;
				}
				const auto found = worlds.find({ h.zoneId, h.instanceId, h.cloneId });
				if (found == worlds.end()) continue;
				auto& world = found->second;
				const auto known = world.byObject.find(object);
				if (known == world.byObject.end() || !world.models[known->second].present) continue;
				auto& model = world.models[known->second];
				if (id == ID_REPLICA_MANAGER_DESTRUCTION) {
					Close(model, t);
					auto event = EventJson("removed", t, i, &model);
					const auto asked = world.deletes.find(object);
					if (asked != world.deletes.end()) {
						event["reason"] = DeleteReason(asked->second.second);
						event["asked"] = asked->second.first;
						world.deletes.erase(asked);
					}
					world.events.push_back(std::move(event));
					continue;
				}
				// Serialization: behaviors, and a new place
				if (const auto* component = ModelFields(*fields); component && component->contains("behaviors")) model.behaviors = (*component)["behaviors"];
				const auto placed = Placed(*fields);
				if (!placed || (!model.spans.empty() && model.spans.back().position == placed->first && model.spans.back().rotation == placed->second)) continue;
				if (!model.spans.empty()) model.spans.back().to = t;
				model.spans.push_back({ t, std::nullopt, i, placed->first, placed->second });
				auto event = EventJson("moved", t, i, &model);
				event["position"] = placed->first;
				world.events.push_back(std::move(event));
				continue;
			}

			// Game messages about the property
			const auto decoded = PacketDecoder::Decode(record.bytes, fromClient);
			if (decoded.gameMessageId < 0 || !decoded.name.starts_with(GAME_MSG)) continue;
			const auto name = decoded.name.substr(std::char_traits<char>::length(GAME_MSG));
			const json empty = json::object();
			const auto& fields = decoded.fields ? *decoded.fields : empty;
			const bool behavior = name.find("BEHAVIOR") != std::string::npos;
			// A live server also sent the data of the player's properties elsewhere (another map): only this world's
			// is about it (any, when the capture doesn't know the world's zone)
			const bool otherProperty = name == "DOWNLOAD_PROPERTY_DATA" && h.zoneId && fields.value("mapId", 0) != h.zoneId;
			const bool propertyMessage = !otherProperty && (name == "DOWNLOAD_PROPERTY_DATA" || name == "GET_MODELS_ON_PROPERTY" ||
				name == "PLACE_MODEL_RESPONSE" || name == "DELETE_MODEL_FROM_CLIENT" || name == "PROPERTY_EDITOR_BEGIN" || name == "PROPERTY_EDITOR_END");
			const auto existing = worlds.find({ h.zoneId, h.instanceId, h.cloneId });
			if (!propertyMessage && !behavior && existing == worlds.end()) continue;
			auto& world = existing != worlds.end() ? existing->second : worldOf(h, t);
			if (propertyMessage) world.property = true;

			if (name == "DOWNLOAD_PROPERTY_DATA" && !fromClient && decoded.fields && !otherProperty) {
				json info{ {"t", t}, {"i", i} };
				for (const char* key : { "propertyId", "name", "description", "ownerName", "ownerId", "accessType", "moderationStatus",
					"rejectionReason", "reputation", "templateId", "mapId", "cloneId", "maxBuildHeight" }) {
					if (fields.contains(key)) info[key] = fields[key];
				}
				if (const auto* moderation = Moderation(fields.value("moderationStatus", UINT32_MAX))) info["moderation"] = moderation;
				// Sent again (to each player, on each edit): kept once while it says the same
				auto said = info;
				said.erase("t");
				said.erase("i");
				if (!world.info.empty()) {
					auto before = world.info.back();
					before.erase("t");
					before.erase("i");
					if (before == said) continue;
				}
				world.info.push_back(std::move(info));
			} else if (name == "GET_MODELS_ON_PROPERTY" && !fromClient && fields.contains("models") && fields["models"].is_array()) {
				// Each captured player on the world gets one: count it once while it says the same
				const auto count = fields["models"].size();
				if (world.counts.empty() || world.counts.back()["count"] != count) world.counts.push_back({ {"t", t}, {"i", i}, {"count", count} });
			} else if (name == "PLACE_MODEL_RESPONSE" && !fromClient && fields.value("response", 0) == BrickByBrick::PLACE_MODEL_PLACED) {
				Placement placement{ t, i, fields.value("position", json::array()), fields.value("rotation", json::array()) };
				// The model it made: this server made it before answering (the latest one made there not placed yet), or
				// it comes after
				auto made = std::find_if(world.models.rbegin(), world.models.rend(), [&placement](const Model& model) {
					return model.present && !model.placed && !model.spans.empty() && model.spans.front().position == placement.position;
				});
				if (made != world.models.rend()) world.events.push_back(PlacedJson(placement, *made));
				else world.placements.push_back(std::move(placement));
			} else if (name == "DELETE_MODEL_FROM_CLIENT" && fromClient && decoded.fields) {
				const auto model = ObjectOf(fields, "modelID");
				if (model) world.deletes[model] = { i, fields.value("reason", 0) };
			} else if (name == "PROPERTY_EDITOR_BEGIN" || name == "PROPERTY_EDITOR_END") {
				world.events.push_back(EventJson(name == "PROPERTY_EDITOR_BEGIN" ? "editing" : "stopped editing", t, i, nullptr));
			}

			// Behaviors: the behavior messages, and what was sent to or from a model while it stood there
			const Model* model = nullptr;
			auto target = decoded.objectId;
			if (name == "CONTROL_BEHAVIORS" && fields.contains("args") && fields["args"].is_object()) {
				const auto fromArgs = ObjectOf(fields["args"], "objectID");
				if (fromArgs) target = fromArgs;
			}
			if (const auto it = world.byObject.find(target); it != world.byObject.end() && world.models[it->second].present) model = &world.models[it->second];
			if (!behavior && !model) continue;
			auto event = EventJson("behavior", t, i, model);
			event["message"] = name;
			event["toServer"] = fromClient;
			if (name == "CONTROL_BEHAVIORS" && fields.contains("command")) event["command"] = fields["command"];
			world.events.push_back(std::move(event));
		}

		json out = json::array();
		for (auto& [key, world] : worlds) {
			if (!world.property && world.models.empty()) continue;
			json models = json::array();
			for (const auto& model : world.models) {
				json spans = json::array();
				for (const auto& span : model.spans) spans.push_back(SpanJson(span));
				models.push_back({ {"object", std::to_string(model.object)}, {"lot", model.lot}, {"spawner", model.spawner},
					{"ugcId", model.ugcId}, {"behaviors", model.behaviors}, {"spans", spans} });
			}
			// Placements whose model was never seen made (made before the capture could read it, or refused later)
			for (const auto& placement : world.placements) {
				auto event = EventJson("placed", placement.t, placement.index, nullptr);
				event["position"] = placement.position;
				event["rotation"] = placement.rotation;
				world.events.push_back(std::move(event));
			}
			std::stable_sort(world.events.begin(), world.events.end(), [](const json& a, const json& b) { return a["i"].get<size_t>() < b["i"].get<size_t>(); });
			out.push_back({ {"zone", world.zone}, {"instance", world.instance}, {"clone", world.clone}, {"t", world.t}, {"property", world.property},
				{"info", world.info}, {"counts", world.counts}, {"models", models}, {"events", world.events} });
		}
		return { {"worlds", out} };
	}
}
