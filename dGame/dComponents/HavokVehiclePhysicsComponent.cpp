#include "HavokVehiclePhysicsComponent.h"
#include "EntityManager.h"
#include "Amf3.h"

HavokVehiclePhysicsComponent::HavokVehiclePhysicsComponent(Entity* parent, const int32_t componentID) : PhysicsComponent(parent, componentID) {
	RegisterMsg(MessageType::Game::GET_OBJECT_REPORT_INFO, this, &HavokVehiclePhysicsComponent::OnGetObjectReportInfo);

	m_Velocity = NiPoint3Constant::ZERO;
	m_AngularVelocity = NiPoint3Constant::ZERO;
	m_IsOnGround = true;
	m_IsOnRail = false;
	m_DirtyPosition = true;
	m_EndBehavior = GeneralUtils::GenerateRandomNumber<uint32_t>(0, 7);
}

void HavokVehiclePhysicsComponent::SetVelocity(const NiPoint3& vel) {
	if (vel == m_Velocity) return;
	m_DirtyPosition = true;
	m_Velocity = vel;
}

void HavokVehiclePhysicsComponent::SetAngularVelocity(const NiPoint3& vel) {
	if (vel == m_AngularVelocity) return;
	m_DirtyPosition = true;
	m_AngularVelocity = vel;
}

void HavokVehiclePhysicsComponent::SetIsOnGround(bool val) {
	if (val == m_IsOnGround) return;
	m_DirtyPosition = true;
	m_IsOnGround = val;
}

void HavokVehiclePhysicsComponent::SetIsOnRail(bool val) {
	if (val == m_IsOnRail) return;
	m_DirtyPosition = true;
	m_IsOnRail = val;
}

void HavokVehiclePhysicsComponent::SetRemoteInputInfo(const RemoteInputInfo& remoteInputInfo) {
	if (remoteInputInfo == m_RemoteInputInfo) return;
	this->m_RemoteInputInfo = remoteInputInfo;
	m_DirtyPosition = true;
}

void HavokVehiclePhysicsComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	outBitStream.Write(bIsInitialUpdate || m_DirtyPosition);

	if (bIsInitialUpdate || m_DirtyPosition) {
		m_DirtyPosition = false;
		outBitStream.Write(m_Position.x);
		outBitStream.Write(m_Position.y);
		outBitStream.Write(m_Position.z);

		outBitStream.Write(m_Rotation.x);
		outBitStream.Write(m_Rotation.y);
		outBitStream.Write(m_Rotation.z);
		outBitStream.Write(m_Rotation.w);

		outBitStream.Write(m_IsOnGround);
		outBitStream.Write(m_IsOnRail);

		bool isNotZero = m_Velocity != NiPoint3Constant::ZERO;
		outBitStream.Write(isNotZero);
		if (isNotZero) {
			outBitStream.Write(m_Velocity.x);
			outBitStream.Write(m_Velocity.y);
			outBitStream.Write(m_Velocity.z);
		}

		isNotZero = m_AngularVelocity != NiPoint3Constant::ZERO;
		outBitStream.Write(isNotZero);
		if (isNotZero) {
			outBitStream.Write(m_AngularVelocity.x);
			outBitStream.Write(m_AngularVelocity.y);
			outBitStream.Write(m_AngularVelocity.z);
		}

		outBitStream.Write0(); // local_space_info. TODO: Implement this

		// This structure only has this bool flag set to false if a ptr to the peVehicle is null, which we don't have
		// therefore, this will always be 1, even if all the values in the structure are 0.
		outBitStream.Write1(); // has remote_input_info
		outBitStream.Write(m_RemoteInputInfo.m_RemoteInputX);
		outBitStream.Write(m_RemoteInputInfo.m_RemoteInputY);
		outBitStream.Write(m_RemoteInputInfo.m_IsPowersliding);
		outBitStream.Write(m_RemoteInputInfo.m_IsModified);


		outBitStream.Write(125.0f); // remote_input_ping TODO: Figure out how this should be calculated as it seems to be constant through the whole race.

		if (!bIsInitialUpdate) {
			outBitStream.Write0();
		}
	}

	if (bIsInitialUpdate) {
		outBitStream.Write<uint8_t>(m_EndBehavior);
		outBitStream.Write1(); // is input locked?
	}

	outBitStream.Write0();
}

bool HavokVehiclePhysicsComponent::OnGetObjectReportInfo(GameMessages::GameMsg& msg) {
	PhysicsComponent::OnGetObjectReportInfo(msg);
	auto& reportInfo = static_cast<GameMessages::GetObjectReportInfo&>(msg);
	if (!reportInfo.subCategory) {
		return false;
	}

	auto& info = reportInfo.subCategory->PushDebug("Havok Vehicle Physics Info");

	auto& velocity = info.PushDebug("Velocity");
	velocity.PushDebug<AMFDoubleValue>("x") = m_Velocity.x;
	velocity.PushDebug<AMFDoubleValue>("y") = m_Velocity.y;
	velocity.PushDebug<AMFDoubleValue>("z") = m_Velocity.z;

	auto& angularVelocity = info.PushDebug("Angular Velocity");
	angularVelocity.PushDebug<AMFDoubleValue>("x") = m_AngularVelocity.x;
	angularVelocity.PushDebug<AMFDoubleValue>("y") = m_AngularVelocity.y;
	angularVelocity.PushDebug<AMFDoubleValue>("z") = m_AngularVelocity.z;

	info.PushDebug<AMFBoolValue>("Is On Ground") = m_IsOnGround;
	info.PushDebug<AMFBoolValue>("Is On Rail") = m_IsOnRail;
	info.PushDebug<AMFIntValue>("End Behavior") = m_EndBehavior;

	auto& remoteInputInfo = info.PushDebug("Remote Input Info");
	remoteInputInfo.PushDebug<AMFDoubleValue>("Remote Input X") = m_RemoteInputInfo.m_RemoteInputX;
	remoteInputInfo.PushDebug<AMFDoubleValue>("Remote Input Y") = m_RemoteInputInfo.m_RemoteInputY;
	remoteInputInfo.PushDebug<AMFBoolValue>("Is Powersliding") = m_RemoteInputInfo.m_IsPowersliding;
	remoteInputInfo.PushDebug<AMFBoolValue>("Is Modified") = m_RemoteInputInfo.m_IsModified;
	return true;
}
