#ifndef __GHOSTCOMPONENT__H__
#define __GHOSTCOMPONENT__H__

#include "Component.h"
#include "eReplicaComponentType.h"
#include <unordered_set>

class NiPoint3;

class GhostComponent final : public Component {
public:
	static inline const eReplicaComponentType ComponentType = eReplicaComponentType::GHOST;
	GhostComponent(Entity* parent);
	~GhostComponent() override;

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

	void ObserveEntity(const int32_t id);

	bool IsObserved(const int32_t id);

	void GhostEntity(const int32_t id);

private:
	NiPoint3 m_GhostReferencePoint;

	NiPoint3 m_GhostOverridePoint;

	std::unordered_set<int32_t> m_ObservedEntities;

	std::unordered_set<LWOOBJID> m_LimboConstructions;

	bool m_GhostOverride;
};

#endif  //!__GHOSTCOMPONENT__H__
