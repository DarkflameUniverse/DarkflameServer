#ifndef __GHOSTCOMPONENT__H__
#define __GHOSTCOMPONENT__H__

#include "Component.h"
#include "eReplicaComponentType.h"
#include <unordered_set>

class NiPoint3;

namespace tinyxml2 {
	class XMLDocument;
}

class GhostComponent final : public Component {
public:
	static inline const eReplicaComponentType ComponentType = eReplicaComponentType::GHOST;
	GhostComponent(Entity* parent, const int32_t componentID);
	~GhostComponent() override;
	void LoadFromXml(const tinyxml2::XMLDocument& doc) override;
	void UpdateXml(tinyxml2::XMLDocument& doc) override;

	void SetGhostOverride(bool value) { m_GhostOverride = value; };

	const NiPoint3& GetGhostReferencePoint() const { return m_GhostOverride ? m_GhostOverridePoint : m_GhostReferencePoint; };

	const NiPoint3& GetOriginGhostReferencePoint() const { return m_GhostReferencePoint; };

	const NiPoint3& GetGhostOverridePoint() const { return m_GhostOverridePoint; };

	bool GetGhostOverride() const { return m_GhostOverride; };

	void SetGhostReferencePoint(const NiPoint3& value);

	void SetGhostOverridePoint(const NiPoint3& value);

	void AddLimboConstruction(const LWOOBJID objectId);

	void RemoveLimboConstruction(const LWOOBJID objectId);

	void ConstructLimboEntities();

	void ObserveEntity(const LWOOBJID id);

	bool IsObserved(const LWOOBJID id);

	void GhostEntity(const LWOOBJID id);

	bool OnToggleGMInvis(GameMessages::GameMsg& msg);

	bool OnGetGMInvis(GameMessages::GameMsg& msg);
	
	bool MsgGetObjectReportInfo(GameMessages::GameMsg& msg);

private:

	NiPoint3 m_GhostReferencePoint;

	NiPoint3 m_GhostOverridePoint;

	std::unordered_set<LWOOBJID> m_ObservedEntities;

	std::unordered_set<LWOOBJID> m_LimboConstructions;

	bool m_GhostOverride;

	bool m_IsGMInvisible{ false };
	
};

#endif  //!__GHOSTCOMPONENT__H__
