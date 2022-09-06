#pragma once

#include <cstdint>
#include <vector>
#include <string>

class Camera3D;
class dpEntity;
class RawFile;

class dVisualDebugger {
public:
	dVisualDebugger(std::string zoneName);
	~dVisualDebugger();

	void Step(float delta);
private:
	Camera3D* m_Camera;
	RawFile* m_Terrain;
	uint64_t m_CameraObjid;
	bool m_BindToGM = false;

	void CreateCamera();
	void CreateInGameCamera();

	void UpdateCameraPosition();
	void UpdateInGameCameraPosition();

	void HandleInputs();

	void RenderEntities(std::vector<dpEntity*>* entities, bool dynamic);
	void RenderTerrainMesh();

	const int32_t m_Width = 800;
	const int32_t m_Height = 450;
	const int32_t m_CameraID = 5204;
};
