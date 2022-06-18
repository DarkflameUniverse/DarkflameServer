#pragma once

#include "../GameMessage.h"

#include "MissionComponent.h"
#include "DestroyableComponent.h"
#include "SkillComponent.h"
#include "PacketUtils.h"

class StartSkill : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_START_SKILL; }

	bool bUsedMouse = false;
	LWOOBJID consumableItemID{};
	float fCasterLatency{};
	int iCastType{};
	NiPoint3 lastClickedPosit{};
	LWOOBJID optionalOriginatorID{};
	LWOOBJID optionalTargetID{};
	NiQuaternion originatorRot{};
	std::string sBitStream = "";
	TSkillID skillID = 0;
	unsigned int uiSkillHandle = 0;

	void Serialize(RakNet::BitStream* outStream) override {
		outStream->Write(bUsedMouse);

		outStream->Write(consumableItemID != LWOOBJID_EMPTY);
		if (consumableItemID != LWOOBJID_EMPTY) outStream->Write(consumableItemID);

		outStream->Write(fCasterLatency != 0.0f);
		if (fCasterLatency != 0.0f) outStream->Write(fCasterLatency);

		outStream->Write(iCastType != 0);
		if (iCastType != 0) outStream->Write(iCastType);

		outStream->Write(lastClickedPosit != NiPoint3::ZERO);
		if (lastClickedPosit != NiPoint3::ZERO) outStream->Write(lastClickedPosit);

		outStream->Write(optionalOriginatorID);

		outStream->Write(optionalTargetID != LWOOBJID_EMPTY);
		if (optionalTargetID != LWOOBJID_EMPTY) outStream->Write(optionalTargetID);

		outStream->Write(originatorRot != NiQuaternion::IDENTITY);
		if (originatorRot != NiQuaternion::IDENTITY) outStream->Write(originatorRot);

		uint32_t sBitStreamLength = sBitStream.length();
		outStream->Write(sBitStreamLength);
		for (unsigned int k = 0; k < sBitStreamLength; k++) {
			outStream->Write(sBitStream[k]);
		}

		outStream->Write(skillID);

		outStream->Write(uiSkillHandle != 0);
		if (uiSkillHandle != 0) outStream->Write(uiSkillHandle);
	}

	void Deserialize(RakNet::BitStream* inStream) override {
		inStream->Read(bUsedMouse);

		bool consumableItemIDIsDefault{};
		inStream->Read(consumableItemIDIsDefault);
		if (consumableItemIDIsDefault != 0) inStream->Read(consumableItemID);

		bool fCasterLatencyIsDefault{};
		inStream->Read(fCasterLatencyIsDefault);
		if (fCasterLatencyIsDefault != 0) inStream->Read(fCasterLatency);

		bool iCastTypeIsDefault{};
		inStream->Read(iCastTypeIsDefault);
		if (iCastTypeIsDefault != 0) inStream->Read(iCastType);

		bool lastClickedPositIsDefault{};
		inStream->Read(lastClickedPositIsDefault);
		if (lastClickedPositIsDefault != 0) inStream->Read(lastClickedPosit);

		inStream->Read(optionalOriginatorID);

		bool optionalTargetIDIsDefault{};
		inStream->Read(optionalTargetIDIsDefault);
		if (optionalTargetIDIsDefault != 0) inStream->Read(optionalTargetID);

		bool originatorRotIsDefault{};
		inStream->Read(originatorRotIsDefault);
		if (originatorRotIsDefault != 0) inStream->Read(originatorRot);

		uint32_t sBitStreamLength{};
		inStream->Read(sBitStreamLength);
		for (unsigned int k = 0; k < sBitStreamLength; k++) {
			unsigned char character;
			inStream->Read(character);
			sBitStream.push_back(character);
		}

		inStream->Read(skillID);

		bool uiSkillHandleIsDefault{};
		inStream->Read(uiSkillHandleIsDefault);
		if (uiSkillHandleIsDefault != 0) inStream->Read(uiSkillHandle);
	}

	void Handle() override {
		if (skillID == 1561 || skillID == 1562 || skillID == 1541) return;

		MissionComponent* comp = associate->GetComponent<MissionComponent>();
		if (comp) {
			comp->Progress(MissionTaskType::MISSION_TASK_TYPE_SKILL, skillID);
		}

		CDSkillBehaviorTable* skillTable = CDClientManager::Instance()->GetTable<CDSkillBehaviorTable>("SkillBehavior");
		unsigned int behaviorId = skillTable->GetSkillByID(skillID).behaviorID;

		bool success = false;

		if (behaviorId > 0) {
			RakNet::BitStream* bs = new RakNet::BitStream((unsigned char*)sBitStream.c_str(), sBitStream.size(), false);

			auto* skillComponent = associate->GetComponent<SkillComponent>();

			success = skillComponent->CastPlayerSkill(behaviorId, uiSkillHandle, bs, optionalTargetID, skillID);

			if (success && associate->GetCharacter()) {
				DestroyableComponent* destComp = associate->GetComponent<DestroyableComponent>();
				destComp->SetImagination(destComp->GetImagination() - skillTable->GetSkillByID(skillID).imaginationcost);
			}

			delete bs;
		}

		if (Game::server->GetZoneID() == 1302) {
			return;
		}

		if (success) {
			//Broadcast our startSkill:
			RakNet::BitStream bitStreamLocal;
			PacketUtils::WriteHeader(bitStreamLocal, CLIENT, MSG_CLIENT_GAME_MSG);
			bitStreamLocal.Write(associate->GetObjectID());

			GameMessages::EchoStartSkill echoStartSkill;
			echoStartSkill.bUsedMouse = bUsedMouse;
			echoStartSkill.fCasterLatency = fCasterLatency;
			echoStartSkill.iCastType = iCastType;
			echoStartSkill.lastClickedPosit = lastClickedPosit;
			echoStartSkill.optionalOriginatorID = optionalOriginatorID;
			echoStartSkill.optionalTargetID = optionalTargetID;
			echoStartSkill.originatorRot = originatorRot;
			echoStartSkill.sBitStream = sBitStream;
			echoStartSkill.skillID = skillID;
			echoStartSkill.uiSkillHandle = uiSkillHandle;
			echoStartSkill.Serialize(&bitStreamLocal);

			Game::server->Send(&bitStreamLocal, associate->GetSystemAddress(), true);
		}
	}
};