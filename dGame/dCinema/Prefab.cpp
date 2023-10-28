#include "Prefab.h"

#include "tinyxml2.h"

#include "../../dWorldServer/ObjectIDManager.h"
#include "EntityManager.h"
#include "EntityInfo.h"
#include "RenderComponent.h"

using namespace Cinema;

std::unordered_map<std::string, Prefab> Prefab::m_Prefabs;
std::unordered_map<size_t, Prefab::Instance> Prefab::m_Instances;

size_t Prefab::AddObject(LOT lot, NiPoint3 position, NiQuaternion rotation, float scale) {
	const auto id = ObjectIDManager::GenerateRandomObjectID();

	m_Pieces.emplace(id, Prefab::Piece { lot, position, rotation, scale, {} });

	return id;
}

void Prefab::AddEffect(size_t id, int32_t effect) {
	m_Pieces[id].m_Effects.push_back(effect);
}

void Prefab::RemoveObject(size_t id) {
	m_Pieces.erase(id);
}

const Prefab& Prefab::LoadFromFile(std::string file) {
	if (m_Prefabs.find(file) != m_Prefabs.end()) {
		return m_Prefabs[file];
	}

	Prefab prefab;

	tinyxml2::XMLDocument doc;
	doc.LoadFile(file.c_str());

	tinyxml2::XMLElement* root = doc.FirstChildElement("Prefab");
	if (!root) {
		LOG("Failed to load prefab from file: %s", file.c_str());

		m_Prefabs.emplace(file, prefab);

		return m_Prefabs[file];
	}

	for (tinyxml2::XMLElement* element = root->FirstChildElement("Object"); element; element = element->NextSiblingElement("Object")) {
		const auto lot = element->UnsignedAttribute("lot");
		const auto position = NiPoint3(element->FloatAttribute("x"), element->FloatAttribute("y"), element->FloatAttribute("z"));

		NiQuaternion rotation;
		// Check if the qx attribute exists, if so the rotation is a quaternion, otherwise it's a vector
		if (!element->Attribute("qx")) {
			rotation = NiQuaternion::FromEulerAngles( { 
				element->FloatAttribute("rx") * 0.0174532925f,
				element->FloatAttribute("ry") * 0.0174532925f,
				element->FloatAttribute("rz") * 0.0174532925f
			} );
		}
		else {
			rotation = NiQuaternion(element->FloatAttribute("qx"), element->FloatAttribute("qy"), element->FloatAttribute("qz"), element->FloatAttribute("qw"));
		}

		float scale = 1.0f;

		if (element->Attribute("scale")) {
			scale = element->FloatAttribute("scale");
		}

		const auto id = prefab.AddObject(lot, position, rotation, scale);

		for (tinyxml2::XMLElement* effect = element->FirstChildElement("Effect"); effect; effect = effect->NextSiblingElement("Effect")) {
			prefab.AddEffect(id, effect->IntAttribute("id"));
		}
	}

	m_Prefabs.emplace(file, prefab);

	return m_Prefabs[file];
}

void Prefab::SaveToFile(std::string file) {
	tinyxml2::XMLDocument doc;

	tinyxml2::XMLElement* root = doc.NewElement("Prefab");
	doc.InsertFirstChild(root);

	for (const auto& [id, piece] : m_Pieces) {
		tinyxml2::XMLElement* object = doc.NewElement("Object");
		object->SetAttribute("lot", piece.m_Lot);
		object->SetAttribute("x", piece.m_Position.x);
		object->SetAttribute("y", piece.m_Position.y);
		object->SetAttribute("z", piece.m_Position.z);
		object->SetAttribute("qx", piece.m_Rotation.x);
		object->SetAttribute("qy", piece.m_Rotation.y);
		object->SetAttribute("qz", piece.m_Rotation.z);
		object->SetAttribute("qw", piece.m_Rotation.w);
		object->SetAttribute("scale", piece.m_Scale);

		for (const auto& effect : piece.m_Effects) {
			tinyxml2::XMLElement* effectElement = doc.NewElement("Effect");
			effectElement->SetAttribute("id", effect);
			object->InsertEndChild(effectElement);
		}

		root->InsertEndChild(object);
	}

	doc.SaveFile(file.c_str());
}

size_t Prefab::Instantiate(NiPoint3 position, float scale) const {
	if (m_Pieces.empty()) {
		return 0;
	}
	
	const auto id = ObjectIDManager::GenerateRandomObjectID();

	std::vector<LWOOBJID> entities;

	for (const auto& [_, piece] : m_Pieces) {
		EntityInfo info;
		info.spawnerID = Game::entityManager->GetZoneControlEntity()->GetObjectID();
		info.lot = piece.m_Lot;
		info.pos = (piece.m_Position * scale) + position;
		info.rot = piece.m_Rotation;
		info.scale = piece.m_Scale * scale;
		
		const auto entity = Game::entityManager->CreateEntity(info);

		for (const auto& effect : piece.m_Effects) {
			auto* renderComponent = entity->GetComponent<RenderComponent>();

			if (!renderComponent) {
				continue;
			}

			// Generate random name
			std::string effectName = "Effect_";
			for (int i = 0; i < 10; ++i) {
				effectName += std::to_string(rand() % 10);
			}

			renderComponent->PlayEffect(effect, u"create", effectName);
		}

		entities.push_back(entity->GetObjectID());

		Game::entityManager->ConstructEntity(entity);
	}

	m_Instances.emplace(id, Prefab::Instance { entities });

	return id;
}

const std::vector<LWOOBJID>& Cinema::Prefab::GetEntities(size_t instanceID) {
	return m_Instances[instanceID].m_Entities;
}

void Prefab::DestroyInstance(size_t id) {
	const auto& instance = m_Instances[id];

	for (const auto& entity : instance.m_Entities) {
		Game::entityManager->DestroyEntity(entity);
	}

	m_Instances.erase(id);
}
