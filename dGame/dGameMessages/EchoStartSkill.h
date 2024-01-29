#ifndef __ECHOSTARTSKILL__H__
#define __ECHOSTARTSKILL__H__

#include "dCommonVars.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "eGameMessageType.h"

/*  Same as start skill but with different network options. An echo down to other clients that need to play the skill. */
class EchoStartSkill {
public:
	EchoStartSkill() {
		bUsedMouse = false;
		fCasterLatency = 0.0f;
		iCastType = 0;
		lastClickedPosit = NiPoint3Constant::ZERO;
		optionalTargetID = LWOOBJID_EMPTY;
		originatorRot = NiQuaternionConstant::IDENTITY;
		uiSkillHandle = 0;
	}

	EchoStartSkill(LWOOBJID _optionalOriginatorID, std::string _sBitStream, TSkillID _skillID, bool _bUsedMouse = false, float _fCasterLatency = 0.0f, int32_t _iCastType = 0, NiPoint3 _lastClickedPosit = NiPoint3Constant::ZERO, LWOOBJID _optionalTargetID = LWOOBJID_EMPTY, NiQuaternion _originatorRot = NiQuaternionConstant::IDENTITY, uint32_t _uiSkillHandle = 0) {
		bUsedMouse = _bUsedMouse;
		fCasterLatency = _fCasterLatency;
		iCastType = _iCastType;
		lastClickedPosit = _lastClickedPosit;
		optionalOriginatorID = _optionalOriginatorID;
		optionalTargetID = _optionalTargetID;
		originatorRot = _originatorRot;
		sBitStream = _sBitStream;
		skillID = _skillID;
		uiSkillHandle = _uiSkillHandle;
	}

	EchoStartSkill(RakNet::BitStream* stream) : EchoStartSkill() {
		Deserialize(stream);
	}

	~EchoStartSkill() {
	}

	void Serialize(RakNet::BitStream* stream) {
		stream->Write(eGameMessageType::ECHO_START_SKILL);

		stream->Write(bUsedMouse);

		stream->Write(fCasterLatency != 0.0f);
		if (fCasterLatency != 0.0f) stream->Write(fCasterLatency);

		stream->Write(iCastType != 0);
		if (iCastType != 0) stream->Write(iCastType);

		stream->Write(lastClickedPosit != NiPoint3Constant::ZERO);
		if (lastClickedPosit != NiPoint3Constant::ZERO) stream->Write(lastClickedPosit);

		stream->Write(optionalOriginatorID);

		stream->Write(optionalTargetID != LWOOBJID_EMPTY);
		if (optionalTargetID != LWOOBJID_EMPTY) stream->Write(optionalTargetID);

		stream->Write(originatorRot != NiQuaternionConstant::IDENTITY);
		if (originatorRot != NiQuaternionConstant::IDENTITY) stream->Write(originatorRot);

		uint32_t sBitStreamLength = sBitStream.length();
		stream->Write(sBitStreamLength);
		for (uint32_t k = 0; k < sBitStreamLength; k++) {
			stream->Write(sBitStream[k]);
		}

		stream->Write(skillID);

		stream->Write(uiSkillHandle != 0);
		if (uiSkillHandle != 0) stream->Write(uiSkillHandle);
	}

	bool Deserialize(RakNet::BitStream* stream) {
		stream->Read(bUsedMouse);

		bool fCasterLatencyIsDefault{};
		stream->Read(fCasterLatencyIsDefault);
		if (fCasterLatencyIsDefault != 0) stream->Read(fCasterLatency);

		bool iCastTypeIsDefault{};
		stream->Read(iCastTypeIsDefault);
		if (iCastTypeIsDefault != 0) stream->Read(iCastType);

		bool lastClickedPositIsDefault{};
		stream->Read(lastClickedPositIsDefault);
		if (lastClickedPositIsDefault != 0) stream->Read(lastClickedPosit);

		stream->Read(optionalOriginatorID);

		bool optionalTargetIDIsDefault{};
		stream->Read(optionalTargetIDIsDefault);
		if (optionalTargetIDIsDefault != 0) stream->Read(optionalTargetID);

		bool originatorRotIsDefault{};
		stream->Read(originatorRotIsDefault);
		if (originatorRotIsDefault != 0) stream->Read(originatorRot);

		uint32_t sBitStreamLength{};
		stream->Read(sBitStreamLength);
		for (uint32_t k = 0; k < sBitStreamLength; k++) {
			unsigned char character;
			stream->Read(character);
			sBitStream.push_back(character);
		}

		stream->Read(skillID);

		bool uiSkillHandleIsDefault{};
		stream->Read(uiSkillHandleIsDefault);
		if (uiSkillHandleIsDefault != 0) stream->Read(uiSkillHandle);

		return true;
	}

	bool bUsedMouse;
	float fCasterLatency;
	int32_t iCastType;
	NiPoint3 lastClickedPosit;
	LWOOBJID optionalOriginatorID;
	LWOOBJID optionalTargetID;
	NiQuaternion originatorRot;
	std::string sBitStream;
	TSkillID skillID;
	uint32_t uiSkillHandle;
};

#endif  //!__ECHOSTARTSKILL__H__
