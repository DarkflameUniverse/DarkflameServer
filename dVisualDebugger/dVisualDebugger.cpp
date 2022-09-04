#include "dVisualDebugger.h"

#include <iostream>

#include "raylib.h"

#include "Game.h"
#include "dpWorld.h"
#include "dpEntity.h"
#include "dpShapeBox.h"
#include "dpShapeSphere.h"
#include "EntityManager.h"
#include "PerformanceManager.h"
#include "dTerrain/RawFile.h"
#include "dTerrain/RawMesh.h"
#include "SimplePhysicsComponent.h"
#include "dZoneManager.h"

inline Vector3 NiPointToVector3(NiPoint3 pos) { return { pos.x, pos.y, pos.z }; };

dVisualDebugger::dVisualDebugger(std::string zoneName) {
	SetTraceLogLevel(LOG_FATAL);
	InitWindow(m_Width, m_Height, ("dVisualDebugger: " + zoneName).c_str());

	CreateCamera();
	CreateInGameCamera();

	std::string zonePath = dZoneManager::Instance()->GetZone()->GetFilePathForZoneID();

	zonePath = zonePath.substr(0, zonePath.rfind('/') + 1);
	zonePath += dZoneManager::Instance()->GetZone()->GetZoneRawPath();

	m_Terrain = new RawFile(zonePath);
}

dVisualDebugger::~dVisualDebugger() {
	if (m_Camera) delete m_Camera;
	if (m_Terrain) delete m_Terrain;
}

void dVisualDebugger::CreateInGameCamera() {
	EntityInfo info;
	info.lot = m_CameraID;
	info.pos = { m_Camera->position.x, m_Camera->position.y, m_Camera->position.z };
	info.rot = NiQuaternion::LookAt(info.pos, { m_Camera->target.x, m_Camera->target.y, m_Camera->target.z });
	info.spawner = nullptr;
	info.spawnerID = 0;
	info.spawnerNodeID = 0;

	Entity* newEntity = EntityManager::Instance()->CreateEntity(info, nullptr);
	m_CameraObjid = newEntity->GetObjectID();
	EntityManager::Instance()->ConstructEntity(newEntity);
}

void dVisualDebugger::CreateCamera() {
	Camera3D* camera = new Camera3D();

	camera->position = { 0.f, 0.f, 0.f };
	camera->target = { 0.f, 1.f, 0.f };
	camera->up = { 0.f, 1.f, 0.f };
	camera->fovy = 40.0f;
	camera->projection = CAMERA_PERSPECTIVE;

	m_Camera = camera;

	SetCameraMode(*m_Camera, CAMERA_FIRST_PERSON);

	float frameRateAprox = 1000 / PerformanceManager::GetServerFramerate();

	SetTargetFPS((int32_t)frameRateAprox); // truncate exact frame rate
}

void dVisualDebugger::Step(float delta) {
	if (WindowShouldClose()) {
		CloseWindow();
		exit(0); // Kill entire server, this is a dev tool at the end of the day
	}

	UpdateCamera(m_Camera);
	this->AttachToCharacter();

	if (IsKeyDown(KEY_SPACE)) m_BindToGM = !m_BindToGM;
	delete m_Camera;

	BeginDrawing();
		ClearBackground(RAYWHITE);
		BeginMode3D(*m_Camera);
			this->RenderEntities(Game::physicsWorld->GetDynamicEntities(), true);
			this->RenderEntities(Game::physicsWorld->GetStaticEntities(), false);
			this->RenderTerrainMesh();
		EndMode3D();

		DrawText((std::to_string(m_Camera->position.x) + " " + std::to_string(m_Camera->position.y) + " " + std::to_string(m_Camera->position.z)).c_str(), 20, 20, 10, BLACK);
		DrawFPS(10, 10);
	EndDrawing();
}

void dVisualDebugger::AttachToCharacter() {
	if (!m_BindToGM) return;

	auto characters = EntityManager::Instance()->GetEntitiesByLOT(1);
	if (characters.size() != 1) return;

	m_Camera->position = NiPointToVector3(characters[0]->GetPosition());
	m_Camera->target = NiPointToVector3(characters[0]->GetRotation().GetForwardVector());

	// Reposition our camera

	auto* camera = EntityManager::Instance()->GetEntity(m_CameraObjid);
	if (!camera) return;
	auto* physComp = camera->GetComponent<SimplePhysicsComponent>();
	physComp->SetPosition(characters[0]->GetPosition());
	physComp->SetRotation(characters[0]->GetRotation());
	EntityManager::Instance()->SerializeEntity(camera);
}

void dVisualDebugger::RenderTerrainMesh() {
	if (!m_Terrain->GetMesh()) return;

	for (int i = 0; i < m_Terrain->GetMesh()->m_Triangles.size(); i += 3) {
		auto v1 =  m_Terrain->GetMesh()->m_Vertices[m_Terrain->GetMesh()->m_Triangles[i]];
		auto v2 =  m_Terrain->GetMesh()->m_Vertices[m_Terrain->GetMesh()->m_Triangles[i + 1]];
		auto v3 =  m_Terrain->GetMesh()->m_Vertices[m_Terrain->GetMesh()->m_Triangles[i + 2]];
		DrawTriangle3D(NiPointToVector3(v1), NiPointToVector3(v2), NiPointToVector3(v3), GRAY);
	}
}

void dVisualDebugger::RenderEntities(std::vector<dpEntity*>* entities, bool dynamic) {
	auto colour = dynamic ? RED : BLUE;

	for (auto* item : *entities) {
		auto* shape = item->GetShape();

		switch (shape->GetShapeType()) {
			case dpShapeType::Box: {
				auto box = static_cast<dpShapeBox*>(shape);
				DrawCube(NiPointToVector3(item->GetPosition()), box->GetWidth(), box->GetHeight(), box->GetDepth(), colour);
				break;
			};
			case dpShapeType::Sphere: {
				auto sphere = static_cast<dpShapeSphere*>(shape);
				DrawSphere(NiPointToVector3(item->GetPosition()), sphere->GetRadius(), colour);
				break;
			};
			default: {
				break; // We can ignore this
			}
		};
	}
}
