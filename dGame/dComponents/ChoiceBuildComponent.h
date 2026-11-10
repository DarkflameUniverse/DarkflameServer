#ifndef CHOICEBUILDCOMPONENT_H
#define CHOICEBUILDCOMPONENT_H

#include "BitStream.h"
#include "Component.h"
#include "dCommonVars.h"
#include "eReplicaComponentType.h"

/**
 * Choice build, as the client's LWOChoiceBuildComponent makes it from the registry (LOTs 5668, 6414, 6462, 7002, 16205).
 * Its state, the object built and the player using it are sent; nothing else of it is done on the server yet.
 */
class ChoiceBuildComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::CHOICE_BUILD;

	// The client's states (its constructor starts at Waiting)
	enum class eState : uint32_t {
		Waiting = 0,
		Building = 1,
	};

	ChoiceBuildComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {}

	// LWOChoiceBuildComponent::Deserialize (0x00bf26d0) reads, in constructions and updates alike: a flag, then the
	// state (u32), the built object (u64) and the user (u64). A construction always carries them.
	void Serialize(RakNet::BitStream& outBitStream, const bool isConstruction) override {
		outBitStream.Write(isConstruction || m_Dirty);
		if (isConstruction || m_Dirty) {
			outBitStream.Write(m_State);
			outBitStream.Write(m_ChildObject);
			outBitStream.Write(m_UserID);
		}
		if (!isConstruction) m_Dirty = false;
	}

	eState GetState() const { return m_State; }
	LWOOBJID GetChildObject() const { return m_ChildObject; }
	LWOOBJID GetUserID() const { return m_UserID; }

	// Changes what the clients are told with the next update (the caller serializes the object)
	void SetState(const eState state, const LWOOBJID childObject, const LWOOBJID userID) {
		m_State = state;
		m_ChildObject = childObject;
		m_UserID = userID;
		m_Dirty = true;
	}

private:
	eState m_State = eState::Waiting;

	// The object the choice build made, none while waiting
	LWOOBJID m_ChildObject = LWOOBJID_EMPTY;

	// The player using the choice build, none while waiting
	LWOOBJID m_UserID = LWOOBJID_EMPTY;

	bool m_Dirty = false;
};

#endif // CHOICEBUILDCOMPONENT_H
