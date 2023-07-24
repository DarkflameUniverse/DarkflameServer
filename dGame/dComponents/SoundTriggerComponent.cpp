#include "SoundTriggerComponent.h"
#include "EntityManager.h"
#include "Game.h"

SoundTriggerComponent::SoundTriggerComponent(Entity* parent) : Component(parent) {
	const auto musicCueName = parent->GetVar<std::string>(u"NDAudioMusicCue_Name");
	const auto musicCueBoredomTime = parent->GetVar<float>(u"NDAudioMusicCue_BoredomTime");

	this->m_MusicCues.push_back({
			musicCueName,
			1,
			musicCueBoredomTime
	});

	const auto musicParemeterName = parent->GetVar<std::string>(u"NDAudioMusicParameter_Name");
	const auto musicParemeterValue = parent->GetVar<float>(u"NDAudioMusicParameter_Value");
	this->m_MusicParameters.push_back({
		musicParemeterName,
		musicParemeterValue
	});

	const auto guidString = parent->GetVar<std::string>(u"NDAudioEventGUID");
	if (!guidString.empty()) {
		this->m_GUID.emplace_back(guidString);
	}

	const auto guid2String = parent->GetVar<std::string>(u"NDAudioEventGUID2");
	if (!guid2String.empty()) {
		this->m_GUID2.emplace_back(guid2String);
	}

	const auto mixerName = parent->GetVar<std::string>(u"NDAudioMixerProgram_Name");
	this->m_MixerPrograms.push_back(mixerName);


}

SoundTriggerComponent::~SoundTriggerComponent() = default;

void SoundTriggerComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	if (bIsInitialUpdate)
		this->m_Dirty = true;

	outBitStream->Write(this->m_Dirty);

	if (this->m_Dirty) {
		outBitStream->Write<uint8_t>(this->m_MusicCues.size());
		for (const auto& musicCue : this->m_MusicCues) {
			outBitStream->Write<uint8_t>(musicCue.name.size());
			outBitStream->Write(musicCue.name.c_str(), musicCue.name.size());
			outBitStream->Write<uint32_t>(musicCue.result);
			outBitStream->Write<float_t>(musicCue.boredomTime);
		}

		outBitStream->Write<uint8_t>(this->m_MusicParameters.size());
		for (const auto& musicParam : this->m_MusicParameters) {
			outBitStream->Write<uint8_t>(musicParam.name.size());
			outBitStream->Write(musicParam.name.c_str(), musicParam.name.size());
			outBitStream->Write<float_t>(musicParam.value);
		}

		outBitStream->Write<uint8_t>(this->m_GUID.size());
		for (const auto guid : this->m_GUID) {
			outBitStream->Write<uint32_t>(guid.GetData1());
			outBitStream->Write<uint16_t>(guid.GetData2());
			outBitStream->Write<uint16_t>(guid.GetData3());
			for (const auto& guidSubPart : guid.GetData4()) {
				outBitStream->Write<uint8_t>(guidSubPart);
			}
			outBitStream->Write<uint32_t>(1); // Unknown
		}

		outBitStream->Write<uint8_t>(this->m_GUID2.size());
		for (const auto guid : this->m_GUID2) {
			outBitStream->Write<uint32_t>(guid.GetData1());
			outBitStream->Write<uint16_t>(guid.GetData2());
			outBitStream->Write<uint16_t>(guid.GetData3());
			for (const auto& guidSubPart : guid.GetData4()) {
				outBitStream->Write<uint8_t>(guidSubPart);
			}
			outBitStream->Write<uint32_t>(1); // Unknown
		}

		// Mixer program part
		outBitStream->Write<uint8_t>(this->m_MixerPrograms.size());
		for (const auto& mixerProgram : this->m_MixerPrograms) {
			outBitStream->Write<uint8_t>(mixerProgram.size());
			outBitStream->Write(mixerProgram.c_str(), mixerProgram.size());
			outBitStream->Write<uint32_t>(1); // Unknown
		}

		this->m_Dirty = false;
	}
}

void SoundTriggerComponent::ActivateMusicCue(const std::string& name) {
	if (std::find_if(this->m_MusicCues.begin(), this->m_MusicCues.end(), [name](const MusicCue& musicCue) {
		return  musicCue.name == name;
		}) == this->m_MusicCues.end()) {
		this->m_MusicCues.push_back({
			name,
			1,
			-1.0f
			});
		this->m_Dirty = true;
		Game::entityManager->SerializeEntity(m_Parent);
	}
}

void SoundTriggerComponent::DeactivateMusicCue(const std::string& name) {
	const auto musicCue = std::find_if(this->m_MusicCues.begin(), this->m_MusicCues.end(), [name](const MusicCue& musicCue) {
		return  musicCue.name == name;
		});

	if (musicCue != this->m_MusicCues.end()) {
		this->m_MusicCues.erase(musicCue);
		this->m_Dirty = true;
		Game::entityManager->SerializeEntity(m_Parent);
	}
}
